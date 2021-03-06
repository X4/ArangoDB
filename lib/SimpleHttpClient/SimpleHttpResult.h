////////////////////////////////////////////////////////////////////////////////
/// @brief http request result
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_SIMPLE_HTTP_CLIENT_SIMPLE_HTTP_RESULT_H
#define TRIAGENS_SIMPLE_HTTP_CLIENT_SIMPLE_HTTP_RESULT_H 1

#include "Basics/Common.h"

#include <map>
#include <string>
#include <sstream>

////////////////////////////////////////////////////////////////////////////////
/// @brief class for storing a request result
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace httpclient {

    class SimpleHttpResult {
    private:
      SimpleHttpResult (SimpleHttpResult const&);
      SimpleHttpResult& operator= (SimpleHttpResult const&);

    public:

////////////////////////////////////////////////////////////////////////////////
/// @brief result types
////////////////////////////////////////////////////////////////////////////////

      enum resultTypes {
        COMPLETE = 0,
        COULD_NOT_CONNECT,
        WRITE_ERROR,
        READ_ERROR,
        UNKNOWN
      };

    public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

      SimpleHttpResult ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

      ~SimpleHttpResult ();

    public:

////////////////////////////////////////////////////////////////////////////////
/// @brief clear result values
////////////////////////////////////////////////////////////////////////////////

      void clear ();

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether the response contains an HTTP error
////////////////////////////////////////////////////////////////////////////////

      bool wasHttpError () const {
        return (_returnCode >= 400);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the http return code
////////////////////////////////////////////////////////////////////////////////

      int getHttpReturnCode () const {
        return _returnCode;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the http return code
////////////////////////////////////////////////////////////////////////////////

      void setHttpReturnCode (int returnCode) {
        this->_returnCode = returnCode;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the http return message
////////////////////////////////////////////////////////////////////////////////

      string getHttpReturnMessage () const {
        return _returnMessage;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the http return message
////////////////////////////////////////////////////////////////////////////////

      void setHttpReturnMessage (const string& message) {
        this->_returnMessage = message;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the content length
////////////////////////////////////////////////////////////////////////////////

      size_t getContentLength () const {
        return _contentLength;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the content length
////////////////////////////////////////////////////////////////////////////////

      void setContentLength (size_t len) {
        _contentLength = len;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the http body
////////////////////////////////////////////////////////////////////////////////

      std::stringstream& getBody ();

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the request result type
////////////////////////////////////////////////////////////////////////////////

      enum resultTypes getResultType () const {
        return _requestResultType;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if result type == OK
////////////////////////////////////////////////////////////////////////////////

      bool isComplete () const {
        return _requestResultType == COMPLETE;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if "transfer-encoding: chunked"
////////////////////////////////////////////////////////////////////////////////

      bool isChunked () const {
        return _chunked;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if "content-encoding: deflate"
////////////////////////////////////////////////////////////////////////////////

      bool isDeflated () const {
        return _deflated;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the request result type
////////////////////////////////////////////////////////////////////////////////

      void setResultType (enum resultTypes requestResultType) {
        this->_requestResultType = requestResultType;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a message for the result type
////////////////////////////////////////////////////////////////////////////////

      std::string getResultTypeMessage ();

////////////////////////////////////////////////////////////////////////////////
/// @brief add header field
////////////////////////////////////////////////////////////////////////////////

      void addHeaderField (std::string const& line);

////////////////////////////////////////////////////////////////////////////////
/// @brief add header field
////////////////////////////////////////////////////////////////////////////////

      void addHeaderField (std::string const& key, std::string const& value);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the value of a single header
////////////////////////////////////////////////////////////////////////////////
    
      string getHeaderField (const string&, bool&) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief get X-VOC-* header fields
////////////////////////////////////////////////////////////////////////////////

      const std::map<std::string, std::string>& getHeaderFields () {
        return _headerFields;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief get content type header field
////////////////////////////////////////////////////////////////////////////////

      const std::string getContentType (const bool partial);


    private:

      // header informtion
      int _returnCode;
      string _returnMessage;
      size_t _contentLength;
      bool _chunked;
      bool _deflated;

      // body content
      std::stringstream _resultBody;

      // request result type
      enum resultTypes _requestResultType;

      // header fields
      std::map<std::string, std::string> _headerFields;
    };
  }
}
#endif

