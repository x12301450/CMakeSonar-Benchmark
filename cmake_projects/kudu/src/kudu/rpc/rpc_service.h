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
#ifndef KUDU_RPC_SERVICE_H_
#define KUDU_RPC_SERVICE_H_

#include "kudu/gutil/ref_counted.h"
#include "kudu/util/status.h"

namespace kudu {
namespace rpc {

class InboundCall;

class RpcService : public RefCountedThreadSafe<RpcService> {
 public:
  virtual ~RpcService() {}

  // Enqueue a call for processing.
  // On failure, the RpcService::QueueInboundCall() implementation is
  // responsible for responding to the client with a failure message.
  virtual Status QueueInboundCall(gscoped_ptr<InboundCall> call) = 0;
};

} // namespace rpc
} // namespace kudu

#endif // KUDU_RPC_SERVICE_H_
