////////////////////////////////////////////////////////////////////////////////
/// @brief conversion functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_C_CONVERSIONS_H
#define TRIAGENS_BASICS_C_CONVERSIONS_H 1

#include "BasicsC/common.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                          public functions for string to something
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Conversions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a single hex to an integer
////////////////////////////////////////////////////////////////////////////////

int TRI_IntHex (char ch, int errorValue);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to boolean from string
////////////////////////////////////////////////////////////////////////////////

bool TRI_BooleanString (char const* str);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to double from string
////////////////////////////////////////////////////////////////////////////////

double TRI_DoubleString (char const* str);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to int32 from string
////////////////////////////////////////////////////////////////////////////////

int32_t TRI_Int32String (char const* str);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to int32 from string with given length
////////////////////////////////////////////////////////////////////////////////

int32_t TRI_Int32String2 (char const* str, size_t length);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to uint32 from string
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_UInt32String (char const* str);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to uint32 from string with given length
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_UInt32String2 (char const* str, size_t length);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to int64 from string
////////////////////////////////////////////////////////////////////////////////

int64_t TRI_Int64String (char const* str);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to int64 from string with given length
////////////////////////////////////////////////////////////////////////////////

int64_t TRI_Int64String2 (char const* str, size_t length);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to uint64 from string
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_UInt64String (char const* str);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to uint64 from string with given length
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_UInt64String2 (char const* str, size_t length);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                             public functions for number to string
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Conversions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from int32
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringInt32 (int32_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from uint32
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringUInt32 (uint32_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from int64
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringInt64 (int64_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from uint64
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringUInt64 (uint64_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from double
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringDouble (double);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
