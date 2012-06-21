////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Martin Schoenert
/// @author Copyright 2009-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "DispatcherThread.h"

#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Dispatcher/Dispatcher.h"
#include "Dispatcher/DispatcherQueue.h"
#include "Dispatcher/Job.h"
#include "Logger/Logger.h"

#ifdef TRI_ENABLE_ZEROMQ
#include "ZeroMQ/ApplicationZeroMQ.h"
#endif

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                            class DispatcherThread
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a dispatcher thread
////////////////////////////////////////////////////////////////////////////////

DispatcherThread::DispatcherThread (DispatcherQueue* queue)
  : Thread("dispatcher"),
    _queue(queue),
    _jobType(Job::READ_JOB) {
  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::run () {
#ifdef TRI_ENABLE_ZEROMQ
  void* zContext = _queue->_dispatcher->zeroMQContext();
  void* zBridge = 0;

  if (zContext != 0) {
    zBridge = zmq_socket(zContext, ZMQ_DEALER);

    if (zBridge == 0) {
      LOGGER_FATAL << "cannot create Disptacher/ZeroMQ bridge: " << zmq_strerror(errno);
      zmq_term(zContext);
      exit(EXIT_FAILURE);
    }

    int zRes = zmq_bind(zBridge, ApplicationZeroMQ::ZEROMQ_INTERNAL_BRIDGE.c_str());

    if (zRes != 0) {
      LOGGER_FATAL << "cannot bind Disptacher/ZeroMQ bridge: " << zmq_strerror(errno);
      zmq_close(zBridge);
      zmq_term(zContext);
      exit(EXIT_FAILURE);
    }
  }
#endif

  _queue->_accessQueue.lock();

  _queue->_nrStarted--;
  _queue->_nrRunning++;
  _queue->_nrUp++;

  _queue->_startedThreads.insert(this);

  // iterate until we are shutting down.
  while (_jobType != Job::SPECIAL_JOB && _queue->_stopping == 0) {

    // delete old jobs
    for (list<DispatcherThread*>::iterator i = _queue->_stoppedThreads.begin();  i != _queue->_stoppedThreads.end();  ++i) {
      delete *i;
    }

    _queue->_stoppedThreads.clear();
    _queue->_nrStopped = 0;

    // a job is waiting to execute
    if (  ! _queue->_readyJobs.empty()
         && _queue->_monopolizer == 0
         && ! (_queue->_readyJobs.front()->type() == Job::WRITE_JOB && 1 < _queue->_nrRunning)) {

      // try next job
      Job* job = _queue->_readyJobs.front();
      _queue->_readyJobs.pop_front();

      // handle job type
      _jobType = job->type();

      // start a new thread for special jobs
      if (_jobType == Job::SPECIAL_JOB) {
        _queue->_nrRunning--;
        _queue->_nrSpecial++;
        _queue->startQueueThread();
      }

      // monopolize queue
      else if (_jobType == Job::WRITE_JOB) {
        _queue->_monopolizer = this;
      }

      // now release the queue lock (initialise is inside the lock, work outside)
      _queue->_accessQueue.unlock();

      // do the work (this might change the job type)
      Job::status_e status = Job::JOB_FAILED;

      try {

        // set current thread
        job->setDispatcherThread(this);

        // and do all the dirty work
        status = job->work();
      }
      catch (TriagensError const& ex) {
        try {
          job->handleError(ex);
        }
        catch (TriagensError const& ex) {
          LOGGER_WARNING << "caught error while handling error: " << DIAGNOSTIC_INFORMATION(ex);
        }
        catch (std::exception const& ex) {
          LOGGER_WARNING << "caught error while handling error: " << ex.what();
        }
        catch (...) {
          LOGGER_WARNING << "caught error while handling error!";
        }

        status = Job::JOB_FAILED;
      }
      catch (std::exception const& ex) {
        try {
          InternalError err(string("job failed with unknown in work: ") + ex.what());

          job->handleError(err);
        }
        catch (TriagensError const& ex) {
          LOGGER_WARNING << "caught error while handling error: " << DIAGNOSTIC_INFORMATION(ex);
        }
        catch (std::exception const& ex) {
          LOGGER_WARNING << "caught error while handling error: " << ex.what();
        }
        catch (...) {
          LOGGER_WARNING << "caught error while handling error!";
        }

        status = Job::JOB_FAILED;
      }
      catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
        if (_queue->_stopping != 0) {
          LOGGER_WARNING << "caught cancellation exception during work";
          throw;
        }
#endif

        try {
          InternalError err("job failed with unknown error in work");

          job->handleError(err);
        }
        catch (TriagensError const& ex) {
          LOGGER_WARNING << "caught error while handling error: " << DIAGNOSTIC_INFORMATION(ex);
        }
        catch (std::exception const& ex) {
          LOGGER_WARNING << "caught error while handling error: " << ex.what();
        }
        catch (...) {
          LOGGER_WARNING << "caught error while handling error!";
        }

        status = Job::JOB_FAILED;
      }

      // trigger GC
      tick(false);

      // require the lock
      _queue->_accessQueue.lock();

      // cleanup
      _queue->_monopolizer = 0;

      // finish jobs
      try {
        job->setDispatcherThread(0);

        if (status == Job::JOB_DONE) {
          job->cleanup();
        }
#ifdef TRI_ENABLE_ZEROMQ
        else if (status == Job::JOB_DONE_ZEROMQ) {
          job->finish(zBridge);
        }
#endif
        else if (status == Job::JOB_REQUEUE) {
          _queue->_dispatcher->addJob(job);
        }
        else if (status == Job::JOB_FAILED) {
          job->cleanup();
        }
      }
      catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
        if (_queue->_stopping != 0) {
          LOGGER_WARNING << "caught cancellation exception during cleanup";
          _queue->_accessQueue.unlock();
          throw;
        }
#endif

        LOGGER_WARNING << "caught error while cleaning up!";
      }

      if (0 < _queue->_nrWaiting && ! _queue->_readyJobs.empty()) {
        _queue->_accessQueue.broadcast();
      }
    }
    else {

      // cleanup without holding a lock
      _queue->_accessQueue.unlock();
      tick(true);
      _queue->_accessQueue.lock();

      // wait, if there are no jobs
      if (_queue->_readyJobs.empty()) {
        _queue->_nrRunning--;
        _queue->_nrWaiting++;

        _queue->_accessQueue.wait();

        _queue->_nrWaiting--;
        _queue->_nrRunning++;
      }
    }
  }

  _queue->_stoppedThreads.push_back(this);
  _queue->_startedThreads.erase(this);

  _queue->_nrRunning--;
  _queue->_nrStopped++;

  if (_jobType == Job::SPECIAL_JOB) {
    _queue->_nrSpecial--;
  }

  _queue->_nrUp--;

  _queue->_accessQueue.unlock();

#ifdef TRI_ENABLE_ZEROMQ
  if (zBridge != 0) {
    zmq_close(zBridge);
  }
#endif

  LOGGER_TRACE << "dispatcher thread has finished";
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief called to report the status of the thread
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::reportStatus () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief called in regular intervalls
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::tick (bool) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
