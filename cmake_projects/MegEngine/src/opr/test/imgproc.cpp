#include "megbrain/opr/imgproc.h"
#include "dnn/legacy_checker.h"
#include "megbrain/opr/basic_arith.h"
#include "megbrain/opr/tensor_manip.h"
#include "megbrain/test/autocheck.h"
#include "megbrain/test/helper.h"
#include "megbrain/test/megdnn_helper.h"

#include <megdnn.h>

#include <cmath>
#include <iomanip>
#include <random>
#include <sstream>

using namespace mgb;

namespace {
megdnn::thin_function<void(HostTensorND&)> warp_perspective_mat_gen(
        size_t N, size_t INP_H, size_t INP_W) {
    static std::mt19937 rng(next_rand_seed());
    auto rand_real = [&](double lo, double hi) {
        return rng() / (std::mt19937::max() + 1.0) * (hi - lo) + lo;
    };
    auto rand_real2 = [&](double range) { return rand_real(-range, range); };
    auto gen = [N, INP_H, INP_W, rand_real, rand_real2](HostTensorND& mat) {
        auto ptr = mat.ptr<float>();
        for (size_t i = 0; i < N; ++i) {
            auto rot = rand_real(0, M_PI * 2), scale = rand_real(0.8, 1.2),
                 sheer = rand_real(0.9, 1.1), dy = rand_real2(INP_H * 0.5),
                 dx = rand_real2(INP_W * 0.5), ky = rand_real2(0.1 / INP_H),
                 kx = rand_real2(0.1 / INP_W), kb = rand_real2(0.1) + 1;
            ptr[0] = ptr[4] = cos(rot) * scale;
            ptr[1] = -(ptr[3] = sin(rot) * scale);
            ptr[3] *= sheer;
            ptr[4] *= sheer;
            ptr[2] = dx;
            ptr[5] = dy;
            ptr[6] = kx;
            ptr[7] = ky;
            ptr[8] = kb;
            ptr += 9;
        }
        mgb_assert(ptr == mat.ptr<float>() + mat.shape().total_nr_elems());
    };
    return gen;
}
}  // namespace

TEST(TestOprImgproc, WarpPerspective) {
    set_rand_seed(20190813);  // a seed that can pass the test
    constexpr size_t INP_H = 6, INP_W = 4, N = 2, C = 3;
    using Checker = AutoOprChecker<2, 1>;
    TensorShape out_shp{N, C, 9, 10};
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        return {opr::WarpPerspective::make(
                inputs[0], inputs[1], TensorShape{out_shp.shape[2], out_shp.shape[3]})};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        auto opr = megdnn_naive_handle()->create_operator<megdnn::WarpPerspective>();
        dest[0].resize(out_shp);
        opr->exec(inp[0]->as_megdnn(), inp[1]->as_megdnn(), dest[0].as_megdnn(), {});
    };
    auto dump_mat = [&](const Checker::NumInpArray& inp) -> std::string {
        std::ostringstream ostr;
        ostr << std::setprecision(3);
        auto&& mat = *inp[1];
        mgb_assert(mat.shape().ndim == 3);
        auto ptr = mat.ptr<float>();
        for (size_t n = 0; n < mat.shape().shape[0]; ++n) {
            ostr << "mat " << n << ":\n";
            for (size_t i = 0; i < 3; ++i) {
                for (size_t j = 0; j < 3; ++j) {
                    ostr << std::setw(10) << *(ptr++);
                }
                ostr << '\n';
            }
        }
        return ostr.str();
    };
    Checker::RunOptions opt;
    opt.numdiff_eps_single_inp[1] = 1e-5;
    opt.numdiff_max_err_single_inp[1] = 0.5;
    Checker(make_graph, fwd)
            .set_input_generator(1, warp_perspective_mat_gen(N, INP_H, INP_W))
            .set_input_dump_on_error(dump_mat)
            .run({TensorShape{N, C, 4, 5}, {N, 3, 3}}, opt)
            .run({TensorShape{N, C, 6, 5}, {N, 3, 3}}, opt)
            .run({TensorShape{N, C, 10, 9}, {N, 3, 3}}, opt);
}

TEST(TestOprImgproc, WarpPerspective_MultiSrc) {
    set_rand_seed(20220801);  // a seed that can pass the test
    constexpr size_t INP_H = 6, INP_W = 4, N = 3, C = 3;
    using Checker = AutoOprChecker<4, 1>;
    TensorShape out_shp{N, C, 9, 10};
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        SymbolVarArray srcs;
        for (size_t i = 0; i < N; i++) {
            srcs.push_back(inputs[i]);
        }
        return {opr::WarpPerspective::make(
                srcs, inputs[N], TensorShape{out_shp.shape[2], out_shp.shape[3]})};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        auto opr = megdnn_naive_handle()->create_operator<megdnn::WarpPerspective>();
        dest[0].resize(out_shp);
        megdnn::TensorNDArray srcs;
        for (size_t i = 0; i < N; i++) {
            srcs.push_back(inp[i]->as_megdnn());
        }
        opr->exec(srcs, inp[N]->as_megdnn(), dest[0].as_megdnn(), {});
    };
    auto dump_mat = [&](const Checker::NumInpArray& inp) -> std::string {
        std::ostringstream ostr;
        ostr << std::setprecision(3);
        auto&& mat = *inp[N];
        mgb_assert(mat.shape().ndim == 3);
        auto ptr = mat.ptr<float>();
        for (size_t n = 0; n < mat.shape().shape[0]; ++n) {
            ostr << "mat " << n << ":\n";
            for (size_t i = 0; i < 3; ++i) {
                for (size_t j = 0; j < 3; ++j) {
                    ostr << std::setw(10) << *(ptr++);
                }
                ostr << '\n';
            }
        }
        return ostr.str();
    };
    Checker::RunOptions opt;
    opt.numdiff_eps_single_inp[1] = 1e-5;
    opt.numdiff_max_err_single_inp[1] = 0.5;
    Checker(make_graph, fwd)
            .set_input_generator(N, warp_perspective_mat_gen(N, INP_H, INP_W))
            .set_input_dump_on_error(dump_mat)
            .disable_grad_check()
            .run({TensorShape{1, C, 10, 9}, {1, C, 10, 9}, {1, C, 10, 9}, {N, 3, 3}},
                 opt)
            .run({TensorShape{1, C, 4, 5}, {1, C, 4, 5}, {1, C, 4, 5}, {N, 3, 3}}, opt)
            .run({TensorShape{1, C, 6, 5}, {1, C, 6, 5}, {1, C, 6, 5}, {N, 3, 3}}, opt);
}

