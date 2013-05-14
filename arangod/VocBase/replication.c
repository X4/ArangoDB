////////////////////////////////////////////////////////////////////////////////
/// @brief replication
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

#include "replication.h"

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/hashes.h"
#include "BasicsC/logging.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/tri-strings.h"

#include "VocBase/collection.h"
#include "VocBase/document-collection.h"

#define LOG_REPLICATION(buffer) \
  printf("REPLICATION: %s\n\n", buffer)

// -----------------------------------------------------------------------------
// --SECTION--                                                       REPLICATION
// -----------------------------------------------------------------------------

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

static bool StringifyBasics (TRI_string_buffer_t* buffer,
                             const TRI_voc_tick_t tick,
                             char const* operationType) {
  
  TRI_AppendStringStringBuffer(buffer, "{\"operation\":\"");
  TRI_AppendStringStringBuffer(buffer, operationType);
  TRI_AppendStringStringBuffer(buffer, "\",\"tick\":\"");
  TRI_AppendUInt64StringBuffer(buffer, (uint64_t) tick); 
  TRI_AppendStringStringBuffer(buffer, "\",");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify the id of a transaction
////////////////////////////////////////////////////////////////////////////////

static bool StringifyIdTransaction (TRI_string_buffer_t* buffer,
                                    const TRI_voc_tid_t tid) {
  TRI_AppendStringStringBuffer(buffer, "\"tid\":\"");
  TRI_AppendUInt64StringBuffer(buffer, (uint64_t) tid); 
  TRI_AppendStringStringBuffer(buffer, "\"");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify the collections of a transaction
////////////////////////////////////////////////////////////////////////////////

static bool StringifyCollectionsTransaction (TRI_string_buffer_t* buffer,
                                             TRI_transaction_t const* trx) {
  size_t i, n;
  bool empty;

  TRI_AppendStringStringBuffer(buffer, ",\"collections\":[");
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
      TRI_AppendCharStringBuffer(buffer, '"');
      empty = false;
    }
    else {
      TRI_AppendStringStringBuffer(buffer, ",\"");
    }

    TRI_AppendUInt64StringBuffer(buffer, (uint64_t) trxCollection->_cid);
    TRI_AppendCharStringBuffer(buffer, '"');
  }
  
  TRI_AppendStringStringBuffer(buffer, "]");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "begin transaction" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyBeginTransaction (TRI_string_buffer_t* buffer,
                                       TRI_voc_tick_t tick,
                                       TRI_transaction_t const* trx) {
  if (! StringifyBasics(buffer, tick, "begin-transaction")) {
    return false;
  }

  if (! StringifyIdTransaction(buffer, trx->_id)) {
    return false;
  }

  if (! StringifyCollectionsTransaction(buffer, trx)) {
    return false;
  }

  TRI_AppendCharStringBuffer(buffer, '}');

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "commit transaction" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyCommitTransaction (TRI_string_buffer_t* buffer,
                                        TRI_voc_tick_t tick,
                                        TRI_voc_tid_t tid) {
  if (! StringifyBasics(buffer, tick, "commit-transaction")) {
    return false;
  }

  if (! StringifyIdTransaction(buffer, tid)) {
    return false;
  }

  TRI_AppendCharStringBuffer(buffer, '}');

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify an index context
////////////////////////////////////////////////////////////////////////////////

static bool StringifyIndex (TRI_string_buffer_t* buffer,
                            const TRI_idx_iid_t iid) {

  TRI_AppendStringStringBuffer(buffer, "\"index\":{\"id\":\"");
  TRI_AppendUInt64StringBuffer(buffer, (uint64_t) iid);
  TRI_AppendStringStringBuffer(buffer, "\"}");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a collection context 
////////////////////////////////////////////////////////////////////////////////

static bool StringifyCollection (TRI_string_buffer_t* buffer,
                                 const TRI_voc_cid_t cid) {

  TRI_AppendStringStringBuffer(buffer, "\"collection\":{\"cid\":\"");
  TRI_AppendUInt64StringBuffer(buffer, (uint64_t) cid);
  TRI_AppendStringStringBuffer(buffer, "\"}");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "create collection" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyCreateCollection (TRI_string_buffer_t* buffer,
                                       TRI_voc_tick_t tick,
                                       TRI_json_t const* json) {
  if (! StringifyBasics(buffer, tick, "create-collection")) {
    return false;
  }

  TRI_AppendStringStringBuffer(buffer, "\"collection\":");

  TRI_StringifyJson(buffer, json);

  TRI_AppendCharStringBuffer(buffer, '}');

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "drop collection" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyDropCollection (TRI_string_buffer_t* buffer,
                                     TRI_voc_tick_t tick,
                                     TRI_voc_cid_t cid) {
  
  if (! StringifyBasics(buffer, tick, "drop-collection")) {
    return false;
  }
  
  if (! StringifyCollection(buffer, cid)) {
    return false;
  }
  
  TRI_AppendCharStringBuffer(buffer, '}');

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "rename collection" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyRenameCollection (TRI_string_buffer_t* buffer,
                                       TRI_voc_tick_t tick,
                                       TRI_voc_cid_t cid,
                                       char const* name) {

  if (! StringifyBasics(buffer, tick, "rename-collection")) {
    return false;
  }

  if (! StringifyCollection(buffer, cid)) {
    return false;
  }

  TRI_AppendStringStringBuffer(buffer, ",\"name\":\"");
  TRI_AppendStringStringBuffer(buffer, name);

  TRI_AppendStringStringBuffer(buffer, "\"}");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "create index" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyCreateIndex (TRI_string_buffer_t* buffer,
                                  TRI_voc_tick_t tick,
                                  TRI_voc_cid_t cid,
                                  TRI_json_t const* json) {

  if (! StringifyBasics(buffer, tick, "create-index")) {
    return false;
  }
  
  if (! StringifyCollection(buffer, cid)) {
    return false;
  }

  TRI_AppendStringStringBuffer(buffer, ",\"index\":");

  TRI_StringifyJson(buffer, json);

  TRI_AppendCharStringBuffer(buffer, '}');

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "drop index" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyDropIndex (TRI_string_buffer_t* buffer,
                                TRI_voc_tick_t tick,
                                TRI_voc_cid_t cid,
                                TRI_idx_iid_t iid) {
  
  if (! StringifyBasics(buffer, tick, "drop-index")) {
    return false;
  }
  
  if (! StringifyCollection(buffer, cid)) {
    return false;
  }
  
  TRI_AppendCharStringBuffer(buffer, ',');

  if (! StringifyIndex(buffer, iid)) {
    return false;
  }
  
  TRI_AppendCharStringBuffer(buffer, '}');

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a document operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyDocumentOperation (TRI_string_buffer_t* buffer,
                                        TRI_document_collection_t* document,
                                        TRI_voc_tid_t tid,
                                        TRI_voc_document_operation_e type,
                                        TRI_df_marker_t const* marker) {
  char* typeString;
  TRI_voc_key_t key;
  
  if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    typeString = "insert";
  }
  else if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
    typeString = "update";
  }
  else if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    typeString = "remove";
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

  TRI_AppendCharStringBuffer(buffer, ',');
  
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

  TRI_AppendStringStringBuffer(buffer, ",\"key\":\"");
  TRI_AppendStringStringBuffer(buffer, key);

  // document
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_document_key_marker_t const* m = (TRI_doc_document_key_marker_t const*) marker; 
    TRI_shaped_json_t shaped;
    
    TRI_AppendStringStringBuffer(buffer, "\",\"doc\":{");
    
    // common document meta-data
    TRI_AppendStringStringBuffer(buffer, "\"_key\":\"");
    TRI_AppendStringStringBuffer(buffer, key);
    TRI_AppendStringStringBuffer(buffer, "\",\"_rev\":\"");
    TRI_AppendUInt64StringBuffer(buffer, marker->_tick);
    TRI_AppendStringStringBuffer(buffer, "\"");

    if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
      TRI_doc_edge_key_marker_t const* e = (TRI_doc_edge_key_marker_t const*) marker;
      TRI_voc_key_t fromKey = ((char*) e) + e->_offsetFromKey;
      TRI_voc_key_t toKey = ((char*) e) + e->_offsetToKey;

      TRI_AppendStringStringBuffer(buffer, ",\"_from\":\"");
      TRI_AppendUInt64StringBuffer(buffer, e->_fromCid);
      TRI_AppendStringStringBuffer(buffer, "/");
      TRI_AppendStringStringBuffer(buffer, fromKey);
      TRI_AppendStringStringBuffer(buffer, "\",\"_to\":\"");
      TRI_AppendUInt64StringBuffer(buffer, e->_toCid);
      TRI_AppendStringStringBuffer(buffer, "/");
      TRI_AppendStringStringBuffer(buffer, toKey);
      TRI_AppendStringStringBuffer(buffer, "\"");
    }

    // the actual document data
    TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, m);
    TRI_StringifyArrayShapedJson(document->base._shaper, buffer, &shaped, true);

    TRI_AppendStringStringBuffer(buffer, "}}");
  }
  else {
    TRI_AppendStringStringBuffer(buffer, "\"}");
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a transaction
////////////////////////////////////////////////////////////////////////////////

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
  TRI_ClearStringBuffer(buffer);

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
  
      if (StringifyDocumentOperation(buffer, document, trx->_id, trxOperation->_type, trxOperation->_marker)) {
        LOG_REPLICATION(buffer->_buffer);
      }

      TRI_ClearStringBuffer(buffer);
    }
  }
  
  if (StringifyCommitTransaction(buffer, TRI_NewTickVocBase(), trx->_id)) {
    LOG_REPLICATION(buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "create collection" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateCollectionReplication (TRI_voc_cid_t cid,
                                     TRI_json_t const* json) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyCreateCollection(buffer, TRI_NewTickVocBase(), json)) {
    LOG_REPLICATION(buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "drop collection" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_DropCollectionReplication (TRI_voc_cid_t cid) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyDropCollection(buffer, TRI_NewTickVocBase(), cid)) {
    LOG_REPLICATION(buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "rename collection" operation
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "create index" operation
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "drop index" operation
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a document operation
////////////////////////////////////////////////////////////////////////////////

int TRI_DocumentReplication (TRI_document_collection_t* document,
                             TRI_voc_document_operation_e type,
                             TRI_df_marker_t const* marker) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyDocumentOperation(buffer, document, 0, type, marker)) {
    LOG_REPLICATION(buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
