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
#ifndef KUDU_UTIL_DEBUG_LEAKCHECK_DISABLER_H_
#define KUDU_UTIL_DEBUG_LEAKCHECK_DISABLER_H_

#include <gperftools/heap-checker.h>
#include "kudu/gutil/macros.h"
#include "kudu/util/debug/leak_annotations.h"

namespace kudu {
namespace debug {

// Scoped object that generically disables leak checking in a given scope,
// supporting both the tcmalloc heap leak checker and LSAN.
// While this object is alive, calls to "new" will not be checked for leaks.
class ScopedLeakCheckDisabler {
 public:
  ScopedLeakCheckDisabler() {}

 private:
#ifdef TCMALLOC_ENABLED
  HeapLeakChecker::Disabler hlc_disabler;
#endif

#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
  ScopedLSANDisabler lsan_disabler;
#  endif
#endif

  DISALLOW_COPY_AND_ASSIGN(ScopedLeakCheckDisabler);
};

} // namespace debug
} // namespace kudu

#endif // KUDU_UTIL_DEBUG_LEAKCHECK_DISABLER_H_