TEST(TestOprImgproc, WarpPerspective_MultiSrc_NHWC) {
    set_rand_seed(20220801);  // a seed that can pass the test
    opr::WarpPerspective::Param param;
    param.format = opr::WarpPerspective::Param::Format::NHWC;
    constexpr size_t INP_H = 6, INP_W = 4, N = 3, C = 3;
    using Checker = AutoOprChecker<4, 1>;
    TensorShape out_shp{N, 9, 10, C};
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        SymbolVarArray srcs;
        for (size_t i = 0; i < N; i++) {
            srcs.push_back(inputs[i]);
        }
        return {opr::WarpPerspective::make(
                srcs, inputs[N], TensorShape{out_shp.shape[1], out_shp.shape[2]},
                param)};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        auto opr = megdnn_naive_handle()->create_operator<megdnn::WarpPerspective>();
        opr->param() = param;
        dest[0].resize(out_shp);
        megdnn::TensorNDArray srcs;
        for (size_t i = 0; i < N; i++) {
            srcs.push_back(inp[i]->as_megdnn());
        }
        opr->exec(srcs, inp[N]->as_megdnn(), dest[0].as_megdnn(), {});
    };
    Checker::RunOptions opt;
    opt.numdiff_eps_single_inp[1] = 1e-5;
    opt.numdiff_max_err_single_inp[1] = 0.5;
    Checker(make_graph, fwd)
            .set_input_generator(N, warp_perspective_mat_gen(N, INP_H, INP_W))
            .disable_grad_check()
            .run({TensorShape{1, 10, 9, C}, {1, 10, 9, C}, {1, 10, 9, C}, {N, 3, 3}},
                 opt)
            .run({TensorShape{1, 4, 5, C}, {1, 4, 5, C}, {1, 4, 5, C}, {N, 3, 3}}, opt)
            .run({TensorShape{1, 6, 5, C}, {1, 6, 5, C}, {1, 6, 5, C}, {N, 3, 3}}, opt);
}

TEST(TestOprImgproc, WarpPerspectiveWithMatIdx_MultiSrc) {
    constexpr size_t INP_H = 13, INP_W = 9, N_MAT = 23, N_SRC = 3, C = 3;
    std::mt19937 rng(next_rand_seed());
    auto rand_real = [&](double lo, double hi) {
        return rng() / (std::mt19937::max() + 1.0) * (hi - lo) + lo;
    };
    auto rand_real2 = [&](double range) { return rand_real(-range, range); };

    using Checker = AutoOprChecker<5, 1>;
    TensorShape out_shp{N_MAT, C, 9, 10};
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        SymbolVarArray srcs;
        for (size_t i = 0; i < N_SRC; i++) {
            srcs.push_back(inputs[i]);
        }
        return {opr::WarpPerspective::make(
                srcs, inputs[N_SRC], inputs[N_SRC + 1],
                cg::var_from_tensor_shape(
                        srcs[0], {out_shp.shape[2], out_shp.shape[3]}))};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        auto opr = megdnn_naive_handle()->create_operator<megdnn::WarpPerspective>();
        dest[0].resize(out_shp);
        megdnn::TensorNDArray srcs;
        for (size_t i = 0; i < N_SRC; i++) {
            srcs.push_back(inp[i]->as_megdnn());
        }
        opr->exec(
                srcs, inp[N_SRC]->as_megdnn(), inp[N_SRC + 1]->as_megdnn(),
                dest[0].as_megdnn(), {});
    };
    auto gen_mat = [&](HostTensorND& mat) {
        auto ptr = mat.ptr<float>();
        for (size_t i = 0; i < N_MAT; ++i) {
            auto rot = rand_real(0, M_PI * 2), scale = rand_real(0.8, 1.2),
                 sheer = rand_real(0.9, 1.1), dy = rand_real2(INP_H * 0.5),
                 dx = rand_real2(INP_W * 0.5), ky = rand_real2(0.1 / INP_H),
                 kx = rand_real2(0.1 / INP_W), kb = rand_real2(0.1) + 1;
            ptr[0] = ptr[4] = cos(rot) * scale;
            ptr[1] = -(ptr[3] = sin(rot) * scale);
            ptr[3] *= sheer;
            ptr[4] *= sheer;
            ptr[2] = dx;
            ptr[5] = dy;
            ptr[6] = kx;
            ptr[7] = ky;
            ptr[8] = kb;
            ptr += 9;
        }
        mgb_assert(ptr == mat.ptr<float>() + mat.shape().total_nr_elems());
    };
    HostTensorGenerator<dtype::Int32> gen_mat_idx_rng{0, N_SRC};
    auto gen_mat_idx = [&](HostTensorND& mat) { mat = *gen_mat_idx_rng(mat.shape()); };
    Checker(make_graph, fwd)
            .set_input_generator(N_SRC, gen_mat)
            .set_input_generator(N_SRC + 1, gen_mat_idx)
            .set_input_dtype(N_SRC + 1, dtype::Int32{})
            .disable_grad_check()
            .run({TensorShape{1, C, 4, 5},
                  {1, C, 4, 5},
                  {1, C, 4, 5},
                  {N_MAT, 3, 3},
                  {N_MAT}})
            .run({TensorShape{1, C, 6, 5},
                  {1, C, 6, 5},
                  {1, C, 6, 5},
                  {N_MAT, 3, 3},
                  {N_MAT}})
            .run({TensorShape{1, C, 22, 19},
                  {1, C, 22, 19},
                  {1, C, 22, 19},
                  {N_MAT, 3, 3},
                  {N_MAT}});
}

