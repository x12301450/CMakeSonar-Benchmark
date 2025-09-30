#pragma once
#include "megdnn/basic_types.h"
#include "megdnn/opr_param_defs.h"

namespace megdnn {
namespace test {
namespace tqt {

struct TestArg {
    param::TQT param;
    TensorShape ishape;
    TensorShape scale_shape;
    TestArg(param::TQT param, TensorShape ishape, TensorShape scale_shape)
            : param(param), ishape(ishape), scale_shape(scale_shape) {}
};

inline std::vector<TestArg> get_args() {
    std::vector<TestArg> args;
    param::TQT cur_param;

    cur_param.qmin = -127;
    cur_param.qmax = 127;

    for (size_t i = 10; i < 30; i += 2) {
        args.emplace_back(cur_param, TensorShape{10, 64, i, i}, TensorShape{1});
    }

    return args;
}

}  // namespace tqt
}  // namespace test
}  // namespace megdnn
