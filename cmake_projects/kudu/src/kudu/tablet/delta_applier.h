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
#ifndef KUDU_TABLET_DELTA_APPLIER_H
#define KUDU_TABLET_DELTA_APPLIER_H

#include <string>
#include <tr1/memory>
#include <vector>

#include <gtest/gtest_prod.h>

#include "kudu/common/iterator.h"
#include "kudu/common/schema.h"
#include "kudu/gutil/macros.h"
#include "kudu/util/status.h"
#include "kudu/tablet/cfile_set.h"

namespace kudu {
namespace tablet {

class DeltaIterator;

////////////////////////////////////////////////////////////
// Delta-applying iterators
////////////////////////////////////////////////////////////

// A DeltaApplier takes in a base ColumnwiseIterator along with a a
// DeltaIterator. It is responsible for applying the updates coming
// from the delta iterator to the results of the base iterator.
class DeltaApplier : public ColumnwiseIterator {
 public:
  virtual Status Init(ScanSpec *spec) OVERRIDE;
  Status PrepareBatch(size_t *nrows) OVERRIDE;

  Status FinishBatch() OVERRIDE;

  bool HasNext() const OVERRIDE;

  std::string ToString() const OVERRIDE;

  const Schema &schema() const OVERRIDE;

  virtual void GetIteratorStats(std::vector<IteratorStats>* stats) const OVERRIDE;

  // Initialize the selection vector for the current batch.
  // This processes DELETEs -- any deleted rows are set to 0 in 'sel_vec'.
  // All other rows are set to 1.
  virtual Status InitializeSelectionVector(SelectionVector *sel_vec) OVERRIDE;

  Status MaterializeColumn(size_t col_idx, ColumnBlock *dst) OVERRIDE;
 private:
  friend class DeltaTracker;

  FRIEND_TEST(TestMajorDeltaCompaction, TestCompact);

  DISALLOW_COPY_AND_ASSIGN(DeltaApplier);

  // Construct. The base_iter and delta_iter should not be Initted.
  DeltaApplier(const std::tr1::shared_ptr<CFileSet::Iterator>& base_iter,
               const std::tr1::shared_ptr<DeltaIterator>& delta_iter);
  virtual ~DeltaApplier();

  std::tr1::shared_ptr<CFileSet::Iterator> base_iter_;
  std::tr1::shared_ptr<DeltaIterator> delta_iter_;

  bool first_prepare_;
};

} // namespace tablet
} // namespace kudu
#endif /* KUDU_TABLET_DELTA_APPLIER_H */