TEST(TestOprImgproc, WarpPerspectiveWithMatIdx_MultiSrc_NHWC) {
    constexpr size_t INP_H = 13, INP_W = 9, N_MAT = 23, N_SRC = 3, C = 3;
    opr::WarpPerspective::Param param;
    param.format = opr::WarpPerspective::Param::Format::NHWC;
    std::mt19937 rng(next_rand_seed());
    auto rand_real = [&](double lo, double hi) {
        return rng() / (std::mt19937::max() + 1.0) * (hi - lo) + lo;
    };
    auto rand_real2 = [&](double range) { return rand_real(-range, range); };

    using Checker = AutoOprChecker<5, 1>;
    TensorShape out_shp{N_MAT, 9, 10, C};
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        SymbolVarArray srcs;
        for (size_t i = 0; i < N_SRC; i++) {
            srcs.push_back(inputs[i]);
        }
        return {opr::WarpPerspective::make(
                srcs, inputs[N_SRC], inputs[N_SRC + 1],
                cg::var_from_tensor_shape(
                        srcs[0], {out_shp.shape[1], out_shp.shape[2]}),
                param)};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        auto opr = megdnn_naive_handle()->create_operator<megdnn::WarpPerspective>();
        opr->param() = param;
        dest[0].resize(out_shp);
        megdnn::TensorNDArray srcs;
        for (size_t i = 0; i < N_SRC; i++) {
            srcs.push_back(inp[i]->as_megdnn());
        }
        opr->exec(
                srcs, inp[N_SRC]->as_megdnn(), inp[N_SRC + 1]->as_megdnn(),
                dest[0].as_megdnn(), {});
    };
    auto gen_mat = [&](HostTensorND& mat) {
        auto ptr = mat.ptr<float>();
        for (size_t i = 0; i < N_MAT; ++i) {
            auto rot = rand_real(0, M_PI * 2), scale = rand_real(0.8, 1.2),
                 sheer = rand_real(0.9, 1.1), dy = rand_real2(INP_H * 0.5),
                 dx = rand_real2(INP_W * 0.5), ky = rand_real2(0.1 / INP_H),
                 kx = rand_real2(0.1 / INP_W), kb = rand_real2(0.1) + 1;
            ptr[0] = ptr[4] = cos(rot) * scale;
            ptr[1] = -(ptr[3] = sin(rot) * scale);
            ptr[3] *= sheer;
            ptr[4] *= sheer;
            ptr[2] = dx;
            ptr[5] = dy;
            ptr[6] = kx;
            ptr[7] = ky;
            ptr[8] = kb;
            ptr += 9;
        }
        mgb_assert(ptr == mat.ptr<float>() + mat.shape().total_nr_elems());
    };
    HostTensorGenerator<dtype::Int32> gen_mat_idx_rng{0, N_SRC};
    auto gen_mat_idx = [&](HostTensorND& mat) { mat = *gen_mat_idx_rng(mat.shape()); };
    Checker(make_graph, fwd)
            .set_input_generator(N_SRC, gen_mat)
            .set_input_generator(N_SRC + 1, gen_mat_idx)
            .set_input_dtype(N_SRC + 1, dtype::Int32{})
            .disable_grad_check()
            .run({TensorShape{1, 4, 5, C},
                  {1, 4, 5, C},
                  {1, 4, 5, C},
                  {N_MAT, 3, 3},
                  {N_MAT}})
            .run({TensorShape{1, 6, 5, C},
                  {1, 6, 5, C},
                  {1, 6, 5, C},
                  {N_MAT, 3, 3},
                  {N_MAT}})
            .run({TensorShape{1, 22, 19, C},
                  {1, 22, 19, C},
                  {1, 22, 19, C},
                  {N_MAT, 3, 3},
                  {N_MAT}});
}

TEST(TestOprImgproc, WarpPerspective_NCHW4) {
    set_rand_seed(19931102);
    constexpr size_t INP_H = 6, INP_W = 4, N = 2, C = 12;
    using Checker = AutoOprChecker<2, 1>;
    TensorShape out_shp{N, C / 4, 9, 10, 4};
    opr::WarpPerspective::Param param;
    param.format = opr::WarpPerspective::Param::Format::NCHW4;
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        auto x = opr::TypeCvt::make(inputs[0], dtype::QuantizedS8(0.01f));
        auto y = opr::WarpPerspective::make(
                x, inputs[1], TensorShape{out_shp.shape[2], out_shp.shape[3]}, param);
        return {opr::TypeCvt::make(y, dtype::Float32())};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        auto opr = megdnn_naive_handle()->create_operator<megdnn::WarpPerspective>();
        opr->param() = param;
        auto typecvt = megdnn_naive_handle()->create_operator<megdnn::TypeCvt>();
        HostTensorND host_x{
                CompNode::load("xpux"), inp[0]->shape(), dtype::QuantizedS8(0.01f)};
        HostTensorND host_y{CompNode::load("xpux"), out_shp, dtype::QuantizedS8(0.01f)};
        typecvt->exec(inp[0]->as_megdnn(), host_x.as_megdnn());
        dest[0].resize(out_shp);
        opr->exec(host_x.as_megdnn(), inp[1]->as_megdnn(), host_y.as_megdnn(), {});

        typecvt->exec(host_y.as_megdnn(), dest[0].as_megdnn());
    };
    auto dump_mat = [&](const Checker::NumInpArray& inp) -> std::string {
        std::ostringstream ostr;
        ostr << std::setprecision(3);
        auto&& mat = *inp[1];
        mgb_assert(mat.shape().ndim == 3);
        auto ptr = mat.ptr<float>();
        for (size_t n = 0; n < mat.shape().shape[0]; ++n) {
            ostr << "mat " << n << ":\n";
            for (size_t i = 0; i < 3; ++i) {
                for (size_t j = 0; j < 3; ++j) {
                    ostr << std::setw(10) << *(ptr++);
                }
                ostr << '\n';
            }
        }
        return ostr.str();
    };
    Checker::RunOptions opt;
    opt.outputs_max_err = 2e-2;
    Checker(make_graph, fwd)
            .disable_grad_check()
            .set_input_generator(1, warp_perspective_mat_gen(N, INP_H, INP_W))
            .set_input_dump_on_error(dump_mat)
            .run({TensorShape{N, C / 4, 4, 5, 4}, {N, 3, 3}}, opt)
            .run({TensorShape{N, C / 4, 6, 5, 4}, {N, 3, 3}}, opt)
            .run({TensorShape{N, C / 4, 10, 9, 4}, {N, 3, 3}}, opt);
}

TEST(TestOprImgproc, WarpPerspectiveWithMatIdx) {
    constexpr size_t INP_H = 13, INP_W = 9, N_MAT = 23, N_SRC = 5, C = 3;
    std::mt19937 rng(next_rand_seed());
    auto rand_real = [&](double lo, double hi) {
        return rng() / (std::mt19937::max() + 1.0) * (hi - lo) + lo;
    };
    auto rand_real2 = [&](double range) { return rand_real(-range, range); };

    using Checker = AutoOprChecker<3, 1>;
    TensorShape out_shp{N_MAT, C, 9, 10};
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        return {opr::WarpPerspective::make(
                inputs[0], inputs[1], inputs[2],
                cg::var_from_tensor_shape(
                        inputs[0], {out_shp.shape[2], out_shp.shape[3]}))};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        auto opr = megdnn_naive_handle()->create_operator<megdnn::WarpPerspective>();
        dest[0].resize(out_shp);
        opr->exec(
                inp[0]->as_megdnn(), inp[1]->as_megdnn(), inp[2]->as_megdnn(),
                dest[0].as_megdnn(), {});
    };
    auto gen_mat = [&](HostTensorND& mat) {
        auto ptr = mat.ptr<float>();
        for (size_t i = 0; i < N_MAT; ++i) {
            auto rot = rand_real(0, M_PI * 2), scale = rand_real(0.8, 1.2),
                 sheer = rand_real(0.9, 1.1), dy = rand_real2(INP_H * 0.5),
                 dx = rand_real2(INP_W * 0.5), ky = rand_real2(0.1 / INP_H),
                 kx = rand_real2(0.1 / INP_W), kb = rand_real2(0.1) + 1;
            ptr[0] = ptr[4] = cos(rot) * scale;
            ptr[1] = -(ptr[3] = sin(rot) * scale);
            ptr[3] *= sheer;
            ptr[4] *= sheer;
            ptr[2] = dx;
            ptr[5] = dy;
            ptr[6] = kx;
            ptr[7] = ky;
            ptr[8] = kb;
            ptr += 9;
        }
        mgb_assert(ptr == mat.ptr<float>() + mat.shape().total_nr_elems());
    };
    HostTensorGenerator<dtype::Int32> gen_mat_idx_rng{0, N_SRC};
    auto gen_mat_idx = [&](HostTensorND& mat) { mat = *gen_mat_idx_rng(mat.shape()); };
    Checker(make_graph, fwd)
            .set_input_generator(1, gen_mat)
            .set_input_generator(2, gen_mat_idx)
            .set_input_dtype(2, dtype::Int32{})
            /*! it's hard to make the grad check success,
                the cuda implementation is grad sum */
            .disable_grad_check()
            .set_input_allow_grad(2, false)
            .run({TensorShape{N_SRC, C, 4, 5}, {N_MAT, 3, 3}, {N_MAT}})
            .run({TensorShape{N_SRC, C, 6, 5}, {N_MAT, 3, 3}, {N_MAT}})
            .run({TensorShape{N_SRC, C, 22, 19}, {N_MAT, 3, 3}, {N_MAT}});
}

