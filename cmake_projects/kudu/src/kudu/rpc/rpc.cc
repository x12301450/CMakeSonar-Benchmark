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

#include "kudu/rpc/rpc.h"

#include <boost/bind.hpp>
#include <string>

#include "kudu/gutil/basictypes.h"
#include "kudu/gutil/strings/substitute.h"
#include "kudu/rpc/messenger.h"
#include "kudu/rpc/rpc_header.pb.h"

using std::tr1::shared_ptr;
using strings::Substitute;

namespace kudu {

namespace rpc {

bool RpcRetrier::HandleResponse(Rpc* rpc, Status* out_status) {
  ignore_result(DCHECK_NOTNULL(rpc));
  ignore_result(DCHECK_NOTNULL(out_status));

  // Always retry a TOO_BUSY error.
  Status controller_status = controller_.status();
  if (controller_status.IsRemoteError()) {
    const ErrorStatusPB* err = controller_.error_response();
    if (err &&
        err->has_code() &&
        err->code() == ErrorStatusPB::ERROR_SERVER_TOO_BUSY) {
      DelayedRetry(rpc);
      return true;
    }
  }

  *out_status = controller_status;
  return false;
}

void RpcRetrier::DelayedRetry(Rpc* rpc) {
  // Add some jitter to the retry delay.
  //
  // If the delay causes us to miss our deadline, RetryCb will fail the
  // RPC on our behalf.
  int num_ms = ++attempt_num_ + ((rand() % 5));
  messenger_->ScheduleOnReactor(boost::bind(&RpcRetrier::DelayedRetryCb,
                                            this,
                                            rpc, _1),
                                MonoDelta::FromMilliseconds(num_ms));
}

void RpcRetrier::DelayedRetryCb(Rpc* rpc, const Status& status) {
  Status new_status = status;
  if (new_status.ok()) {
    // Has this RPC timed out?
    if (deadline_.Initialized()) {
      MonoTime now = MonoTime::Now(MonoTime::FINE);
      if (deadline_.ComesBefore(now)) {
        new_status = Status::TimedOut(
            Substitute("$0 passed its deadline after $1 attempts",
                       rpc->ToString(), attempt_num_));
      }
    }
  }
  if (new_status.ok()) {
    controller_.Reset();
    rpc->SendRpc();
  } else {
    rpc->SendRpcCb(new_status);
  }
}

} // namespace rpc
} // namespace kudu
