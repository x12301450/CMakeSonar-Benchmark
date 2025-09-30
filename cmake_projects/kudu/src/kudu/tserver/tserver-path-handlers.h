// Copyright 2013 Cloudera, Inc.
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
#ifndef KUDU_TSERVER_TSERVER_PATH_HANDLERS_H
#define KUDU_TSERVER_TSERVER_PATH_HANDLERS_H

#include "kudu/gutil/macros.h"
#include "kudu/server/webserver.h"
#include <string>
#include <sstream>
#include <vector>

namespace kudu {

class Schema;
struct IteratorStats;

namespace consensus {
class ConsensusStatePB;
} // namespace consensus

namespace tserver {

class TabletServer;
class Scanner;

class TabletServerPathHandlers {
 public:
  explicit TabletServerPathHandlers(TabletServer* tserver)
    : tserver_(tserver) {
  }

  ~TabletServerPathHandlers();

  Status Register(Webserver* server);

 private:
  void HandleScansPage(const Webserver::WebRequest& req,
                       std::stringstream* output);
  void HandleTabletsPage(const Webserver::WebRequest& req,
                         std::stringstream* output);
  void HandleTabletPage(const Webserver::WebRequest& req,
                        std::stringstream* output);
  void HandleTransactionsPage(const Webserver::WebRequest& req,
                              std::stringstream* output);
  void HandleTabletSVGPage(const Webserver::WebRequest& req,
                           std::stringstream* output);
  void HandleLogAnchorsPage(const Webserver::WebRequest& req,
                            std::stringstream* output);
  void HandleConsensusStatusPage(const Webserver::WebRequest& req,
                                 std::stringstream* output);
  void HandleDashboardsPage(const Webserver::WebRequest& req,
                            std::stringstream* output);
  void HandleMaintenanceManagerPage(const Webserver::WebRequest& req,
                                    std::stringstream* output);
  std::string ConsensusStatePBToHtml(const consensus::ConsensusStatePB& cstate) const;
  std::string ScannerToHtml(const Scanner& scanner) const;
  std::string IteratorStatsToHtml(const Schema& projection,
                                  const std::vector<IteratorStats>& stats) const;
  std::string GetDashboardLine(const std::string& link,
                               const std::string& text, const std::string& desc);

  TabletServer* tserver_;

  DISALLOW_COPY_AND_ASSIGN(TabletServerPathHandlers);
};

} // namespace tserver
} // namespace kudu
#endif /* KUDU_TSERVER_TSERVER_PATH_HANDLERS_H */