TEST(TestOprImgproc, WarpPerspective_NHWC) {
    constexpr size_t INP_H = 6, INP_W = 4, N = 2, C = 3;
    std::mt19937 rng(next_rand_seed());
    auto rand_real = [&](double lo, double hi) {
        return rng() / (std::mt19937::max() + 1.0) * (hi - lo) + lo;
    };
    auto rand_real2 = [&](double range) { return rand_real(-range, range); };

    using Checker = AutoOprChecker<2, 1>;
    TensorShape out_shp{N, 9, 10, C};
    opr::WarpPerspective::Param param;
    param.format = opr::WarpPerspective::Param::Format::NHWC;
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        return {opr::WarpPerspective::make(
                inputs[0], inputs[1], TensorShape{out_shp.shape[1], out_shp.shape[2]},
                param)};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        auto opr = megdnn_naive_handle()->create_operator<megdnn::WarpPerspective>();
        dest[0].resize(out_shp);
        opr->param() = param;
        opr->exec(inp[0]->as_megdnn(), inp[1]->as_megdnn(), dest[0].as_megdnn(), {});
    };
    auto gen_mat = [&](HostTensorND& mat) {
        auto ptr = mat.ptr<float>();
        for (size_t i = 0; i < N; ++i) {
            auto rot = rand_real(0, M_PI * 2), scale = rand_real(0.8, 1.2),
                 sheer = rand_real(0.9, 1.1), dy = rand_real2(INP_H * 0.5),
                 dx = rand_real2(INP_W * 0.5), ky = rand_real2(0.1 / INP_H),
                 kx = rand_real2(0.1 / INP_W), kb = rand_real2(0.1) + 1;
            ptr[0] = ptr[4] = cos(rot) * scale;
            ptr[1] = -(ptr[3] = sin(rot) * scale);
            ptr[3] *= sheer;
            ptr[4] *= sheer;
            ptr[2] = dx;
            ptr[5] = dy;
            ptr[6] = kx;
            ptr[7] = ky;
            ptr[8] = kb;
            ptr += 9;
        }
        mgb_assert(ptr == mat.ptr<float>() + mat.shape().total_nr_elems());
    };
    auto dump_mat = [&](const Checker::NumInpArray& inp) -> std::string {
        std::ostringstream ostr;
        ostr << std::setprecision(3);
        auto&& mat = *inp[1];
        mgb_assert(mat.shape().ndim == 3);
        auto ptr = mat.ptr<float>();
        for (size_t n = 0; n < mat.shape().shape[0]; ++n) {
            ostr << "mat " << n << ":\n";
            for (size_t i = 0; i < 3; ++i) {
                for (size_t j = 0; j < 3; ++j) {
                    ostr << std::setw(10) << *(ptr++);
                }
                ostr << '\n';
            }
        }
        return ostr.str();
    };
    Checker::RunOptions opt;
    opt.outputs_max_err = 0.1;  // cuda NHWC impl is different from naive
    opt.numdiff_eps_single_inp[1] = 1e-5;
    opt.numdiff_max_err_single_inp[1] = 0.5;
    Checker(make_graph, fwd)
            .set_input_generator(1, gen_mat)
            .set_output_allow_grad(0, false)
            .set_input_dump_on_error(dump_mat)
            .run({TensorShape{N, 4, 5, C}, {N, 3, 3}}, opt)
            .run({TensorShape{N, 6, 5, C}, {N, 3, 3}}, opt)
            .run({TensorShape{N, 10, 9, C}, {N, 3, 3}}, opt);
}

TEST(TestOprImgproc, RotateForward) {
    constexpr size_t N = 2, C = 3;

    opr::Rotate::Param param;
    using Checker = AutoOprChecker<1, 1>;
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        return {opr::Rotate::make(inputs[0], param)};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        auto out_shape = inp[0]->shape();
        std::swap(out_shape[1], out_shape[2]);
        dest[0].resize(out_shape);
        auto opr = megdnn_naive_handle()->create_operator<megdnn::Rotate>();
        opr->param() = param;
        opr->exec(inp[0]->as_megdnn(), dest[0].as_megdnn(), {});
    };

    Checker::RunOptions opt;
    Checker(make_graph, fwd, CompNode::load("cpu1"))
            .set_output_allow_grad(0, false)
            .run({TensorShape{N, 4, 5, C}}, opt)
            .run({TensorShape{N, 6, 5, C}}, opt)
            .run({TensorShape{N, 10, 9, C}}, opt);
}

TEST(TestOprImgproc, CvtColorForward) {
    constexpr size_t N = 2, C = 3;

    opr::CvtColor::Param param;
    using Checker = AutoOprChecker<1, 1>;
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        return {opr::CvtColor::make(inputs[0], param)};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        TensorLayout out_layout;
        auto opr = megdnn_naive_handle()->create_operator<megdnn::CvtColor>();
        opr->param() = param;
        opr->deduce_layout(inp[0]->layout(), out_layout);
        dest[0].resize(out_layout);
        opr->exec(inp[0]->as_megdnn(), dest[0].as_megdnn(), {});
    };

    Checker::RunOptions opt;
    Checker(make_graph, fwd, CompNode::load("cpu1"))
            .set_output_allow_grad(0, false)
            .run({TensorShape{N, 4, 5, C}}, opt)
            .run({TensorShape{N, 6, 5, C}}, opt)
            .run({TensorShape{N, 10, 9, C}}, opt);
}

TEST(TestOprImgproc, GaussianBlurForward) {
    constexpr size_t N = 2, C = 3;

    opr::GaussianBlur::Param param;
    param.kernel_height = param.kernel_width = 5;
    using Checker = AutoOprChecker<1, 1>;
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        return {opr::GaussianBlur::make(inputs[0], param)};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        TensorLayout out_layout;
        auto opr = megdnn_naive_handle()->create_operator<megdnn::GaussianBlur>();
        opr->param() = param;
        opr->deduce_layout(inp[0]->layout(), out_layout);
        dest[0].resize(out_layout);
        opr->exec(inp[0]->as_megdnn(), dest[0].as_megdnn(), {});
    };

    Checker::RunOptions opt;
    Checker(make_graph, fwd, CompNode::load("cpu1"))
            .set_output_allow_grad(0, false)
            .run({TensorShape{N, 4, 5, C}}, opt)
            .run({TensorShape{N, 6, 5, C}}, opt)
            .run({TensorShape{N, 10, 9, C}}, opt);
}

