#include "test/arm_common/fixture.h"

#include "test/common/benchmarker.h"
#include "test/common/checker.h"
#include "test/common/group_local.h"
#include "test/common/task_record_check.h"
#include "test/common/timer.h"

namespace megdnn {
namespace test {
using Param = param::Convolution;

TEST_F(ARM_COMMON, GROUP_LOCAL_FORWARD) {
    auto args = group_local::get_args();
    Checker<GroupLocalForward> checker(handle());
    for (auto&& arg : args) {
        checker.set_param(arg.param).execs({arg.sshape(), arg.fshape(), arg.dshape()});
    }

    NormalRNG rng(10.f);
    checker.set_rng(0, &rng).set_rng(1, &rng);
    args = group_local::get_args_for_fp16();

#if __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
    for (auto&& arg : args) {
        checker.set_dtype(0, dtype::Float16())
                .set_dtype(1, dtype::Float16())
                .set_dtype(2, dtype::Float16());
        checker.set_epsilon(1e-2);
        checker.set_param(arg.param).execs({arg.sshape(), arg.fshape(), arg.dshape()});
    }
#endif
}

TEST_F(ARM_COMMON, GROUP_LOCAL_FORWARD_RECORD) {
    auto args = group_local::get_args();
    TaskRecordChecker<GroupLocalForward> checker(0);
    for (auto&& arg : args) {
        checker.set_param(arg.param).execs({arg.sshape(), arg.fshape(), arg.dshape()});
    }

    NormalRNG rng(10.f);
    checker.set_rng(0, &rng).set_rng(1, &rng);
    args = group_local::get_args_for_fp16();

#if __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
    for (auto&& arg : args) {
        checker.set_dtype(0, dtype::Float16())
                .set_dtype(1, dtype::Float16())
                .set_dtype(2, dtype::Float16());
        checker.set_epsilon(1e-2);
        checker.set_param(arg.param).execs({arg.sshape(), arg.fshape(), arg.dshape()});
    }
#endif
}

}  // namespace test
}  // namespace megdnn
// vim: syntax=cpp.doxygen
