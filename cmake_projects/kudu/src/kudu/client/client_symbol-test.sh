#!/bin/bash
# Copyright 2014 Cloudera, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Tests that the dynamic symbols visible in the public client library
# (i.e. those available for runtime linking) are all approved Kudu symbols.

NM=`which nm`
if [ -n $NM ]; then
  echo "Found nm: $NM"
else
  echo "Cannot find nm on PATH: $PATH"
  exit 1
fi

ROOT=$(readlink -f $(dirname "$BASH_SOURCE"))/../../..
LIB=$ROOT/build/latest/exported/libkudu_client.so
if [ -r $LIB ]; then
  echo "Found kudu client library: $LIB"
else
  echo "Can't read kudu client library at $LIB"
  exit 1
fi

NUM_BAD_SYMS=0
while read ADDR TYPE SYMBOL; do
  # Skip all symbols that aren't strong and global.
  if [ "$TYPE" != "T" ]; then
    echo "Skipping non-strong and non-global symbol '$SYMBOL'"
    continue
  fi

  # Skip special symbols.
  if [ "$SYMBOL" = "_init" -o "$SYMBOL" = "_fini" ]; then
    echo "Skipping special symbol '$SYMBOL'"
    continue
  fi

  # Skip Kudu symbols. Using [[ ]] for regex support.
  if [[ "$SYMBOL" =~ ^kudu:: ]]; then
    echo "Skipping kudu symbol '$SYMBOL'"
    continue;
  fi

  # KUDU-455: skip bizarro global symbol that remains when compiling with old gcc.
  if [ "$SYMBOL" = "__gnu_cxx::hash<StringPiece>::operator()(StringPiece) const" ]; then
    echo "Skipping KUDU-455 symbol '$SYMBOL'"
    continue
  fi

  # Any left over symbol is bad.
  echo "Found bad symbol '$SYMBOL'"
  NUM_BAD_SYMS=$((NUM_BAD_SYMS + 1))
done < <($NM -D --defined-only --demangle $LIB)

if [ $NUM_BAD_SYMS -gt 0 ]; then
  echo "Kudu client library contains $NUM_BAD_SYMS bad symbols"
  exit 1
fi

exit 0