TEST(TestOprImgproc, FlipForward) {
    constexpr size_t N = 2, C = 3;

    opr::Flip::Param param;
    using Checker = AutoOprChecker<1, 1>;
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        return {opr::Flip::make(inputs[0], param)};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        TensorLayout out_layout;
        auto opr = megdnn_naive_handle()->create_operator<megdnn::Flip>();
        opr->param() = param;
        opr->deduce_layout(inp[0]->layout(), out_layout);
        dest[0].resize(out_layout);
        opr->exec(inp[0]->as_megdnn(), dest[0].as_megdnn(), {});
    };

    Checker::RunOptions opt;
    Checker(make_graph, fwd, CompNode::load("cpu1"))
            .set_output_allow_grad(0, false)
            .run({TensorShape{N, 4, 5, C}}, opt)
            .run({TensorShape{N, 6, 5, C}}, opt)
            .run({TensorShape{N, 10, 9, C}}, opt);
}

TEST(TestOprImgproc, ResizeForward) {
    constexpr size_t N = 2, C = 3;

    opr::Resize::Param param;
    using Checker = AutoOprChecker<1, 1>;
    TensorShape out_shp{N, 9, 10, C};
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        return {opr::Resize::make(
                inputs[0], TensorShape{out_shp.shape[1], out_shp.shape[2]}, param)};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        auto opr = megdnn_naive_handle()->create_operator<megdnn::Resize>();
        opr->param() = param;
        dest[0].resize(out_shp);
        opr->exec(inp[0]->as_megdnn(), dest[0].as_megdnn(), {});
    };

    Checker::RunOptions opt;
    Checker(make_graph, fwd, CompNode::load("cpu1"))
            .set_output_allow_grad(0, false)
            .run({TensorShape{N, 4, 5, C}}, opt)
            .run({TensorShape{N, 6, 5, C}}, opt)
            .run({TensorShape{N, 10, 9, C}}, opt);
}

TEST(TestOprImgproc, ResizeForward_NCHW) {
    constexpr size_t N = 2, C = 8;

    opr::Resize::Param param;
    using Checker = AutoOprChecker<1, 1>;
    TensorShape out_shp{N, C, 9, 10};
    param.format = opr::Resize::Param::Format::NCHW;
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        return {opr::Resize::make(
                inputs[0], TensorShape{out_shp.shape[2], out_shp.shape[3]}, param)};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        auto opr = megdnn_naive_handle()->create_operator<megdnn::Resize>();
        opr->param() = param;
        dest[0].resize(out_shp);
        opr->exec(inp[0]->as_megdnn(), dest[0].as_megdnn(), {});
    };

    Checker::RunOptions opt;
    Checker(make_graph, fwd)
            .run({TensorShape{N, C, 4, 5}}, opt)
            .run({TensorShape{N, C, 6, 5}}, opt)
            .run({TensorShape{N, C, 10, 9}}, opt);
}

TEST(TestOprImgproc, ResizeForward_NCHW_NonContiguous) {
    opr::Resize::Param param;
    param.format = opr::Resize::Param::Format::NCHW;
    using Checker = AutoOprChecker<1, 2>;
    SymbolVar sub, sub_rev, input;

    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        input = inputs[0];
        auto graph = input.node()->owner_graph();
        auto cn = input.node()->comp_node();
        auto tshp = SymbolVar::make_scalar(3, *graph, cn).broadcast({2});
        auto zero = SymbolVar::make_scalar(0, *graph, cn);
        auto one = SymbolVar::make_scalar(1, *graph, cn);
        auto minus_one = SymbolVar::make_scalar(-1, *graph, cn);
        auto src_h = opr::GetVarShape::make(input, 2);
        auto src_w = opr::GetVarShape::make(input, 3);
        sub = opr::Subtensor::make(
                input,
                {opr::Subtensor::AxisIndexer::make_interval(2, one, src_h - 1, None),
                 opr::Subtensor::AxisIndexer::make_interval(3, one, src_w - 1, None)});
        sub_rev = opr::Subtensor::make(
                input, {opr::Subtensor::AxisIndexer::make_interval(
                                2, src_h - 2, zero, minus_one),
                        opr::Subtensor::AxisIndexer::make_interval(
                                3, src_w - 2, zero, minus_one)});
        auto dst = opr::Resize::make(sub, tshp, param);
        auto dst_rev = opr::Resize::make(sub_rev, tshp, param);
        return {dst, dst_rev};
    };

    auto fwd = [&](Checker::NumOutArray& out, Checker::NumInpArray in) {
        auto cn = in[0]->comp_node();
        TensorShape in_shp = in[0]->shape();
        TensorShape sub_shp{2, 3, in_shp[2] - 2, in_shp[3] - 2};
        auto sub = std::make_shared<HostTensorND>(cn, sub_shp);
        auto sub_rev = std::make_shared<HostTensorND>(cn, sub_shp);
        float* in_ptr = in[0]->ptr<float>();
        const ptrdiff_t* in_stride = in[0]->layout().stride;
        float* sub_ptr = sub->ptr<float>();
        float* sub_rev_ptr = sub_rev->ptr<float>();

        // get subtensor manually and make it contiguous
        for (size_t n = 0; n < sub_shp[0]; ++n)
            for (size_t c = 0; c < sub_shp[1]; ++c)
                for (size_t h = 0; h < sub_shp[2]; ++h)
                    for (size_t w = 0; w < sub_shp[3]; ++w) {
                        *(sub_ptr++) =
                                in_ptr[n * in_stride[0] + c * in_stride[1] +
                                       (h + 1) * in_stride[2] + (w + 1) * in_stride[3]];
                        *(sub_rev_ptr++) =
                                in_ptr[n * in_stride[0] + c * in_stride[1] +
                                       (in_shp[2] - 2 - h) * in_stride[2] +
                                       (in_shp[3] - 2 - w) * in_stride[3]];
                    }

        auto opr = megdnn_naive_handle()->create_operator<megdnn::Resize>();
        opr->param() = param;
        out[0].resize({2, 3, 3, 3});
        out[1].resize({2, 3, 3, 3});
        opr->exec(sub->as_megdnn(), out[0].as_megdnn(), {});
        opr->exec(sub_rev->as_megdnn(), out[1].as_megdnn(), {});
    };

    Checker checker(make_graph, fwd);
    checker.disable_grad_check().disable_graph_opt();

    auto test = [&](TensorShape&& shape) {
        checker.run({shape});
        auto inp_dev_ptr = static_cast<const float*>(prev_dev_ptr(input));
        ASSERT_EQ(
                inp_dev_ptr + shape[3] + 1,
                static_cast<const float*>(prev_dev_ptr(sub)));
        ASSERT_EQ(
                inp_dev_ptr + (shape[2] - 1) * shape[3] - 2,
                static_cast<const float*>(prev_dev_ptr(sub_rev)));
    };

    test(TensorShape{2, 3, 4, 4});
    test(TensorShape{2, 3, 5, 5});
    test(TensorShape{2, 3, 6, 7});
}

