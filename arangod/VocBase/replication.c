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
/// @brief stringify the basics of a transaction
////////////////////////////////////////////////////////////////////////////////

static bool StringifyTransaction (TRI_string_buffer_t* buffer,
                                  const TRI_voc_tid_t tid) {
  TRI_AppendStringStringBuffer(buffer, "\"tid\":\"");
  TRI_AppendUInt64StringBuffer(buffer, (uint64_t) tid); 
  TRI_AppendStringStringBuffer(buffer, "\"");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "begin transaction" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyBeginTransaction (TRI_string_buffer_t* buffer,
                                       TRI_voc_tick_t tick,
                                       TRI_voc_tid_t tid) {
  if (! StringifyBasics(buffer, tick, "begin")) {
    return false;
  }

  if (! StringifyTransaction(buffer, tid)) {
    return false;
  }

  TRI_AppendCharStringBuffer(buffer, '}');

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify an "abort transaction" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyAbortTransaction (TRI_string_buffer_t* buffer,
                                       TRI_voc_tick_t tick,
                                       TRI_voc_tid_t tid) {
  if (! StringifyBasics(buffer, tick, "abort")) {
    return false;
  }

  if (! StringifyTransaction(buffer, tid)) {
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
  if (! StringifyBasics(buffer, tick, "commit")) {
    return false;
  }

  if (! StringifyTransaction(buffer, tid)) {
    return false;
  }

  TRI_AppendCharStringBuffer(buffer, '}');

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
                                       TRI_col_info_t const* info) {
  TRI_json_t* json;

  if (! StringifyBasics(buffer, tick, "create")) {
    return false;
  }

  TRI_AppendStringStringBuffer(buffer, "\"collection\":");

  json = TRI_CreateJsonCollectionInfo(info);
  TRI_StringifyJson(buffer, json);

  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  TRI_AppendCharStringBuffer(buffer, '}');

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "drop collection" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyDropCollection (TRI_string_buffer_t* buffer,
                                     TRI_voc_tick_t tick,
                                     TRI_voc_cid_t cid) {
  
  if (! StringifyBasics(buffer, tick, "drop")) {
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

  if (! StringifyBasics(buffer, tick, "rename")) {
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
/// @brief stringify a document operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyDocumentOperation (TRI_string_buffer_t* buffer,
                                        TRI_document_collection_t* document,
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
/// @brief replicate a "begin transaction" operation
////////////////////////////////////////////////////////////////////////////////

TRI_string_buffer_t* TRI_BeginTransactionReplication (TRI_voc_tid_t tid) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyBeginTransaction(buffer, TRI_NewTickVocBase(), tid)) {
    printf("REPLICATION: %s\n\n", buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate an "abort transaction" operation
////////////////////////////////////////////////////////////////////////////////

TRI_string_buffer_t* TRI_AbortTransactionReplication (TRI_voc_tid_t tid) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyAbortTransaction(buffer, TRI_NewTickVocBase(), tid)) {
    printf("REPLICATION: %s\n\n", buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "commit transaction" operation
////////////////////////////////////////////////////////////////////////////////

TRI_string_buffer_t* TRI_CommitTransactionReplication (TRI_voc_tid_t tid) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyCommitTransaction(buffer, TRI_NewTickVocBase(), tid)) {
    printf("REPLICATION: %s\n\n", buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "create collection" operation
////////////////////////////////////////////////////////////////////////////////

TRI_string_buffer_t* TRI_CreateCollectionReplication (TRI_col_info_t const* info) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyCreateCollection(buffer, TRI_NewTickVocBase(), info)) {
    printf("REPLICATION: %s\n\n", buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "drop collection" operation
////////////////////////////////////////////////////////////////////////////////

TRI_string_buffer_t* TRI_DropCollectionReplication (TRI_voc_cid_t cid) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyDropCollection(buffer, TRI_NewTickVocBase(), cid)) {
    printf("REPLICATION: %s\n\n", buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "rename collection" operation
////////////////////////////////////////////////////////////////////////////////

TRI_string_buffer_t* TRI_RenameCollectionReplication (TRI_voc_cid_t cid,
                                                      char const* name) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyRenameCollection(buffer, TRI_NewTickVocBase(), cid, name)) {
    printf("REPLICATION: %s\n\n", buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a document operation
////////////////////////////////////////////////////////////////////////////////

TRI_string_buffer_t* TRI_DocumentReplication (TRI_document_collection_t* document,
                                              TRI_voc_document_operation_e type,
                                              TRI_df_marker_t const* marker) {
  TRI_string_buffer_t* buffer;
  
  buffer = TRI_CreateStringBuffer(TRI_CORE_MEM_ZONE);

  if (StringifyDocumentOperation(buffer, document, type, marker)) {
    printf("REPLICATION: %s\n\n", buffer->_buffer);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
