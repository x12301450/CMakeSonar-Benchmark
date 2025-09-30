#pragma once
#include <cstddef>
#include "megdnn/basic_types.h"
#include "megdnn/opr_param_defs.h"

namespace megdnn {
namespace test {
namespace images2neibs {

struct TestArg {
    param::Images2Neibs param;
    TensorShape ishape;
    TestArg(param::Images2Neibs param, TensorShape ishape)
            : param(param), ishape(ishape) {}
};

inline std::vector<TestArg> get_args() {
    std::vector<TestArg> args;

    // clang-format off
        for (uint32_t ph : {0, 1})
        for (uint32_t pw : {0, 1})
        for (uint32_t sh : {1, 2})
        for (uint32_t sw : {1, 2})
        for (uint32_t dh : {1, 2, 3})
        for (uint32_t dw : {1, 2, 3})
        for (uint32_t wh : {3, 4})
        for (uint32_t ww : {3, 4}) {
            args.emplace_back(param::Images2Neibs{ph, pw, sh, sw, dh, dw, wh, ww},
                              TensorShape{2, 3, 19, 20});
        }
    // clang-format on
    // large window case
    args.emplace_back(
            param::Images2Neibs{0, 0, 1, 1, 1, 1, 32, 64}, TensorShape{2, 3, 96, 128});
    // large size
    args.emplace_back(
            param::Images2Neibs{0, 0, 1, 1, 1, 1, 1, 1}, TensorShape{128, 128, 28, 24});

    return args;
}

inline std::vector<TestArg> get_cd4_args() {
    std::vector<TestArg> args;
    // clang-format off
    for (uint32_t ph : {0, 1})
    for (uint32_t pw : {0, 1})
    for (uint32_t sh : {1, 2})
    for (uint32_t sw : {1, 2})
    for (uint32_t dh : {1, 2, 3})
    for (uint32_t dw : {1, 2, 3})
    for (uint32_t wh : {2, 3})
    for (uint32_t ww : {2, 3}) {
            args.emplace_back(param::Images2Neibs{ph, pw, sh, sw, dh, dw, wh,
            ww},
                            TensorShape{2, 13, 1, 14, 4});
    }

    // clang-format on
    // large window case
    args.emplace_back(
            param::Images2Neibs{0, 0, 1, 1, 1, 1, 8, 14}, TensorShape{2, 16, 1, 16, 4});
    // large size
    args.emplace_back(
            param::Images2Neibs{0, 0, 1, 1, 1, 1, 1, 1},
            TensorShape{256, 16, 64, 16, 4});

    return args;
}

inline std::vector<TestArg> get_benchmark_args() {
    std::vector<TestArg> args;
    // clang-format off
    for (uint32_t ph : {0, 1})
    for (uint32_t pw : {0, 1})
    for (uint32_t sh : {1, 2})
    for (uint32_t sw : {1, 2})
    for (uint32_t dh : {1, 2})
    for (uint32_t dw : {1, 2})
    for (uint32_t wh : {3, 4})
    for (uint32_t ww : {3, 4})
    for (uint32_t b : {1, 64})
    for (uint32_t c : {64, 128})
    for (uint32_t hw : {64, 128}) {
        args.emplace_back(param::Images2Neibs{ph, pw, sh, sw, dh, dw, wh, ww},
                          TensorShape{b, c, hw, hw});
    }
    // clang-format on
    // large size
    args.emplace_back(
            param::Images2Neibs{0, 0, 1, 1, 1, 1, 1, 1},
            TensorShape{1024, 128, 28, 24});

    return args;
}

inline std::vector<TestArg> get_benchmark_args_cd4() {
    std::vector<TestArg> args;
    // clang-format off
    for (uint32_t ph : {0, 1})
    for (uint32_t pw : {0, 1})
    for (uint32_t sh : {1, 2})
    for (uint32_t sw : {1, 2})
    for (uint32_t dh : {1, 2})
    for (uint32_t dw : {1, 2})
    for (uint32_t wh : {3, 4})
    for (uint32_t ww : {3, 4})
    for (uint32_t b : {1, 32})
    for (uint32_t c : {16, 32})
    for (uint32_t hw : {16, 32}) {
        args.emplace_back(param::Images2Neibs{ph, pw, sh, sw, dh, dw, wh, ww},
                          TensorShape{b, hw, (c + 3) / 4, hw, 4});
    }

    // clang-format on
    // large size
    args.emplace_back(
            param::Images2Neibs{0, 0, 1, 1, 1, 1, 1, 1},
            TensorShape{256, 28, 32, 24, 4});

    return args;
}

}  // namespace images2neibs
}  // namespace test
}  // namespace megdnn

// vim: syntax=cpp.doxygen