TEST(TestOprImgproc, ResizeForward_NCHW4) {
    constexpr size_t N = 2, C = 8;

    opr::Resize::Param param;
    using Checker = AutoOprChecker<1, 1>;
    TensorShape out_shp{N, C / 4, 9, 10, 4};
    param.format = opr::Resize::Param::Format::NCHW4;
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        auto x = opr::TypeCvt::make(inputs[0], dtype::QuantizedS8(0.01f));
        auto y = opr::Resize::make(
                x, TensorShape{out_shp.shape[2], out_shp.shape[3]}, param);
        return {opr::TypeCvt::make(y, dtype::Float32())};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        auto opr = megdnn_naive_handle()->create_operator<megdnn::Resize>();
        auto typecvt = megdnn_naive_handle()->create_operator<megdnn::TypeCvt>();
        HostTensorND host_x{
                CompNode::load("xpux"), inp[0]->shape(), dtype::QuantizedS8(0.01f)};
        HostTensorND host_y{CompNode::load("xpux"), out_shp, dtype::QuantizedS8(0.01f)};
        typecvt->exec(inp[0]->as_megdnn(), host_x.as_megdnn());
        opr->param() = param;
        opr->exec(host_x.as_megdnn(), host_y.as_megdnn(), {});
        dest[0].resize(out_shp);
        typecvt->exec(host_y.as_megdnn(), dest[0].as_megdnn());
    };

    Checker::RunOptions opt;
    opt.outputs_max_err = 2e-2;
    Checker(make_graph, fwd)
            .disable_grad_check()
            .run({TensorShape{N, C / 4, 4, 5, 4}}, opt)
            .run({TensorShape{N, C / 4, 6, 5, 4}}, opt)
            .run({TensorShape{N, C / 4, 10, 9, 4}}, opt);
}

TEST(TestOprImgproc, ResizeBackward) {
    opr::Resize::Param param;
    param.format = opr::Resize::Param::Format::NCHW;
    opr::test::BackwardChecker<opr::ResizeForward, 2> backward_checker(
            {{10, 8, 8, 4}, {10, 8, 4, 8}}, param, 1e-1, 1e-2);
}

TEST(TestOprImgproc, Resize3DForward) {
    using Param = opr::Resize3D::Param;
    using IMode = Param::InterpolationMode;
    using Format = Param::Format;
    auto ac_param = Param{IMode::LINEAR, Format::NCDHW, true};
    auto nac_param = Param{IMode::LINEAR, Format::NCDHW, false};

    auto run = [&](TensorShape ishape, TensorShape oshape, std::vector<float> idata,
                   std::vector<float> oup_ref, Param param, DType test_dtype) {
        std::shared_ptr<HostTensorND> inp_host(
                new HostTensorND{CompNode::load("xpux"), ishape, test_dtype});
        for (size_t i = 0; i < ishape.total_nr_elems(); ++i) {
            if (test_dtype == dtype::Float32()) {
                inp_host->ptr<dt_float32>()[i] = idata[i];
            } else if (test_dtype == dtype::Float16()) {
                inp_host->ptr<dt_float16>()[i] = idata[i];
            } else {
                mgb_assert(false, "invalid");
            }
        }
        std::shared_ptr<HostTensorND> oup_shape_host(new HostTensorND{
                CompNode::load("xpux"), TensorShape({oshape.ndim}), dtype::Int32()});
        for (size_t i = 0; i < oshape.ndim; ++i) {
            oup_shape_host->ptr<dt_int32>()[i] = oshape[i];
        }

        auto graph = ComputingGraph::make();
        auto inp_sym = opr::Host2DeviceCopy::make(*graph, inp_host);
        auto oup_shape_sym = opr::Host2DeviceCopy::make(*graph, oup_shape_host);
        auto oup = opr::Resize3D::make(inp_sym, oup_shape_sym, param);

        HostTensorND oup_host;
        auto func = graph->compile({make_callback_copy(oup, oup_host)});
        func->execute();

        for (size_t i = 0; i < oshape.total_nr_elems(); ++i) {
            if (test_dtype == dtype::Float32()) {
                MGB_ASSERT_FLOAT_EQ(oup_ref[i], oup_host.ptr<dt_float32>()[i]);
            } else if (test_dtype == dtype::Float16()) {
                MGB_ASSERT_FLOAT_NEAR(oup_ref[i], oup_host.ptr<dt_float16>()[i], 1e-3);
            } else {
                mgb_assert(false, "invalid");
            }
        }
    };

    for (auto&& test_dtype : std::vector<DType>{dtype::Float32(), dtype::Float16()}) {
        run({1, 1, 2, 2, 2}, {4, 4, 4}, {0., 1., 2., 3., 4., 5., 6., 7.},
            {0.,   0.25, 0.75, 1.,   0.5,  0.75, 1.25, 1.5,  1.5,  1.75, 2.25,
             2.5,  2.,   2.25, 2.75, 3.,   1.,   1.25, 1.75, 2.,   1.5,  1.75,
             2.25, 2.5,  2.5,  2.75, 3.25, 3.5,  3.,   3.25, 3.75, 4.,   3.,
             3.25, 3.75, 4.,   3.5,  3.75, 4.25, 4.5,  4.5,  4.75, 5.25, 5.5,
             5.,   5.25, 5.75, 6.,   4.,   4.25, 4.75, 5.,   4.5,  4.75, 5.25,
             5.5,  5.5,  5.75, 6.25, 6.5,  6.,   6.25, 6.75, 7.},
            nac_param, test_dtype);

        run({1, 1, 2, 2, 2}, {4, 4, 4}, {0., 1., 2., 3., 4., 5., 6., 7.},
            {0.,        0.3333333, 0.6666667, 1.,        0.6666667, 1.,
             1.3333333, 1.6666666, 1.3333334, 1.6666667, 1.9999999, 2.3333333,
             2.,        2.3333333, 2.6666665, 3.,        1.3333334, 1.6666666,
             2.0000002, 2.3333335, 2.,        2.333333,  2.6666667, 2.9999998,
             2.6666665, 3.,        3.3333333, 3.6666665, 3.3333333, 3.6666665,
             4.,        4.3333335, 2.6666667, 3.,        3.3333337, 3.6666667,
             3.3333335, 3.6666663, 4.,        4.333333,  3.9999998, 4.333333,
             4.6666665, 5.,        4.6666665, 5.,        5.3333335, 5.666667,
             4.,        4.333333,  4.666667,  5.,        4.6666665, 4.9999995,
             5.3333335, 5.6666665, 5.333333,  5.6666665, 6.,        6.3333335,
             6.,        6.333333,  6.666667,  7.},
            ac_param, test_dtype);
    }
}

