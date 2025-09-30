// Copyright 2014 Cloudera, Inc.
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

// This unit test belongs in gutil, but it depends on test_main which is
// part of util.
#include "kudu/gutil/map-util.h"

#include <gtest/gtest.h>
#include <map>

using std::map;

namespace kudu {

TEST(FloorTest, TestMapUtil) {
  map<int, int> my_map;

  ASSERT_EQ(NULL, FindFloorOrNull(my_map, 5));

  my_map[5] = 5;
  ASSERT_EQ(5, *FindFloorOrNull(my_map, 6));
  ASSERT_EQ(5, *FindFloorOrNull(my_map, 5));
  ASSERT_EQ(NULL, FindFloorOrNull(my_map, 4));

  my_map[1] = 1;
  ASSERT_EQ(5, *FindFloorOrNull(my_map, 6));
  ASSERT_EQ(5, *FindFloorOrNull(my_map, 5));
  ASSERT_EQ(1, *FindFloorOrNull(my_map, 4));
  ASSERT_EQ(1, *FindFloorOrNull(my_map, 1));
  ASSERT_EQ(NULL, FindFloorOrNull(my_map, 0));

}

} // namespace kudu
