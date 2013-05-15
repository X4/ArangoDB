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
#include "BasicsC/string-buffer.h"
#include "BasicsC/tri-strings.h"

#include "VocBase/collection.h"
#include "VocBase/document-collection.h"

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

#define LOG_REPLICATION(buffer) printf("REPLICATION: %s\n\n", buffer)

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

#define OPERATION_TRANSACTION_BEGIN  "transaction-begin"
#define OPERATION_TRANSACTION_COMMIT "transaction-commit"

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
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify the basics of any operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

static bool StringifyBasics (TRI_string_buffer_t* buffer,
                             const TRI_voc_tick_t tick,
                             char const* operationType) {
  APPEND_STRING(buffer, "{\"type\":\"");
  APPEND_STRING(buffer, operationType);
  APPEND_STRING(buffer, "\",\"tick\":\""); 
  APPEND_UINT64(buffer, (uint64_t) tick);
  APPEND_STRING(buffer, "\",");

  return true;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify the id of a transaction
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

static bool StringifyIdTransaction (TRI_string_buffer_t* buffer,
                                    const TRI_voc_tid_t tid) {

  APPEND_STRING(buffer, "\"tid\":\"");
  APPEND_UINT64(buffer, (uint64_t) tid);
  APPEND_CHAR(buffer, '"');

  return true;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify the collections of a transaction
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

static bool StringifyCollectionsTransaction (TRI_string_buffer_t* buffer,
                                             TRI_transaction_t const* trx) {
  size_t i, n;
  bool empty;

  APPEND_STRING(buffer, ",\"collections\":[");

  n = trx->_collections._length;
  empty = true;

  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* trxCollection;

    trxCollection = TRI_AtVectorPointer(&trx->_collections, i);

    if (trxCollection->_operations == NULL) {
      // no markers available for collection
      continue;
    }

    if (empty) {
      APPEND_CHAR(buffer, '"');

      empty = false;
    }
    else {
      APPEND_STRING(buffer, ",\"");
    }

    APPEND_UINT64(buffer, (uint64_t) trxCollection->_cid);
    APPEND_CHAR(buffer, '"');
  }
  
  APPEND_STRING(buffer, "]");

  return true;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "begin transaction" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

static bool StringifyBeginTransaction (TRI_string_buffer_t* buffer,
                                       TRI_voc_tick_t tick,
                                       TRI_transaction_t const* trx) {
  if (! StringifyBasics(buffer, tick, OPERATION_TRANSACTION_BEGIN)) {
    return false;
  }

  if (! StringifyIdTransaction(buffer, trx->_id)) {
    return false;
  }

  if (! StringifyCollectionsTransaction(buffer, trx)) {
    return false;
  }

  APPEND_CHAR(buffer, '}');

  return true;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "commit transaction" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

static bool StringifyCommitTransaction (TRI_string_buffer_t* buffer,
                                        TRI_voc_tick_t tick,
                                        TRI_voc_tid_t tid) {
  if (! StringifyBasics(buffer, tick, OPERATION_TRANSACTION_COMMIT)) {
    return false;
  }

  if (! StringifyIdTransaction(buffer, tid)) {
    return false;
  }

  APPEND_CHAR(buffer, '}');

  return true;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify an index context
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

static bool StringifyIndex (TRI_string_buffer_t* buffer,
                            const TRI_idx_iid_t iid) {
  APPEND_STRING(buffer, "\"index\":{\"id\":\"");
  APPEND_UINT64(buffer, (uint64_t) iid);
  APPEND_STRING(buffer, "\"}");

  return true;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a collection context 
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

static bool StringifyCollection (TRI_string_buffer_t* buffer,
                                 const TRI_voc_cid_t cid) {
  APPEND_STRING(buffer, "\"collection\":{\"cid\":\"");
  APPEND_UINT64(buffer, (uint64_t) cid);
  APPEND_STRING(buffer, "\"}");

  return true;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "create collection" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

static bool StringifyCreateCollection (TRI_string_buffer_t* buffer,
                                       TRI_voc_tick_t tick,
                                       char const* operationType,
                                       TRI_json_t const* json) {
  if (! StringifyBasics(buffer, tick, operationType)) {
    return false;
  }

  APPEND_STRING(buffer, "\"collection\":");
  APPEND_JSON(buffer, json);
  APPEND_CHAR(buffer, '}');

  return true;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "drop collection" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

static bool StringifyDropCollection (TRI_string_buffer_t* buffer,
                                     TRI_voc_tick_t tick,
                                     TRI_voc_cid_t cid) {
  if (! StringifyBasics(buffer, tick, OPERATION_COLLECTION_DROP)) {
    return false;
  }
  
  if (! StringifyCollection(buffer, cid)) {
    return false;
  }
  
  APPEND_CHAR(buffer, '}');

  return true;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "rename collection" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

static bool StringifyRenameCollection (TRI_string_buffer_t* buffer,
                                       TRI_voc_tick_t tick,
                                       TRI_voc_cid_t cid,
                                       char const* name) {
  if (! StringifyBasics(buffer, tick, OPERATION_COLLECTION_RENAME)) {
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

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "create index" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

static bool StringifyCreateIndex (TRI_string_buffer_t* buffer,
                                  TRI_voc_tick_t tick,
                                  TRI_voc_cid_t cid,
                                  TRI_json_t const* json) {

  if (! StringifyBasics(buffer, tick, OPERATION_INDEX_CREATE)) {
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

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "drop index" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

static bool StringifyDropIndex (TRI_string_buffer_t* buffer,
                                TRI_voc_tick_t tick,
                                TRI_voc_cid_t cid,
                                TRI_idx_iid_t iid) {
  
  if (! StringifyBasics(buffer, tick, OPERATION_INDEX_DROP)) {
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

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a document operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

static bool StringifyDocumentOperation (TRI_string_buffer_t* buffer,
                                        TRI_document_collection_t* document,
                                        TRI_voc_tid_t tid,
                                        TRI_voc_document_operation_e type,
                                        TRI_df_marker_t const* marker,
                                        TRI_doc_mptr_t const* oldHeader) {
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
  
  if (! StringifyBasics(buffer, marker->_tick, typeString)) {
    return false;
  }
  
  if (! StringifyIdTransaction(buffer, tid)) {
    return false;
  }

  APPEND_CHAR(buffer, '}'); 
  
  if (! StringifyCollection(buffer, document->base.base._info._cid)) {
    return false;
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

  APPEND_STRING(buffer, ",\"key\":\""); 
  // key is user-defined, but does not need escaping
  APPEND_STRING(buffer, key); 

  if (oldRev > 0) {
    APPEND_STRING(buffer, ",\"oldRev\":\""); 
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

#endif

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
/// @brief replicate a transaction
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_TransactionReplication (TRI_transaction_t const* trx) {
  TRI_string_buffer_t* buffer;
  size_t i, n;

  if (! trx->_replicate) {
    return TRI_ERROR_NO_ERROR;
  }
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyBeginTransaction(buffer, TRI_NewTickVocBase(), trx)) {
    LOG_REPLICATION(buffer->_buffer);
  }

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

    // write the individual operations
    k = trxCollection->_operations->_length;
    for (j = 0; j < k; ++j) {
      TRI_transaction_operation_t* trxOperation;

      trxOperation = TRI_AtVector(trxCollection->_operations, j);
  
      if (StringifyDocumentOperation(buffer, document, trx->_id, trxOperation->_type, trxOperation->_marker, trxOperation->_oldHeader)) {
        LOG_REPLICATION(buffer->_buffer);
      }
    }
  }
  
  if (StringifyCommitTransaction(buffer, TRI_NewTickVocBase(), trx->_id)) {
    LOG_REPLICATION(buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return TRI_ERROR_NO_ERROR;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "create collection" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_CreateCollectionReplication (TRI_voc_cid_t cid,
                                     TRI_json_t const* json) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyCreateCollection(buffer, TRI_NewTickVocBase(), OPERATION_COLLECTION_CREATE, json)) {
    LOG_REPLICATION(buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return TRI_ERROR_NO_ERROR;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "drop collection" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_DropCollectionReplication (TRI_voc_cid_t cid) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyDropCollection(buffer, TRI_NewTickVocBase(), cid)) {
    LOG_REPLICATION(buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return TRI_ERROR_NO_ERROR;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "rename collection" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_RenameCollectionReplication (TRI_voc_cid_t cid,
                                     char const* name) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyRenameCollection(buffer, TRI_NewTickVocBase(), cid, name)) {
    LOG_REPLICATION(buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return TRI_ERROR_NO_ERROR;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "change collection properties" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_ChangePropertiesCollectionReplication (TRI_voc_cid_t cid,
                                               TRI_json_t const* json) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyCreateCollection(buffer, TRI_NewTickVocBase(), OPERATION_COLLECTION_CHANGE, json)) {
    LOG_REPLICATION(buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return TRI_ERROR_NO_ERROR;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "create index" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_CreateIndexReplication (TRI_voc_cid_t cid,
                                TRI_idx_iid_t iid,
                                TRI_json_t const* json) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyCreateIndex(buffer, TRI_NewTickVocBase(), cid, json)) {
    LOG_REPLICATION(buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return TRI_ERROR_NO_ERROR;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "drop index" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_DropIndexReplication (TRI_voc_cid_t cid,
                              TRI_idx_iid_t iid) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyDropIndex(buffer, TRI_NewTickVocBase(), cid, iid)) {
    LOG_REPLICATION(buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return TRI_ERROR_NO_ERROR;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a document operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_DocumentReplication (TRI_document_collection_t* document,
                             TRI_voc_document_operation_e type,
                             TRI_df_marker_t const* marker,
                             TRI_doc_mptr_t const* oldHeader) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyDocumentOperation(buffer, document, 0, type, marker, oldHeader)) {
    LOG_REPLICATION(buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return TRI_ERROR_NO_ERROR;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