TEST(TestOprImgproc, WarpAffineForward) {
    constexpr size_t INP_H = 6, INP_W = 4, N = 2, C = 3;

    opr::WarpAffine::Param param;
    using Checker = AutoOprChecker<2, 1>;
    TensorShape out_shp{N, 9, 10, C};
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        return {opr::WarpAffine::make(
                inputs[0], inputs[1], TensorShape{out_shp.shape[1], out_shp.shape[2]},
                param)};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        auto opr = megdnn_naive_handle()->create_operator<megdnn::WarpAffine>();
        opr->param() = param;
        dest[0].resize(out_shp);
        opr->exec(inp[0]->as_megdnn(), inp[1]->as_megdnn(), dest[0].as_megdnn(), {});
    };
    std::mt19937 rng(next_rand_seed());
    auto rand_real = [&](double lo, double hi) {
        return rng() / (std::mt19937::max() + 1.0) * (hi - lo) + lo;
    };
    auto rand_real2 = [&](double range) { return rand_real(-range, range); };
    auto gen_mat = [&](HostTensorND& mat) {
        auto ptr = mat.ptr<float>();
        for (size_t i = 0; i < N; ++i) {
            auto rot = rand_real(0, M_PI * 2), scale = rand_real(0.8, 1.2),
                 dy = rand_real2(INP_H * 0.5), dx = rand_real2(INP_W * 0.5);
            ptr[0] = cos(rot) * scale;
            ptr[1] = -(sin(rot) * scale);
            ptr[2] = dx;
            ptr[3] = sin(rot) * scale;
            ptr[4] = cos(rot) * scale;
            ptr[5] = dy;
            ptr += 6;
        }
        mgb_assert(ptr == mat.ptr<float>() + mat.shape().total_nr_elems());
    };
    auto dump_mat = [&](const Checker::NumInpArray& inp) -> std::string {
        std::ostringstream ostr;
        ostr << std::setprecision(3);
        auto&& mat = *inp[1];
        mgb_assert(mat.shape().ndim == 3);
        auto ptr = mat.ptr<float>();
        for (size_t n = 0; n < mat.shape().shape[0]; ++n) {
            ostr << "mat " << n << ":\n";
            for (size_t i = 0; i < 2; ++i) {
                for (size_t j = 0; j < 3; ++j) {
                    ostr << std::setw(10) << *(ptr++);
                }
                ostr << '\n';
            }
        }
        return ostr.str();
    };

    Checker::RunOptions opt;
    opt.outputs_max_err = 0.08;
    Checker(make_graph, fwd, CompNode::load("cpu1"))
            .set_input_generator(1, gen_mat)
            .set_output_allow_grad(0, false)
            .set_input_dump_on_error(dump_mat)
            .run({TensorShape{N, 4, 5, C}, {N, 2, 3}}, opt)
            .run({TensorShape{N, 6, 5, C}, {N, 2, 3}}, opt)
            .run({TensorShape{N, 10, 9, C}, {N, 2, 3}}, opt);
}

TEST(TestOprImgproc, Remap_NCHW) {
    constexpr size_t N = 2, C = 8, OH = 10, OW = 10;

    opr::Remap::Param param;
    using Checker = AutoOprChecker<2, 1>;
    TensorShape out_shp{N, C, OH, OW};
    param.format = opr::Remap::Param::Format::NCHW;
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        return {opr::Remap::make(inputs[0], inputs[1], param)};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        auto opr = megdnn_naive_handle()->create_operator<megdnn::Remap>();
        opr->param() = param;
        dest[0].resize(out_shp);
        opr->exec(inp[0]->as_megdnn(), inp[1]->as_megdnn(), dest[0].as_megdnn(), {});
    };

    std::mt19937 rng(next_rand_seed());
    auto rand_real = [&](double lo, double hi) {
        auto real = rng() / (std::mt19937::max() + 1.0) * (hi - lo) + lo;
        if (std::abs(std::round(real) - real) <= 1e-2)
            return real + 1e-1;
        return real;
    };
    auto rand_real2 = [&](double range) { return rand_real(-range, range); };
    auto gen_mat = [&](HostTensorND& mat) {
        auto ptr = mat.ptr<float>();
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < OH * OW * 2; j++) {
                //! undifferentiable when map is an integer
                ptr[j] = static_cast<float>(rand_real2(20));
            }
            ptr += OH * OW * 2;
        }
        mgb_assert(ptr == mat.ptr<float>() + mat.shape().total_nr_elems());
    };

    Checker::RunOptions opt;
    Checker(make_graph, fwd, CompNode::load("cpu1"))
            .set_input_generator(1, gen_mat)
            .run({TensorShape{N, C, 3, 20}, TensorShape{N, OH, OW, 2}}, opt)
            .run({TensorShape{N, C, 6, 5}, TensorShape{N, OH, OW, 2}}, opt)
            .run({TensorShape{N, C, 20, 20}, TensorShape{N, OH, OW, 2}}, opt);
}

TEST(TestOprImgproc, Remap_NHWC) {
    constexpr size_t N = 2, C = 8;

    opr::Remap::Param param;
    using Checker = AutoOprChecker<2, 1>;
    TensorShape out_shp{N, 10, 10, C};
    param.format = opr::Remap::Param::Format::NHWC;
    auto make_graph = [&](const Checker::SymInpArray& inputs) -> Checker::SymOutArray {
        return {opr::Remap::make(inputs[0], inputs[1], param)};
    };
    auto fwd = [&](Checker::NumOutArray& dest, Checker::NumInpArray inp) {
        auto opr = megdnn_naive_handle()->create_operator<megdnn::Remap>();
        opr->param() = param;
        dest[0].resize(out_shp);
        opr->exec(inp[0]->as_megdnn(), inp[1]->as_megdnn(), dest[0].as_megdnn(), {});
    };

    Checker::RunOptions opt;
    Checker(make_graph, fwd, CompNode::load("cpu1"))
            .disable_grad_check()
            .run({TensorShape{N, 3, 20, C}, TensorShape{N, 10, 10, 2}}, opt)
            .run({TensorShape{N, 6, 5, C}, TensorShape{N, 10, 10, 2}}, opt)
            .run({TensorShape{N, 20, 20, C}, TensorShape{N, 10, 10, 2}}, opt);
}

