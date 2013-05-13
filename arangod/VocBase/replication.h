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

#ifndef TRIAGENS_VOC_BASE_REPLICATION_H
#define TRIAGENS_VOC_BASE_REPLICATION_H 1

#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------


struct TRI_col_info_s;
struct TRI_df_marker_s;
struct TRI_document_collection_s;
struct TRI_string_buffer_s;
struct TRI_vocbase_col_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                       REPLICATION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "begin transaction" operation
////////////////////////////////////////////////////////////////////////////////

struct TRI_string_buffer_s* TRI_BeginTransactionReplication (TRI_voc_tid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "abort transaction" operation
////////////////////////////////////////////////////////////////////////////////

struct TRI_string_buffer_s* TRI_AbortTransactionReplication (TRI_voc_tid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "commit transaction" operation
////////////////////////////////////////////////////////////////////////////////

struct TRI_string_buffer_s* TRI_CommitTransactionReplication (TRI_voc_tid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "create collection" operation
////////////////////////////////////////////////////////////////////////////////

struct TRI_string_buffer_s* TRI_CreateCollectionReplication (struct TRI_col_info_s const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "drop collection" operation
////////////////////////////////////////////////////////////////////////////////

struct TRI_string_buffer_s* TRI_DropCollectionReplication (TRI_voc_cid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "rename collection" operation
////////////////////////////////////////////////////////////////////////////////

struct TRI_string_buffer_s* TRI_RenameCollectionReplication (TRI_voc_cid_t,
                                                             char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a document operation
////////////////////////////////////////////////////////////////////////////////

struct TRI_string_buffer_s* TRI_DocumentReplication (struct TRI_document_collection_s*,
                                                     TRI_voc_document_operation_e,
                                                     struct TRI_df_marker_s const*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
