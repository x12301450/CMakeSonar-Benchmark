#include "test/common/fake_quant.h"
#include "megdnn/oprs.h"
#include "test/common/checker.h"
#include "test/cuda/fixture.h"

namespace megdnn {
namespace test {
using namespace fake_quant;
TEST_F(CUDA, FAKE_QUANT) {
    std::vector<TestArg> args = get_args();
    auto dtype = dtype::Float32();
    UniformFloatRNG rng(-1.0f, 1.0f);
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    UniformFloatWithValueRNG rng1 = UniformFloatWithValueRNG(-1.0f, 1.0f, 0.5f, nan);

    for (auto&& arg : args) {
        auto param = arg.param;
        auto ishape = arg.ishape;
        auto scale_shape = arg.scale_shape;
        auto zeropoint_shape = arg.zeropoint_shape;
        Checker<FakeQuantForward> checker(handle_cuda());
        checker.set_param(param)
                .set_dtype(0, dtype)
                .set_dtype(1, dtype)
                .set_dtype(2, dtype)
                .set_dtype(3, dtype)
                .execs(TensorShapeArray{ishape, scale_shape, zeropoint_shape, ishape});

        checker.set_allow_invalid_check(true);
        checker.set_rng(0, &rng1);
        checker.set_param(param)
                .set_dtype(0, dtype)
                .set_dtype(1, dtype)
                .set_dtype(2, dtype)
                .set_dtype(3, dtype)
                .execs(TensorShapeArray{ishape, scale_shape, zeropoint_shape, ishape});
        checker.set_rng(0, &rng);
        checker.set_allow_invalid_check(false);
    }
    // test noncontiguous layout
    for (auto&& arg : args) {
        auto param = arg.param;
        auto ishape = arg.ishape;
        auto scale_shape = arg.scale_shape;
        auto zeropoint_shape = arg.zeropoint_shape;
        Checker<FakeQuantForward> checker(handle_cuda());
        TensorLayout ilayout(
                ishape,
                {(long int)(ishape[1] * ishape[2] * ishape[3] * 2),
                 (long int)(ishape[2] * ishape[3]), (long int)ishape[3], 1},
                dtype::Float32());
        checker.set_param(param).execl(
                {ilayout,
                 {scale_shape, dtype::Float32()},
                 {zeropoint_shape, dtype::Float32()},
                 ilayout});

        checker.set_allow_invalid_check(true);
        checker.set_rng(0, &rng1);
        checker.set_param(param).execl(
                {ilayout,
                 {scale_shape, dtype::Float32()},
                 {zeropoint_shape, dtype::Float32()},
                 ilayout});
        checker.set_rng(0, &rng);
        checker.set_allow_invalid_check(false);
    }
}

TEST_F(CUDA, FAKE_QUANT_BACKWARD) {
    std::vector<TestArg> args = get_args();
    auto dtype = dtype::Float32();
    UniformFloatRNG rng(-1.0f, 1.0f);
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    UniformFloatWithValueRNG rng1 = UniformFloatWithValueRNG(-1.0f, 1.0f, 0.5f, nan);

    for (auto&& arg : args) {
        auto param = arg.param;
        auto ishape = arg.ishape;
        auto scale_shape = arg.scale_shape;
        auto zeropoint_shape = arg.zeropoint_shape;
        Checker<FakeQuantBackward> checker(handle_cuda());
        checker.set_param(param)
                .set_dtype(0, dtype)
                .set_dtype(1, dtype)
                .set_dtype(2, dtype)
                .set_dtype(3, dtype)
                .set_dtype(4, dtype)
                .execs(TensorShapeArray{
                        ishape, ishape, scale_shape, zeropoint_shape, ishape});

        checker.set_allow_invalid_check(true);
        checker.set_rng(0, &rng1);
        checker.set_param(param)
                .set_dtype(0, dtype)
                .set_dtype(1, dtype)
                .set_dtype(2, dtype)
                .set_dtype(3, dtype)
                .set_dtype(4, dtype)
                .execs(TensorShapeArray{
                        ishape, ishape, scale_shape, zeropoint_shape, ishape});
        checker.set_rng(0, &rng);
        checker.set_allow_invalid_check(false);
    }
    // test noncontiguous layout
    for (auto&& arg : args) {
        auto param = arg.param;
        auto ishape = arg.ishape;
        auto scale_shape = arg.scale_shape;
        auto zeropoint_shape = arg.zeropoint_shape;
        Checker<FakeQuantBackward> checker(handle_cuda());
        TensorLayout ilayout(
                ishape,
                {(long int)(ishape[1] * ishape[2] * ishape[3] * 2),
                 (long int)(ishape[2] * ishape[3]), (long int)ishape[3], 1},
                dtype::Float32());
        checker.set_param(param).execl(
                {ilayout,
                 ilayout,
                 {scale_shape, dtype::Float32()},
                 {zeropoint_shape, dtype::Float32()},
                 ilayout});

        checker.set_allow_invalid_check(true);
        checker.set_rng(0, &rng1);
        checker.set_param(param).execl(
                {ilayout,
                 ilayout,
                 {scale_shape, dtype::Float32()},
                 {zeropoint_shape, dtype::Float32()},
                 ilayout});
        checker.set_rng(0, &rng);
        checker.set_allow_invalid_check(false);
    }
}

}  // namespace test
}  // namespace megdnn