TEST(TestOprImgproc, DCT) {
    REQUIRE_GPU(1);
    using Checker3 = AutoOprChecker<3, 1>;
    using Checker1 = AutoOprChecker<1, 1>;
    opr::DctChannelSelectForward::Param param;
    opr::DctChannelSelectForward::Param param_nchw4;
    param_nchw4.format = opr::DctChannelSelectForward::Param::Format::NCHW4;
    auto make_graph3 =
            [&](const Checker3::SymInpArray& inputs) -> Checker3::SymOutArray {
        return {opr::DctChannelSelectForward::make(
                inputs[0], inputs[1], inputs[2], param)};
    };
    auto fwd3 = [&](Checker3::NumOutArray& dest, Checker3::NumInpArray inp) {
        auto opr = megdnn_naive_handle()
                           ->create_operator<megdnn::DctChannelSelectForward>();
        auto& in_shape = inp[0]->shape();
        TensorShape out_shp{
                in_shape[0], in_shape[1] * 64, in_shape[2] / 8, in_shape[3] / 8};
        dest[0].comp_node(inp[0]->comp_node()).resize(out_shp);
        opr->param() = param;
        opr->exec(
                inp[0]->as_megdnn(), inp[1]->as_megdnn(), inp[2]->as_megdnn(),
                dest[0].as_megdnn(), {});
    };
    auto make_graph1 =
            [&](const Checker1::SymInpArray& inputs) -> Checker1::SymOutArray {
        return {opr::DctChannelSelectForward::make(inputs[0], param)};
    };
    auto make_graph1_s8 =
            [&](const Checker1::SymInpArray& inputs) -> Checker1::SymOutArray {
        return {opr::DctChannelSelectForward::make(
                inputs[0], param_nchw4, OperatorNodeConfig(dtype::QuantizedS8(10.f)))};
    };
    auto fwd1 = [&](Checker1::NumOutArray& dest, Checker1::NumInpArray inp) {
        auto opr = megdnn_naive_handle()
                           ->create_operator<megdnn::DctChannelSelectForward>();
        auto& in_shape = inp[0]->shape();
        TensorShape out_shp{
                in_shape[0], in_shape[1] * 64, in_shape[2] / 8, in_shape[3] / 8};
        dest[0].comp_node(inp[0]->comp_node()).resize(out_shp);
        opr->param() = param;
        opr->exec(inp[0]->as_megdnn(), {}, {}, dest[0].as_megdnn(), {});
    };
    auto fwd1_s8 = [&](Checker1::NumOutArray& dest, Checker1::NumInpArray inp) {
        auto opr = megdnn_naive_handle()
                           ->create_operator<megdnn::DctChannelSelectForward>();
        auto& in_shape = inp[0]->shape();
        TensorShape out_shp{
                in_shape[0], in_shape[1] * 64 / 4, in_shape[2] / 8, in_shape[3] / 8, 4};
        dest[0].comp_node(inp[0]->comp_node()).resize(out_shp);
        opr->param() = param_nchw4;
        opr->exec(inp[0]->as_megdnn(), {}, {}, dest[0].as_megdnn(), {});
    };
    Checker3::RunOptions opt3;
    Checker1::RunOptions opt1;
    Checker1::RunOptions opt1_qint8;
    opt3.outputs_max_err = 1e-3;
    opt1.outputs_max_err = 1e-3;
    opt1_qint8.outputs_max_err = 1.001;

    auto gen_input = [](HostTensorND& dest) {
        HostTensorGenerator<dtype::Uint8, RandomDistribution::UNIFORM> mask_generator{
                0, 255};
        dest = *mask_generator(dest.shape(), dest.comp_node());
    };
    auto gen_mask = [](HostTensorND& dest) {
        HostTensorGenerator<dtype::Int32, RandomDistribution::UNIFORM> mask_generator{
                0, 8};
        dest = *mask_generator(dest.shape(), dest.comp_node());
    };
    Checker1(make_graph1, fwd1, CompNode::load("gpu0"))
            .disable_grad_check()
            .set_input_generator(0, gen_input)
            .set_input_dtype(0, dtype::Uint8())
            .run({TensorShape{1, 1, 16, 16}}, opt1)
            .run({TensorShape{1, 3, 256, 256}}, opt1)
            .run({TensorShape{4, 3, 512, 512}}, opt1);

    Checker1(make_graph1_s8, fwd1_s8, CompNode::load("gpu0"))
            .disable_grad_check()
            .set_input_generator(0, gen_input)
            .set_input_dtype(0, dtype::Uint8())
            .run({TensorShape{1, 1, 16, 16}}, opt1_qint8)
            .run({TensorShape{1, 3, 256, 256}}, opt1_qint8)
            .run({TensorShape{4, 3, 512, 512}}, opt1_qint8);

    MGB_MARK_USED_VAR(make_graph3);
    MGB_MARK_USED_VAR(fwd3);
    MGB_MARK_USED_VAR(gen_mask);
}

TEST(TestOprImgproc, DCT_BAD_MASK) {
    HostTensorGenerator<dtype::Uint8> gen_u8;
    HostTensorGenerator<dtype::Int32> gen_s32;
    TensorShape src_shape({1, 2, 256, 256}), mask_offset_shape({3}),
            mask_val_shape({8});
    opr::DctChannelSelectForward::Param param;

    auto graph = ComputingGraph::make();

    auto src_tensor = gen_u8(src_shape);
    auto mask_offset_tensor = gen_s32(mask_offset_shape);
    auto mask_val_tensor = gen_s32(mask_val_shape);
    auto mask_offset_ptr = mask_offset_tensor->ptr<int32_t>();
    auto mask_val_ptr = mask_val_tensor->ptr<int32_t>();
    mask_offset_ptr[0] = 1;
    mask_val_ptr[0] = 64;
    auto src_sym = opr::ImmutableTensor::make(*graph, *src_tensor);
    auto mask_offset_sym = opr::ImmutableTensor::make(*graph, *mask_offset_tensor);
    auto mask_val_sym = opr::ImmutableTensor::make(*graph, *mask_val_tensor);

    ASSERT_THROW(
            opr::DctChannelSelect::make(src_sym, mask_offset_sym, mask_val_sym, param),
            MegBrainError);

    mask_offset_ptr[0] = 0;
    mask_offset_ptr[1] = 2;
    mask_offset_ptr[2] = 8;
    mask_offset_sym = opr::ImmutableTensor::make(*graph, *mask_offset_tensor);
    ASSERT_THROW(
            opr::DctChannelSelect::make(src_sym, mask_offset_sym, mask_val_sym, param),
            MegBrainError);

    mask_val_ptr[0] = 0;
    mask_val_ptr[1] = 1;
    mask_val_ptr[2] = 2;
    mask_val_ptr[3] = 3;
    mask_val_ptr[4] = 4;
    mask_val_ptr[5] = 5;
    mask_val_ptr[6] = 6;
    mask_val_ptr[7] = 7;
    mask_val_sym = opr::ImmutableTensor::make(*graph, *mask_val_tensor);
    opr::DctChannelSelect::make(src_sym, mask_offset_sym, mask_val_sym, param);

    param.format = opr::DctChannelSelect::Param::Format::NCHW4;
    ASSERT_THROW(
            opr::DctChannelSelect::make(src_sym, mask_offset_sym, mask_val_sym, param),
            MegBrainError);
}
// vim: syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
