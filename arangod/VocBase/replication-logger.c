////////////////////////////////////////////////////////////////////////////////
/// @brief replication logger
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
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "replication-logger.h"

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/tri-strings.h"

#include "VocBase/collection.h"
#include "VocBase/document-collection.h"


#ifdef TRI_ENABLE_REPLICATION

// -----------------------------------------------------------------------------
// --SECTION--                                                       REPLICATION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut function
////////////////////////////////////////////////////////////////////////////////

#define FAIL_IFNOT(func, buffer, val)                                     \
  if (func(buffer, val) != TRI_ERROR_NO_ERROR) {                          \
    return false;                                                         \
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a string-buffer function name
////////////////////////////////////////////////////////////////////////////////

#define APPEND_FUNC(name) TRI_ ## name ## StringBuffer

////////////////////////////////////////////////////////////////////////////////
/// @brief append a character to a string-buffer or fail
////////////////////////////////////////////////////////////////////////////////

#define APPEND_CHAR(buffer, c)      FAIL_IFNOT(APPEND_FUNC(AppendChar), buffer, c)

////////////////////////////////////////////////////////////////////////////////
/// @brief append a string to a string-buffer or fail
////////////////////////////////////////////////////////////////////////////////

#define APPEND_STRING(buffer, str)  FAIL_IFNOT(APPEND_FUNC(AppendString), buffer, str)

////////////////////////////////////////////////////////////////////////////////
/// @brief append uint64 to a string-buffer or fail
////////////////////////////////////////////////////////////////////////////////

#define APPEND_UINT64(buffer, val)  FAIL_IFNOT(APPEND_FUNC(AppendUInt64), buffer, val)

////////////////////////////////////////////////////////////////////////////////
/// @brief append json to a string-buffer or fail
////////////////////////////////////////////////////////////////////////////////

#define APPEND_JSON(buffer, json)   FAIL_IFNOT(TRI_StringifyJson, buffer, json)

////////////////////////////////////////////////////////////////////////////////
/// @brief collection operations
////////////////////////////////////////////////////////////////////////////////

#define OPERATION_COLLECTION_CREATE  "collection-create"
#define OPERATION_COLLECTION_DROP    "collection-drop"
#define OPERATION_COLLECTION_RENAME  "collection-rename"
#define OPERATION_COLLECTION_CHANGE  "collection-change"

////////////////////////////////////////////////////////////////////////////////
/// @brief index operations
////////////////////////////////////////////////////////////////////////////////

#define OPERATION_INDEX_CREATE       "index-create"
#define OPERATION_INDEX_DROP         "index-drop"

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction control operations
////////////////////////////////////////////////////////////////////////////////

#define OPERATION_TRANSACTION        "transaction"

////////////////////////////////////////////////////////////////////////////////
/// @brief document operations
////////////////////////////////////////////////////////////////////////////////

#define OPERATION_DOCUMENT_INSERT    "document-insert"
#define OPERATION_DOCUMENT_UPDATE    "document-update"
#define OPERATION_DOCUMENT_REMOVE    "document-remove"

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   REPLICATION LOG
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief replication log info
////////////////////////////////////////////////////////////////////////////////

typedef struct replication_log_s {
  TRI_voc_fid_t   _id;
  int             _fd;
  bool            _flushed;
  bool            _sealed;
  int64_t         _size;
  TRI_voc_tick_t  _tickMin;
}
replication_log_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create an absolute or relative filename for a log
////////////////////////////////////////////////////////////////////////////////

static char* CreateFilenameLog (TRI_replication_logger_t const* logger,
                                TRI_voc_fid_t fid,
                                bool relative) {
  char* idString;
  char* relFilename;
 
  idString = TRI_StringUInt64(fid);

  if (idString == NULL) {
    return NULL;
  }

  relFilename = TRI_Concatenate3String("replication-", idString, ".db");
  TRI_FreeString(TRI_CORE_MEM_ZONE, idString);

  if (relFilename == NULL) {
    return NULL;
  }

  if (relative) {
    return relFilename;
  }
  else {
    char* absFilename;

    absFilename = TRI_Concatenate2File(logger->_setup._path, relFilename);
    TRI_FreeString(TRI_CORE_MEM_ZONE, relFilename);

    return absFilename;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush a log file
////////////////////////////////////////////////////////////////////////////////

static int FlushLog (replication_log_t* l) {
  if (l->_fd == 0 || l->_sealed) {
    return TRI_ERROR_INTERNAL;
  }

  if (! l->_flushed) {
    if (! TRI_fsync(l->_fd)) {
      // TODO: fix error code
      return TRI_ERROR_INTERNAL;
    }

    l->_flushed = true;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief close a log file
////////////////////////////////////////////////////////////////////////////////

static int CloseLog (replication_log_t* l, 
                     bool seal) {
  if (l->_fd == 0) {
    return TRI_ERROR_INTERNAL;
  }

  if (! l->_sealed) {
    FlushLog(l);

    if (seal) {
      l->_sealed = true;
    }
  }
  
  TRI_CLOSE(l->_fd);
  l->_fd = 0;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a log file
////////////////////////////////////////////////////////////////////////////////

static int RemoveLog (TRI_replication_logger_t* logger,
                      replication_log_t* l) {
  char* absFilename;
  int res;

  if (l->_fd > 0) {
    CloseLog(l, ! l->_sealed);
  }

  absFilename = CreateFilenameLog(logger, l->_id, false);

  res = TRI_UnlinkFile(absFilename);
  TRI_FreeString(TRI_CORE_MEM_ZONE, absFilename);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a JSON representation of a log file
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* JsonLog (TRI_replication_logger_t const* logger,
                     replication_log_t const* l) {
  TRI_json_t* json;

  json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

  if (json != NULL) {
    char* tickString;
    char* relFilename;

    tickString = TRI_StringUInt64((uint64_t) l->_id);
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "id", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, tickString));

    relFilename = CreateFilenameLog(logger, l->_id, true);
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "filename", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, relFilename));

    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "sealed", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, l->_sealed));
  
    tickString = TRI_StringUInt64((uint64_t) l->_tickMin);
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "tickMin", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, tickString));
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a log file
////////////////////////////////////////////////////////////////////////////////

replication_log_t* CreateLog (TRI_voc_fid_t fid) {
  replication_log_t* l;

  l = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(replication_log_t), false);

  if (l != NULL) {
    l->_id       = fid;

    l->_fd       = 0;
    l->_size     = 0;
    l->_flushed  = true;
    l->_sealed   = true;
    l->_tickMin  = fid;
  }

  return l;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a log file
////////////////////////////////////////////////////////////////////////////////

static void DestroyLog (replication_log_t* l) {
  if (l->_fd > 0) {
    CloseLog(l, false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a log file
////////////////////////////////////////////////////////////////////////////////

static void FreeLog (replication_log_t* l) {
  DestroyLog(l);
  TRI_Free(TRI_CORE_MEM_ZONE, l);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return the last log file
/// note: must hold the lock when calling this
////////////////////////////////////////////////////////////////////////////////

static replication_log_t* GetLastLog (TRI_replication_logger_t const* logger) {
  size_t n;

  n = logger->_logs._length;
  if (n == 0) {
    return NULL;
  }

  return TRI_AtVectorPointer(&logger->_logs, (size_t) (n - 1));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the active log
/// note: must hold the lock when calling this
////////////////////////////////////////////////////////////////////////////////

static int CloseActiveLog (TRI_replication_logger_t* logger, 
                           bool seal) {
  replication_log_t* l = GetLastLog(logger);

  if (l == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  return CloseLog(l, seal);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a JSON representation of the replication state
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonStateReplicationLogger (TRI_replication_logger_t* logger) {
  TRI_json_t* logs;
  TRI_json_t* json;
  size_t i, n;

  n = logger->_logs._length;
  logs = TRI_CreateList2Json(TRI_CORE_MEM_ZONE, n);

  for (i = 0; i < n; ++i) {
    replication_log_t* l = TRI_AtVectorPointer(&logger->_logs, i);

    TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, logs, JsonLog(logger, l));
  }
  
  json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

  if (json == NULL) {
    TRI_FreeJson(TRI_CORE_MEM_ZONE, logs);

    return NULL;
  }

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "logs", logs);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save state of the replication system into a JSON file
////////////////////////////////////////////////////////////////////////////////

static int SaveStateReplicationLogger (TRI_replication_logger_t* logger) {
  TRI_json_t* json;
  char* filename;
  bool ok;
  
  filename = TRI_Concatenate2File(logger->_setup._path, "replication.json");

  if (filename == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  json = JsonStateReplicationLogger(logger);  

  if (json == NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  ok = TRI_SaveJson(filename, json, true);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  if (! ok) {
    LOG_ERROR("could not save replication state in file '%s': %s", filename, TRI_last_error());
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return TRI_ERROR_INTERNAL;
  }
  
  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read the state of the replication system from a JSON file
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* LoadStateReplicationLogger (TRI_replication_logger_t* logger) {
  TRI_json_t* json;
  char* filename;

  // read replication state  
  filename = TRI_Concatenate2File(logger->_setup._path, "replication.json");

  if (filename == NULL) {
    return NULL;
  }
  
  if (! TRI_ExistsFile(filename)) {
    LOG_DEBUG("replication state file '%s' does not exist", filename);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return NULL;
  }
    
  LOG_DEBUG("read replication state from file '%s'", filename);

  json = TRI_JsonFile(TRI_CORE_MEM_ZONE, filename, NULL);
  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  // might be NULL
  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove old, currently unused logfiles
/// note: must hold the lock when calling this
////////////////////////////////////////////////////////////////////////////////

static bool RemoveOldReplicationLogger (TRI_replication_logger_t* logger) {
  bool worked;

  worked = false;

  if (logger->_setup._maxLogs > 1 && 
      logger->_logs._length > logger->_setup._maxLogs) {
    size_t n;

    // we'll be removing this many logs
    n = logger->_logs._length - logger->_setup._maxLogs;

    while (n > 0) {
      // pick the first log in the vector
      replication_log_t* l = TRI_AtVectorPointer(&logger->_logs, 0);

      if (l->_sealed) {
        if (RemoveLog(logger, l) == TRI_ERROR_NO_ERROR) {
          FreeLog(l);

          TRI_RemoveVectorPointer(&logger->_logs, 0);
          n--;
          worked = true;

          continue;
        }
      }

      break;
    }
  }

  return worked;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief open the last log file for writing or create a new one
/// note: must hold the lock when calling this
////////////////////////////////////////////////////////////////////////////////

static int OpenLog (TRI_replication_logger_t* logger) {
  replication_log_t* l = GetLastLog(logger);

  if (l != NULL && ! l->_sealed) {
    char* filename;
    off_t offset;
    int fd;
   
    assert(l->_id > 0);
    assert(l->_fd == 0);
    filename = CreateFilenameLog(logger, l->_id, false);

    if (filename == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    fd = TRI_OPEN(filename, O_RDWR);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    if (fd <= 0) {
      return TRI_ERROR_INTERNAL;
    }

    offset = TRI_LSEEK(fd, l->_size, SEEK_SET);
  
    if (offset == (off_t) -1) {
      TRI_CLOSE(fd);

      return TRI_ERROR_INTERNAL;
    }
    
    l->_fd = fd;
    
    return TRI_ERROR_NO_ERROR;
  }
  else {
    char* filename;
    int fd;

    l = CreateLog((TRI_voc_fid_t) TRI_NewTickVocBase());
    
    if (l == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    filename = CreateFilenameLog(logger, l->_id, false);
    
    if (filename == NULL) {
      FreeLog(l);

      return TRI_ERROR_OUT_OF_MEMORY;
    }

    fd = TRI_CREATE(filename, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    if (fd <= 0) {
      FreeLog(l);
      return TRI_ERROR_INTERNAL;
    }
  
    l->_fd = fd;
    l->_sealed = false;

    TRI_PushBackVectorPointer(&logger->_logs, l);
  
    SaveStateReplicationLogger(logger);
  
    return TRI_ERROR_NO_ERROR;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the replication logger
/// note: must hold the lock when calling this
////////////////////////////////////////////////////////////////////////////////

static int StartReplicationLogger (TRI_replication_logger_t* logger) {
  int res;

  if (logger->_active) {
    return TRI_ERROR_INTERNAL;
  }

  if (RemoveOldReplicationLogger(logger)) {
    SaveStateReplicationLogger(logger);
  }

  res = OpenLog(logger);

  if (res == TRI_ERROR_NO_ERROR) {
    LOG_INFO("started replication logger");
    logger->_active = true;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication logger
/// note: must hold the lock when calling this
////////////////////////////////////////////////////////////////////////////////

static int StopReplicationLogger (TRI_replication_logger_t* logger) {
  if (! logger->_active) {
    return TRI_ERROR_INTERNAL;
  }

  CloseActiveLog(logger, false);

  logger->_active = false;
  LOG_INFO("stopped replication logger");

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump information about the replication logger
////////////////////////////////////////////////////////////////////////////////

static void DumpReplicationLogger (TRI_replication_logger_t* logger) {
  size_t i, n;

  printf("\nreplication state\n-----------------\n");
  printf("- active: %d\n", (int) logger->_active);
  printf("- logs:\n");

  n = logger->_logs._length;

  for (i = 0; i < n; ++i) {
    replication_log_t* l = TRI_AtVectorPointer(&logger->_logs, i);

    printf("  - id: %llu\n", (unsigned long long) l->_id);
    printf("  - flushed: %d\n", (int) l->_flushed);
    printf("  - sealed: %d\n", (int) l->_sealed);
    printf("  - size: %d\n\n", (int) l->_size);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two logfiles, based on the logfile ids
////////////////////////////////////////////////////////////////////////////////

static int LogComparator (const void* lhs, const void* rhs) {
  const replication_log_t* l = *((replication_log_t**) lhs);
  const replication_log_t* r = *((replication_log_t**) rhs);

  if (l->_id < r->_id) {
    return -1;
  }
  else if (l->_id > r->_id) {
    return 1;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort a vector of logfiles, using their ids
////////////////////////////////////////////////////////////////////////////////

static void SortLogsReplicationLogger (TRI_replication_logger_t* logger) {
  TRI_vector_pointer_t* logs = &logger->_logs;
  
  if (logs->_length < 2) {
    return;
  }

  qsort(logs->_buffer, logs->_length, sizeof(void*), LogComparator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add an existing log file to the vector of files
////////////////////////////////////////////////////////////////////////////////

static int AddLogReplicationLogger (TRI_replication_logger_t* logger,
                                    TRI_json_t const* json) {
  replication_log_t* l;
  TRI_json_t* id;
  TRI_json_t* tickMin;
  TRI_json_t* sealed;
  TRI_voc_fid_t fid;
  char* absFilename;
  
  if (json == NULL || json->_type != TRI_JSON_ARRAY) {
    return TRI_ERROR_INTERNAL;
  }

  id = TRI_LookupArrayJson(json, "id");
  tickMin = TRI_LookupArrayJson(json, "tickMin");
  sealed  = TRI_LookupArrayJson(json, "sealed");

  if (id == NULL || id->_type != TRI_JSON_STRING) {
    return TRI_ERROR_INTERNAL;
  }
  if (tickMin == NULL || tickMin->_type != TRI_JSON_STRING) {
    return TRI_ERROR_INTERNAL;
  }
  if (sealed == NULL || sealed->_type != TRI_JSON_BOOLEAN) {
    return TRI_ERROR_INTERNAL;
  }

  fid = TRI_UInt64String(id->_value._string.data);
  if (fid == 0) {
    return TRI_ERROR_INTERNAL;
  }

  l = CreateLog((TRI_voc_fid_t) fid);

  if (l == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  l->_tickMin = TRI_UInt64String(tickMin->_value._string.data);
  l->_sealed = sealed->_value._boolean;

  absFilename = CreateFilenameLog(logger, fid, false);

  if (absFilename == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  l->_size = TRI_SizeFile(absFilename);

  if (l->_size < 0) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, absFilename);
    FreeLog(l);

    return TRI_ERROR_INTERNAL;
  }
  
  LOG_DEBUG("adding replication log file '%s', size: %lld, sealed: %d, tickMin: %llu", 
            absFilename,
            (long long int) l->_size,
            (int) l->_sealed,
            (unsigned long long) l->_tickMin);

  TRI_FreeString(TRI_CORE_MEM_ZONE, absFilename);

  TRI_PushBackVectorPointer(&logger->_logs, l);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief look up information about a log file in the JSON
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* LookupLog (TRI_json_t const* logs, 
                              char const* name) {
  size_t i, n;

  if (logs == NULL) {
    return NULL;
  }

  assert(logs->_type == TRI_JSON_LIST);

  n = logs->_value._objects._length;
  for (i = 0; i < n; ++i) {
    TRI_json_t* entry;
    TRI_json_t* filename;

    entry = TRI_LookupListJson(logs, i);

    if (entry == NULL || entry->_type != TRI_JSON_ARRAY) {
      continue;
    }

    filename = TRI_LookupArrayJson(entry, "filename");

    if (filename == NULL || filename->_type != TRI_JSON_STRING) {
      continue;
    }

    if (TRI_EqualString(filename->_value._string.data, name)) {
      return entry;
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief scan the replication directory for existing logs
/// this is done on startup to get an inventory of log files
////////////////////////////////////////////////////////////////////////////////

static int ScanPathReplicationLogger (TRI_replication_logger_t* logger) {
  TRI_vector_string_t files;
  regmatch_t matches[2];
  regex_t re;
  TRI_json_t* json;
  TRI_json_t* logs;
  char* filename;
  size_t i, n;
  int res;
  
  if (! TRI_IsDirectory(logger->_setup._path)) {
    return TRI_ERROR_FILE_NOT_FOUND;
  }
  
  LOG_DEBUG("investigating previous replication state");
  
  // check if tmp file exists
  filename = TRI_Concatenate2File(logger->_setup._path, "replication.json.tmp");

  if (filename == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  // if yes, remove it
  if (TRI_ExistsFile(filename)) {
    LOG_DEBUG("removing dangling replication state file '%s'", filename);
    TRI_UnlinkFile(filename);
  }
  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);


  // load the state of the replication system from a JSON file 
  json = LoadStateReplicationLogger(logger);

  logs = NULL;
  if (json != NULL && json->_type == TRI_JSON_ARRAY) {
    // check "logs" attribute of JSON
    logs = TRI_LookupArrayJson(json, "logs");

    if (logs->_type != TRI_JSON_LIST) {
      logs = NULL;
    }
  }

  res = TRI_ERROR_NO_ERROR;

  files = TRI_FilesDirectory(logger->_setup._path);
  n = files._length;
  
  regcomp(&re, "^replication-([0-9]+)\\\\.db$", REG_EXTENDED);
  
  for (i = 0;  i < n;  ++i) {
    char* name = files._buffer[i];
  
    if (regexec(&re, name, sizeof(matches) / sizeof(matches[0]), matches, 0) != 0) {
      TRI_json_t* logInfo;
   
      // check if we have the filename contained in the JSON we read
      logInfo = LookupLog(logs, name);

      if (logInfo == NULL) {
        // we don't have any information about this log file. better skip it
        continue;
      }

      res = AddLogReplicationLogger(logger, logInfo);

      if (res != TRI_ERROR_NO_ERROR) {
        break;
      }
    }
  }
  
  regfree(&re);
  TRI_DestroyVectorString(&files);

  // free replication state JSON 
  if (json != NULL) {
    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a buffer to write an event in
/// TODO: some optimisations can go here so that we do not create new buffers
/// over and over...
////////////////////////////////////////////////////////////////////////////////

static TRI_string_buffer_t* GetBuffer (TRI_replication_logger_t* logger) {
  TRI_string_buffer_t* buffer;

  assert(logger != NULL);
 
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);
  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a buffer
/// TODO: some optimisations can go here so that we do not dispose unused 
/// buffers but recycle them
////////////////////////////////////////////////////////////////////////////////

static void ReturnBuffer (TRI_replication_logger_t* logger,
                          TRI_string_buffer_t* buffer) {
  assert(logger != NULL);
  assert(buffer != NULL);

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a replication event contained in the buffer
////////////////////////////////////////////////////////////////////////////////

static int LogEvent (TRI_replication_logger_t* logger,
                     TRI_string_buffer_t* buffer) {
  replication_log_t* l;
  size_t len;
  int res;

  assert(logger != NULL);
  assert(buffer != NULL);

  len = TRI_LengthStringBuffer(buffer);

  if (len < 1) {
    // buffer is empty
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_NO_ERROR;
  }
  
  if (TRI_AppendCharStringBuffer(buffer, '\n') != TRI_ERROR_NO_ERROR) {
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // lock start
  // -----------------------------------------
  
  TRI_WriteLockReadWriteLock(&logger->_lock);
  l = GetLastLog(logger);

  if (l == NULL || l->_fd <= 0 || l->_sealed) {
    TRI_WriteUnlockReadWriteLock(&logger->_lock);
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_INTERNAL;
  }

  res = TRI_WRITE(l->_fd, TRI_BeginStringBuffer(buffer), len + 1);

  if (res >= 0) {
    // set new size of log file
    l->_size += (int64_t) (len + 1);

    // check for rotation
    if (l->_size >= logger->_setup._logSize) {
      CloseActiveLog(logger, true);

      // remove old, now superfluous logs
      RemoveOldReplicationLogger(logger);

      res = OpenLog(logger);
    }
  }

  TRI_WriteUnlockReadWriteLock(&logger->_lock);
  
  // lock end
  // -----------------------------------------
  
  ReturnBuffer(logger, buffer);

  if (res != TRI_ERROR_NO_ERROR) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a replication logger
////////////////////////////////////////////////////////////////////////////////

TRI_replication_logger_t* TRI_CreateReplicationLogger (TRI_replication_setup_t const* setup) {
  TRI_replication_logger_t* logger;
  int res;

  if (! TRI_IsDirectory(setup->_path)) {
    LOG_ERROR("replication directory '%s' does not exist", setup->_path);

    return NULL;
  }

  if (! TRI_IsWritable(setup->_path)) {
    LOG_ERROR("replication directory '%s' is not writable", setup->_path);

    return NULL;
  }

  logger = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_replication_logger_t), false);

  if (logger == NULL) {
    return NULL;
  }

  TRI_InitReadWriteLock(&logger->_lock);

  logger->_setup._logSize       = setup->_logSize;
  logger->_setup._path          = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, setup->_path);
  logger->_setup._maxLogs       = setup->_maxLogs;
  logger->_setup._waitForSync   = setup->_waitForSync;
  logger->_active               = false;

  TRI_InitVectorPointer(&logger->_logs, TRI_CORE_MEM_ZONE);

  res = ScanPathReplicationLogger(logger);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not initialise replication: '%s'", TRI_errno_string(res));

    TRI_FreeReplicationLogger(logger);

    return NULL;
  }

  SortLogsReplicationLogger(logger);

  DumpReplicationLogger(logger);

  return logger;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a replication logger
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyReplicationLogger (TRI_replication_logger_t* logger) {
  size_t i, n;

  TRI_StopReplicationLogger(logger);

  SaveStateReplicationLogger(logger);
  
  n = logger->_logs._length;
  for (i = 0; i < n; ++i) {
    FreeLog(TRI_AtVectorPointer(&logger->_logs, i));
  }
  
  TRI_DestroyVectorPointer(&logger->_logs);

  TRI_FreeString(TRI_CORE_MEM_ZONE, logger->_setup._path);
  TRI_DestroyReadWriteLock(&logger->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a replication logger
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeReplicationLogger (TRI_replication_logger_t* logger) {
  TRI_DestroyReplicationLogger(logger);
  TRI_Free(TRI_CORE_MEM_ZONE, logger);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the replication logger
////////////////////////////////////////////////////////////////////////////////

int TRI_StartReplicationLogger (TRI_replication_logger_t* logger) {
  int res;
  
  TRI_WriteLockReadWriteLock(&logger->_lock);
  res = StartReplicationLogger(logger);
  TRI_WriteUnlockReadWriteLock(&logger->_lock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication logger
////////////////////////////////////////////////////////////////////////////////

int TRI_StopReplicationLogger (TRI_replication_logger_t* logger) {
  int res;
  
  TRI_WriteLockReadWriteLock(&logger->_lock);
  res = StopReplicationLogger(logger);
  TRI_WriteUnlockReadWriteLock(&logger->_lock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify the basics of any operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyBasics (TRI_string_buffer_t* buffer,
                             char const* operationType) {
  if (buffer == NULL) {
    return false;
  }

  APPEND_STRING(buffer, "{\"type\":\"");
  APPEND_STRING(buffer, operationType);
  APPEND_STRING(buffer, "\",");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify the id of a transaction
////////////////////////////////////////////////////////////////////////////////

static bool StringifyIdTransaction (TRI_string_buffer_t* buffer,
                                    const TRI_voc_tid_t tid) {
  if (buffer == NULL) {
    return false;
  }

  APPEND_STRING(buffer, "\"tid\":\"");
  APPEND_UINT64(buffer, (uint64_t) tid);
  APPEND_CHAR(buffer, '"');

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify an index context
////////////////////////////////////////////////////////////////////////////////

static bool StringifyIndex (TRI_string_buffer_t* buffer,
                            const TRI_idx_iid_t iid) {
  if (buffer == NULL) {
    return false;
  }

  APPEND_STRING(buffer, "\"index\":{\"id\":\"");
  APPEND_UINT64(buffer, (uint64_t) iid);
  APPEND_STRING(buffer, "\"}");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a collection context 
////////////////////////////////////////////////////////////////////////////////

static bool StringifyCollection (TRI_string_buffer_t* buffer,
                                 const TRI_voc_cid_t cid) {
  if (buffer == NULL) {
    return false;
  }

  APPEND_STRING(buffer, "\"cid\":\"");
  APPEND_UINT64(buffer, (uint64_t) cid);
  APPEND_CHAR(buffer, '"');

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "create collection" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyCreateCollection (TRI_string_buffer_t* buffer,
                                       char const* operationType,
                                       TRI_json_t const* json) {
  if (! StringifyBasics(buffer, operationType)) {
    return false;
  }

  APPEND_STRING(buffer, "\"collection\":");
  APPEND_JSON(buffer, json);
  APPEND_CHAR(buffer, '}');

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "drop collection" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyDropCollection (TRI_string_buffer_t* buffer,
                                     TRI_voc_cid_t cid) {
  if (! StringifyBasics(buffer, OPERATION_COLLECTION_DROP)) {
    return false;
  }
  
  if (! StringifyCollection(buffer, cid)) {
    return false;
  }
  
  APPEND_CHAR(buffer, '}');

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "rename collection" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyRenameCollection (TRI_string_buffer_t* buffer,
                                       TRI_voc_cid_t cid,
                                       char const* name) {
  if (! StringifyBasics(buffer, OPERATION_COLLECTION_RENAME)) {
    return false;
  }

  if (! StringifyCollection(buffer, cid)) {
    return false;
  }

  APPEND_STRING(buffer, ",\"name\":\"");
  // name is user-defined, but does not need escaping
  APPEND_STRING(buffer, name);
  APPEND_STRING(buffer, "\"}");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "create index" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyCreateIndex (TRI_string_buffer_t* buffer,
                                  TRI_voc_cid_t cid,
                                  TRI_json_t const* json) {
  if (! StringifyBasics(buffer, OPERATION_INDEX_CREATE)) {
    return false;
  }
  
  if (! StringifyCollection(buffer, cid)) {
    return false;
  }

  APPEND_STRING(buffer, ",\"index\":");
  APPEND_JSON(buffer, json);
  APPEND_CHAR(buffer, '}'); 

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "drop index" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyDropIndex (TRI_string_buffer_t* buffer,
                                TRI_voc_cid_t cid,
                                TRI_idx_iid_t iid) {
  if (! StringifyBasics(buffer, OPERATION_INDEX_DROP)) {
    return false;
  }
  
  if (! StringifyCollection(buffer, cid)) {
    return false;
  }
  
  APPEND_CHAR(buffer, ','); 

  if (! StringifyIndex(buffer, iid)) {
    return false;
  }
  
  APPEND_CHAR(buffer, '}'); 

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a document operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyDocumentOperation (TRI_string_buffer_t* buffer,
                                        TRI_document_collection_t* document,
                                        TRI_voc_document_operation_e type,
                                        TRI_df_marker_t const* marker,
                                        TRI_doc_mptr_t const* oldHeader,
                                        bool withCid) {
  char* typeString;
  TRI_voc_rid_t oldRev;
  TRI_voc_key_t key;
  
  if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    typeString = OPERATION_DOCUMENT_INSERT;
    oldRev = 0;
  }
  else if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
    typeString = OPERATION_DOCUMENT_UPDATE;
    oldRev = oldHeader->_rid;
  }
  else if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    typeString = OPERATION_DOCUMENT_REMOVE;
    oldRev = oldHeader->_rid;
  }
  else {
    return false;
  }
  
  if (! StringifyBasics(buffer, typeString)) {
    return false;
  }
  
  if (withCid) {
    if (! StringifyCollection(buffer, document->base.base._info._cid)) {
      return false;
    }
    APPEND_CHAR(buffer, ',');
  }
  
  if (marker->_type == TRI_DOC_MARKER_KEY_DELETION) {
    TRI_doc_deletion_key_marker_t const* m = (TRI_doc_deletion_key_marker_t const*) marker; 
    key = ((char*) m) + m->_offsetKey;
  }
  else if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    TRI_doc_document_key_marker_t const* m = (TRI_doc_document_key_marker_t const*) marker; 
    key = ((char*) m) + m->_offsetKey;
  }
  else if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_document_key_marker_t const* m = (TRI_doc_document_key_marker_t const*) marker; 
    key = ((char*) m) + m->_offsetKey;
  }
  else {
    return false;
  }

  APPEND_STRING(buffer, "\"key\":\""); 
  // key is user-defined, but does not need escaping
  APPEND_STRING(buffer, key); 

  if (oldRev > 0) {
    APPEND_STRING(buffer, "\",\"oldRev\":\""); 
    APPEND_UINT64(buffer, (uint64_t) oldRev); 
  }

  // document
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_document_key_marker_t const* m = (TRI_doc_document_key_marker_t const*) marker; 
    TRI_shaped_json_t shaped;
    
    APPEND_STRING(buffer, "\",\"doc\":{");
    
    // common document meta-data
    APPEND_STRING(buffer, "\"_key\":\"");
    APPEND_STRING(buffer, key);
    APPEND_STRING(buffer, "\",\"_rev\":\"");
    APPEND_UINT64(buffer, marker->_tick);
    APPEND_CHAR(buffer, '"');

    if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
      TRI_doc_edge_key_marker_t const* e = (TRI_doc_edge_key_marker_t const*) marker;
      TRI_voc_key_t fromKey = ((char*) e) + e->_offsetFromKey;
      TRI_voc_key_t toKey = ((char*) e) + e->_offsetToKey;

      APPEND_STRING(buffer, ",\"_from\":\"");
      APPEND_UINT64(buffer, e->_fromCid);
      APPEND_CHAR(buffer, '/');
      APPEND_STRING(buffer, fromKey);
      APPEND_STRING(buffer, "\",\"_to\":\"");
      APPEND_UINT64(buffer, e->_toCid);
      APPEND_CHAR(buffer, '/');
      APPEND_STRING(buffer, toKey);
      APPEND_CHAR(buffer, '"');
    }

    // the actual document data
    TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, m);
    TRI_StringifyArrayShapedJson(document->base._shaper, buffer, &shaped, true);

    APPEND_STRING(buffer, "}}");
  }
  else {
    APPEND_STRING(buffer, "\"}");
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a transaction
////////////////////////////////////////////////////////////////////////////////

static bool StringifyTransaction (TRI_string_buffer_t* buffer,
                                  TRI_transaction_t const* trx) {
  size_t i, n;
  bool printed;

  if (! StringifyBasics(buffer, OPERATION_TRANSACTION)) {
    return false;
  }
   
  if (! StringifyIdTransaction(buffer, trx->_id)) {
    return false;
  }

  APPEND_STRING(buffer, ",\"collections\":{");

  printed = false;
  n = trx->_collections._length;

  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* trxCollection;
    TRI_document_collection_t* document;
    size_t j, k;

    trxCollection = TRI_AtVectorPointer(&trx->_collections, i);

    if (trxCollection->_operations == NULL) {
      // no markers available for collection
      continue;
    }
    
    document = (TRI_document_collection_t*) trxCollection->_collection->_collection;
      
    if (printed) {
      APPEND_CHAR(buffer, ',');
    }
    else {
      printed = true;
    }
  
    APPEND_STRING(buffer, "\"cid\":\"");
    APPEND_UINT64(buffer, (uint64_t) document->base.base._info._cid);
    APPEND_STRING(buffer, "\",\"operations\":[");

    // write the individual operations
    k = trxCollection->_operations->_length;
    for (j = 0; j < k; ++j) {
      TRI_transaction_operation_t* trxOperation;

      trxOperation = TRI_AtVector(trxCollection->_operations, j);

      if (j > 0) {
        APPEND_CHAR(buffer, ',');
      }

      if (! StringifyDocumentOperation(buffer, document, trxOperation->_type, trxOperation->_marker, trxOperation->_oldHeader, false)) {
        return false;
      }
    }
    
    APPEND_CHAR(buffer, ']');
  }

  APPEND_STRING(buffer, "}}");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              public log functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_TransactionReplication (TRI_vocbase_t* vocbase,
                                TRI_transaction_t const* trx) {
  TRI_string_buffer_t* buffer;
  TRI_replication_logger_t* logger;
  
  assert(trx->_replicate);
  assert(trx->_hasOperations);

  logger = vocbase->_replicationLogger;
  if (! logger->_active) {
    return TRI_ERROR_NO_ERROR;
  }

  buffer = GetBuffer(logger);
  
  if (! StringifyTransaction(buffer, trx)) {
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_OUT_OF_MEMORY;
  }
   
  return LogEvent(logger, buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "create collection" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateCollectionReplication (TRI_vocbase_t* vocbase,
                                     TRI_voc_cid_t cid,
                                     TRI_json_t const* json) {
  TRI_string_buffer_t* buffer;
  TRI_replication_logger_t* logger;

  logger = vocbase->_replicationLogger;
  if (! logger->_active) {
    return TRI_ERROR_NO_ERROR;
  }
  
  buffer = GetBuffer(logger);

  if (! StringifyCreateCollection(buffer, OPERATION_COLLECTION_CREATE, json)) {
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return LogEvent(logger, buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "drop collection" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_DropCollectionReplication (TRI_vocbase_t* vocbase,
                                   TRI_voc_cid_t cid) {
  TRI_string_buffer_t* buffer;
  TRI_replication_logger_t* logger;
  
  logger = vocbase->_replicationLogger;
  if (! logger->_active) {
    return TRI_ERROR_NO_ERROR;
  }
  
  buffer = GetBuffer(logger);

  if (! StringifyDropCollection(buffer, cid)) {
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return LogEvent(logger, buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "rename collection" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_RenameCollectionReplication (TRI_vocbase_t* vocbase,
                                     TRI_voc_cid_t cid,
                                     char const* name) {
  TRI_string_buffer_t* buffer;
  TRI_replication_logger_t* logger;
  
  logger = vocbase->_replicationLogger;
  if (! logger->_active) {
    return TRI_ERROR_NO_ERROR;
  }
  
  buffer = GetBuffer(logger);

  if (! StringifyRenameCollection(buffer, cid, name)) {
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return LogEvent(logger, buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "change collection properties" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_ChangePropertiesCollectionReplication (TRI_vocbase_t* vocbase,
                                               TRI_voc_cid_t cid,
                                               TRI_json_t const* json) {
  TRI_string_buffer_t* buffer;
  TRI_replication_logger_t* logger;
  
  logger = vocbase->_replicationLogger;
  if (! logger->_active) {
    return TRI_ERROR_NO_ERROR;
  }
  
  buffer = GetBuffer(logger);

  if (! StringifyCreateCollection(buffer, OPERATION_COLLECTION_CHANGE, json)) {
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return LogEvent(logger, buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "create index" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateIndexReplication (TRI_vocbase_t* vocbase,
                                TRI_voc_cid_t cid,
                                TRI_idx_iid_t iid,
                                TRI_json_t const* json) {
  TRI_string_buffer_t* buffer;
  TRI_replication_logger_t* logger;
  
  logger = vocbase->_replicationLogger;
  if (! logger->_active) {
    return TRI_ERROR_NO_ERROR;
  }
  
  buffer = GetBuffer(logger);

  if (! StringifyCreateIndex(buffer, cid, json)) {
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return LogEvent(logger, buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "drop index" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_DropIndexReplication (TRI_vocbase_t* vocbase,
                              TRI_voc_cid_t cid,
                              TRI_idx_iid_t iid) {
  TRI_string_buffer_t* buffer;
  TRI_replication_logger_t* logger;
  
  logger = vocbase->_replicationLogger;
  if (! logger->_active) {
    return TRI_ERROR_NO_ERROR;
  }
  
  buffer = GetBuffer(logger);

  if (! StringifyDropIndex(buffer, cid, iid)) {
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return LogEvent(logger, buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a document operation
////////////////////////////////////////////////////////////////////////////////

int TRI_DocumentReplication (TRI_vocbase_t* vocbase,
                             TRI_document_collection_t* document,
                             TRI_voc_document_operation_e type,
                             TRI_df_marker_t const* marker,
                             TRI_doc_mptr_t const* oldHeader) {
  TRI_string_buffer_t* buffer;
  TRI_replication_logger_t* logger;

  logger = vocbase->_replicationLogger;
  if (! logger->_active) {
    return TRI_ERROR_NO_ERROR;
  }

  buffer = GetBuffer(logger);

  if (! StringifyDocumentOperation(buffer, document, type, marker, oldHeader, true)) {
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return LogEvent(logger, buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    dump functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

// /_api/replication/dump-start: 
// - keep track of current tick, activate replication log, set flag to keep replication logs infinitely
// - return list of all collections plus current tick
// for each collection in result:
//   /_api/replication/dump-collection?collection=abc&last=0 // create a barrier // ... dump ... // drop barrier
// return all data + "hasMore" attribute
//   /_api/replication/dump-collection?collection=abc&last=9999
// until no more data for a collection
// after that:
//   /_api/replication/dump-end: to remove any barriers etc.
//   /_api/replication/dump-continuous?last=... to access the stream of replication events... server-push
// clients needs to note last xfered tick

// client: replication.endpoint. establish connection and query data incrementally
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
