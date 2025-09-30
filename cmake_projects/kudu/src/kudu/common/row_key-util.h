// Copyright 2015 Cloudera, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Utility functions for working with the primary key portion of a row.
#ifndef KUDU_COMMON_ROW_KEY_UTIL_H
#define KUDU_COMMON_ROW_KEY_UTIL_H

#include "kudu/gutil/port.h"

namespace kudu {

class Arena;
class ContiguousRow;

namespace row_key_util {

// Set all of the parts of the key in 'row' to the minimum legal values
// for their types.
//
// For example:
// - signed ints become very large negative values
// - unsigned ints become 0
// - strings become ""
void SetKeyToMinValues(ContiguousRow* row);

// Increment the primary key of this row to the smallest key which is greater
// than the current key.
//
// For example, for a composite key with types (int8, int8), incrementing
// the row (1, 1) will result in (1, 2). Incrementing (1, 127) will result
// in (2, -128).
//
// Note that not all keys may be incremented without overflow. For example,
// if the primary key is an int8, and the key is already set to '127',
// incrementing would overflow. In this case, the value is incremented and
// overflowed, but the function returns 'false' to indicate the overflow
// condition. Otherwise, returns 'true'.
//
// String types are increment by appending a '\0' byte to the end. Since our
// strings have unbounded length, this implies that if a key has a string
// component, it will always be incremented.
//
// For the case of incrementing string types, we allocate a new copy of the
// string from 'arena', which must be non-NULL.
//
// REQUIRES: all key columns must be valid.
bool IncrementKey(ContiguousRow* row, Arena* arena) WARN_UNUSED_RESULT;

// The same as the above function, but only acts on a prefix of the primary
// key.
//
// For example, for a composite primary key (int8, int8, int8) with value
// (1,2,3), IncrementKeyPrefix(2) will return (1,3,3).
bool IncrementKeyPrefix(ContiguousRow* row, int prefix_len,
                        Arena* arena) WARN_UNUSED_RESULT;

} // namespace row_key_util
} // namespace kudu
#endif /* KUDU_COMMON_ROW_KEY_UTIL_H */
