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

#include "kudu/client/client_builder-internal.h"

namespace kudu {

namespace client {

KuduClientBuilder::Data::Data()
  : default_admin_operation_timeout_(MonoDelta::FromSeconds(10)),
    default_rpc_timeout_(MonoDelta::FromSeconds(5)) {
}

KuduClientBuilder::Data::~Data() {
}

} // namespace client
} // namespace kudu
