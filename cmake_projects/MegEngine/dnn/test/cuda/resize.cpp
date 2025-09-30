#include "test/common/resize.h"
#include "src/common/cv/enums.h"
#include "test/common/benchmarker.h"
#include "test/common/checker.h"
#include "test/cuda/fixture.h"

namespace megdnn {
namespace test {
namespace resize {

TEST_F(CUDA, RESIZE_CV) {
    using namespace resize;
    std::vector<TestArg> args = get_cv_args();
    Checker<Resize> checker(handle_cuda());

    for (auto&& arg : args) {
        checker.set_param(arg.param)
                .set_dtype(0, dtype::Uint8())
                .set_dtype(1, dtype::Uint8())
                .set_epsilon(1)
                .set_max_avg_error(0.4)
                .execs({arg.src, arg.dst});
    }

    for (auto&& arg : args) {
        checker.set_param(arg.param)
                .set_dtype(0, dtype::Float32())
                .set_dtype(1, dtype::Float32())
                .set_epsilon(1e-3)
                .execs({arg.src, arg.dst});
    }
}

TEST_F(CUDA, RESIZE_FORWARD) {
    using namespace resize;
    IMode modes[] = {IMode::INTER_LINEAR, IMode::NEAREST, IMode::INTER_CUBIC};
    for (auto imode : modes) {
        std::vector<TestArg> args = get_args(imode);
        Checker<Resize> checker(handle_cuda());

        for (auto&& arg : args) {
            checker.set_param(arg.param)
                    .set_dtype(0, dtype::Uint8())
                    .set_dtype(1, dtype::Uint8())
                    .set_epsilon(1)
                    .execs({arg.src, arg.dst});
        }

        for (auto&& arg : args) {
            checker.set_param(arg.param)
                    .set_dtype(0, dtype::Float32())
                    .set_dtype(1, dtype::Float32())
                    .set_epsilon(1e-3)
                    .execs({arg.src, arg.dst});
        }

        for (auto&& arg : args) {
            checker.set_param(arg.param)
                    .set_dtype(0, dtype::Int8())
                    .set_dtype(1, dtype::Int8())
                    .set_epsilon(1)
                    .execs({arg.src, arg.dst});
        }

        for (auto&& arg : args) {
            checker.set_param(arg.param)
                    .set_dtype(0, dtype::Float16())
                    .set_dtype(1, dtype::Float16())
                    .set_epsilon(1e-3)
                    .execs({arg.src, arg.dst});
        }
    }
}

TEST_F(CUDA, RESIZE_NHWC) {
    using namespace resize;
    std::vector<TestArg> args;

    param::Resize param;
    param.format = param::Resize::Format::NHWC;
    param.imode = param::Resize::InterpolationMode::LINEAR;

    args.emplace_back(param, TensorShape{1, 1, 4, 5}, TensorShape{1, 1, 8, 5});
    args.emplace_back(param, TensorShape{2, 6, 4, 5}, TensorShape{2, 3, 8, 5});
    args.emplace_back(param, TensorShape{1, 2, 2, 2}, TensorShape{1, 4, 3, 2});

    Checker<ResizeBackward> checkerBackward(handle_cuda());

    for (auto&& arg : args) {
        checkerBackward.set_param(arg.param)
                .set_dtype(0, dtype::Float32())
                .set_dtype(1, dtype::Float32())
                .set_epsilon(1e-3)
                .execs({arg.src, arg.dst});
    }

    for (auto&& arg : args) {
        checkerBackward.set_param(arg.param)
                .set_dtype(0, dtype::Float16())
                .set_dtype(1, dtype::Float16())
                .set_epsilon(1e-3)
                .execs({arg.src, arg.dst});
    }

    Checker<ResizeForward> checkerForward(handle_cuda());
    for (auto&& arg : args) {
        checkerForward.set_param(arg.param)
                .set_dtype(0, dtype::Float16())
                .set_dtype(1, dtype::Float16())
                .set_epsilon(1e-3)
                .execs({arg.src, arg.dst});
    }
    for (auto&& arg : args) {
        checkerForward.set_param(arg.param)
                .set_dtype(0, dtype::Float32())
                .set_dtype(1, dtype::Float32())
                .set_epsilon(1e-3)
                .execs({arg.src, arg.dst});
    }
}

TEST_F(CUDA, RESIZE_NCHW4) {
    using namespace resize;
    Checker<Resize> checker(handle_cuda());
    auto args = get_nchw4_args();
    for (auto&& arg : args) {
        checker.set_param(arg.param)
                .set_dtype(0, dtype::QuantizedS8(0.1f))
                .set_dtype(1, dtype::QuantizedS8(0.1f))
                .set_epsilon(1 + 1e-3)
                .execs({arg.src, arg.dst});
    }
}

TEST_F(CUDA, RESIZE_NCHW_WITH_STRIDE) {
    IMode modes[] = {IMode::INTER_LINEAR, IMode::NEAREST, IMode::INTER_CUBIC};
    for (auto imode : modes) {
        param::Resize param;
        param.format = param::Resize::Format::NCHW;
        param.imode = imode;
        Checker<Resize> checker(handle_cuda());
        checker.set_epsilon(1 + 1e-3).set_param(param);

        auto run = [&](TensorShape src_shape, std::vector<ptrdiff_t> src_layout,
                       TensorShape dst_shape, DType dtype) {
            checker.set_dtype(0, dtype).set_dtype(1, dtype).execl(
                    {{src_shape, src_layout, dtype}, {dst_shape, dtype}});
        };

        for (DType& dtype :
             std::vector<DType>{dtype::Float32(), dtype::Uint8(), dtype::Int8()}) {
            run({2, 3, 4, 4}, {256, 32, 8, 1}, {2, 3, 3, 3}, dtype);
            run({1, 3, 4, 3}, {105, 35, 7, 2}, {1, 3, 5, 5}, dtype);
            run({1, 3, 40, 40}, {25600, 3200, 80, 1}, {1, 3, 30, 30}, dtype);
            run({2, 3, 4, 4}, {-256, 32, -8, 1}, {2, 3, 3, 3}, dtype);
            run({2, 3, 4, 4}, {256, -32, 8, -1}, {2, 3, 3, 3}, dtype);
            run({2, 3, 4, 4}, {-256, -32, -8, -1}, {2, 3, 3, 3}, dtype);
        }
    }
}

TEST_F(CUDA, RESIZE_BACKWARD) {
    IMode modes[] = {IMode::INTER_LINEAR, IMode::NEAREST, IMode::INTER_CUBIC};
    for (auto imode : modes) {
        Checker<ResizeBackward> checker(handle_cuda());
        param::Resize param;
        param.format = param::Resize::Format::NCHW;
        param.imode = imode;
        checker.set_param(param);
        checker.set_dtype(0, dtype::Float16());
        checker.set_dtype(1, dtype::Float16());
        checker.set_epsilon(1 + 1e-3);

        checker.execs({{2, 3, 4, 5}, {2, 3, 8, 9}});
        checker.execs({{2, 5, 8, 9}, {2, 5, 4, 5}});
        checker.execs({{2, 5, 8, 5}, {2, 5, 4, 9}});
        checker.execs({{2, 5, 4, 9}, {2, 5, 8, 5}});
    }

    for (auto imode : modes) {
        Checker<ResizeBackward> checker(handle_cuda());
        param::Resize param;
        param.format = param::Resize::Format::NCHW;
        param.imode = imode;
        checker.set_param(param);
        checker.set_dtype(0, dtype::Float32());
        checker.set_dtype(1, dtype::Float32());

        checker.execs({{2, 3, 4, 5}, {2, 3, 8, 9}});
        checker.execs({{2, 5, 8, 9}, {2, 5, 4, 5}});
        checker.execs({{2, 5, 8, 5}, {2, 5, 4, 9}});
        checker.execs({{2, 5, 4, 9}, {2, 5, 8, 5}});
    }
}

TEST_F(CUDA, RESIZE3D_NCDHW) {
    using IMode = param::Resize3D::InterpolationMode;
    using Format = param::Resize3D::Format;
    auto ac_param = param::Resize3D{IMode::LINEAR, Format::NCDHW, true};
    auto nac_param = param::Resize3D{IMode::LINEAR, Format::NCDHW, false};

    auto run = [&](DType d, TensorShape ishape, TensorShape oshape) {
        Checker<Resize3D> checker(handle_cuda());
        checker.set_param(ac_param).set_dtype(0, d).set_dtype(1, d).execs(
                {ishape, oshape});
        checker.set_param(nac_param).execs({ishape, oshape});
    };

    for (auto&& dtype : std::vector<DType>{dtype::Float32(), dtype::Float16()}) {
        run(dtype, {1, 1, 2, 2, 2}, {1, 1, 4, 4, 4});
        run(dtype, {2, 2, 2, 3, 4}, {2, 2, 2, 3, 6});
        run(dtype, {2, 2, 2, 3, 4}, {2, 2, 3, 4, 5});
    }
}

#if MEGDNN_WITH_BENCHMARK

TEST_F(CUDA, BENCHMARK_RESIZE_CV) {
    Benchmarker<Resize> benchmarker(handle_cuda());
    param::Resize param;
    param.format = param::Resize::Format::NHWC;
    param.imode = param::Resize::InterpolationMode::LANCZOS4;
    benchmarker.set_param(param);
    benchmarker.set_display(false);

    auto run = [&benchmarker](const TensorShape& src, const TensorShape& dst) {
        auto used = benchmarker.execs({src, dst});

        //! bandwith: each dst elem require 4 read and 1 write
        //! gflops: each dst elem require 4 mul + 3 add
        printf("run %s->%s used: %f ms %f GBPS %f Gflops\n", src.to_string().c_str(),
               dst.to_string().c_str(), used,
               dst.total_nr_elems() * (4.f + 1.f) * sizeof(float) /
                       (1024 * 1024 * 1024) / used * 1e3,
               dst.total_nr_elems() * (4.f + 3.f) / (1024 * 1024 * 1024) / used * 1e3);
    };

    run({1, 128, 128, 3}, {1, 256, 256, 3});
}

TEST_F(CUDA, BENCHMARK_RESIZE_FORWARD) {
    Benchmarker<Resize> benchmarker(handle_cuda());
    param::Resize param;
    param.format = param::Resize::Format::NCHW;
    param.imode = param::Resize::InterpolationMode::LINEAR;
    benchmarker.set_param(param);
    benchmarker.set_display(false);

    auto run = [&benchmarker](const TensorShape& src, const TensorShape& dst) {
        auto used = benchmarker.execs({src, dst});

        //! bandwith: each dst elem require 4 read and 1 write
        //! gflops: each dst elem require 4 mul + 3 add
        printf("run %s->%s used: %f ms %f GBPS %f Gflops\n", src.to_string().c_str(),
               dst.to_string().c_str(), used,
               dst.total_nr_elems() * (4.f + 1.f) * sizeof(float) /
                       (1024 * 1024 * 1024) / used * 1e3,
               dst.total_nr_elems() * (4.f + 3.f) / (1024 * 1024 * 1024) / used * 1e3);
    };

    run({1, 100, 256, 256}, {1, 100, 256, 5120});
    run({1, 100, 256, 5120}, {1, 100, 256, 256});
    run({1, 100, 256, 256}, {1, 100, 512, 512});
    run({1, 100, 512, 512}, {1, 100, 256, 256});
}

TEST_F(CUDA, BENCHMARK_RESIZE_FORWARD_NCHW4) {
    Benchmarker<Resize> benchmarker(handle_cuda());
    param::Resize param;
    param.imode = param::Resize::InterpolationMode::LINEAR;
    benchmarker.set_display(false);

    auto run = [&benchmarker](const TensorShape& src, const TensorShape& dst) {
        auto used = benchmarker.execs({src, dst});

        //! bandwith: each dst elem require 4 read and 1 write
        //! gflops: each dst elem require 4 mul + 3 add
        printf("run %s->%s used: %f ms %f GBPS %f Gflops\n", src.to_string().c_str(),
               dst.to_string().c_str(), used,
               dst.total_nr_elems() * (4.f + 1.f) / (1024 * 1024 * 1024) / used * 1e3,
               dst.total_nr_elems() * (4.f + 3.f) / (1024 * 1024 * 1024) / used * 1e3);
    };
    param.format = param::Resize::Format::NCHW;
    benchmarker.set_param(param);
    benchmarker.set_dtype(0, dtype::Int8());
    benchmarker.set_dtype(1, dtype::Int8());
    run({1, 100, 256, 256}, {1, 100, 256, 5120});
    run({1, 100, 256, 5120}, {1, 100, 256, 256});
    run({1, 100, 256, 256}, {1, 100, 512, 512});
    run({1, 100, 512, 512}, {1, 100, 256, 256});

    param.format = param::Resize::Format::NCHW4;
    benchmarker.set_param(param);
    benchmarker.set_dtype(0, dtype::QuantizedS8(1.0f));
    benchmarker.set_dtype(1, dtype::QuantizedS8(1.0f));
    run({1, 25, 256, 256, 4}, {1, 25, 256, 5120, 4});
    run({1, 25, 256, 5120, 4}, {1, 25, 256, 256, 4});
    run({1, 25, 256, 256, 4}, {1, 25, 512, 512, 4});
    run({1, 25, 512, 512, 4}, {1, 25, 256, 256, 4});
}

TEST_F(CUDA, BENCHMARK_RESIZE_BACKWARD) {
    Benchmarker<ResizeBackward> benchmarker(handle_cuda());
    param::Resize param;
    param.format = param::Resize::Format::NCHW;
    param.imode = param::Resize::InterpolationMode::LINEAR;
    benchmarker.set_param(param);
    benchmarker.set_display(false);
    const size_t RUNS = 5;
    benchmarker.set_times(RUNS);

    auto run = [&benchmarker](const TensorShape& diff, const TensorShape& grad) {
        auto used = benchmarker.execs({diff, grad});
        used /= RUNS;

        //! bandwith: each dst elem require 1 read and 4 write
        //! gflops: each dst elem require 4 add
        printf("run %s<-%s used: %f ms %f GBPS %f Gflops\n", diff.to_string().c_str(),
               grad.to_string().c_str(), used,
               diff.total_nr_elems() * (4.f + 1.f) * sizeof(float) /
                       (1024 * 1024 * 1024) / used * 1e3,
               diff.total_nr_elems() * 4.f / (1024 * 1024 * 1024) / used * 1e3);
    };

    run({1, 100, 256, 256}, {1, 100, 256, 5120});
    run({1, 100, 256, 5120}, {1, 100, 256, 256});
    run({1, 100, 256, 256}, {1, 100, 512, 512});
    run({1, 100, 512, 512}, {1, 100, 256, 256});
}

#endif

}  // namespace resize
}  // namespace test
}  // namespace megdnn

// vim: syntax=cpp.doxygen
