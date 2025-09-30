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

#include "kudu/client/client-test-util.h"
#include "kudu/client/row_result.h"

#include <boost/foreach.hpp>
#include <vector>

#include "kudu/gutil/stl_util.h"
#include "kudu/util/test_util.h"

namespace kudu {
namespace client {

void LogSessionErrorsAndDie(const std::tr1::shared_ptr<KuduSession>& session,
                            const Status& s) {
  CHECK(!s.ok());
  std::vector<KuduError*> errors;
  ElementDeleter d(&errors);
  bool overflow;
  session->GetPendingErrors(&errors, &overflow);
  CHECK(!overflow);

  // Log only the first 10 errors.
  LOG(INFO) << errors.size() << " failed ops. First 10 errors follow";
  int i = 0;
  BOOST_FOREACH(const KuduError* e, errors) {
    if (i == 10) {
      break;
    }
    LOG(INFO) << "Op " << e->failed_op().ToString()
              << " had status " << e->status().ToString();
    i++;
  }
  CHECK_OK(s); // will fail
}

void ScanTableToStrings(KuduTable* table, vector<string>* row_strings) {
  row_strings->clear();
  KuduScanner scanner(table);
  ASSERT_OK(scanner.SetSelection(KuduClient::LEADER_ONLY));
  scanner.SetTimeoutMillis(60000);
  ScanToStrings(&scanner, row_strings);
}

void ScanToStrings(KuduScanner* scanner, vector<string>* row_strings) {
  ASSERT_OK(scanner->Open());
  vector<KuduRowResult> rows;
  while (scanner->HasMoreRows()) {
    ASSERT_OK(scanner->NextBatch(&rows));
    BOOST_FOREACH(const KuduRowResult& row, rows) {
      row_strings->push_back(row.ToString());
    }
  }
}

KuduSchema KuduSchemaFromSchema(const Schema& schema) {
  return KuduSchema(schema);
}

} // namespace client
} // namespace kudu
