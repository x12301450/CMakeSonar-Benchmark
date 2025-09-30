#include "src/cuda/matrix_mul/naive.cuh"
#include <cuda.h>
#include "src/cuda/matrix_mul/algos.h"
#include "src/cuda/utils.h"

using namespace megdnn;
using namespace cuda;

#include "midout.h"
MIDOUT_DECL(megdnn_naive_matmul)

bool MatrixMulForwardImpl::AlgoNaive::is_available(const SizeArgs& args) const {
    if (args.can_be_treated_as_int8x8x32())
        return true;
    auto&& layout_a = args.layout_a;
    auto&& layout_b = args.layout_b;
    auto&& layout_c = args.layout_c;
    return layout_a.dtype.enumv() == layout_b.dtype.enumv() &&
           (layout_a.dtype.enumv() == DTypeEnum::Float32 ||
            layout_a.dtype.enumv() == DTypeEnum::Float16) &&
           (layout_c.dtype.enumv() == DTypeEnum::Float32 ||
            layout_c.dtype.enumv() == DTypeEnum::Float16) &&
           args.opr->param().format == param::MatrixMul::Format::DEFAULT;
}
void MatrixMulForwardImpl::AlgoNaive::exec(const ExecArgs& args) const {
    auto&& param = args.opr->param();
    auto m = args.tensor_c.layout.shape[0], n = args.tensor_c.layout.shape[1],
         k = args.tensor_a.layout.shape[param.transposeA ? 0 : 1];
    auto LDA = args.tensor_a.layout.stride[0], LDB = args.tensor_b.layout.stride[0],
         LDC = args.tensor_c.layout.stride[0];

    auto&& handle = concrete_handle(args.opr->handle());

    using ComputeMode = Param::ComputeMode;
#define DISPATCH_CMODE(in_dt, out_dt, in_ct, out_ct, comp_ct, cmode)               \
    MIDOUT_BEGIN(                                                                  \
            megdnn_naive_matmul,                                                   \
            midout_iv(#in_dt #out_dt #in_ct, #out_ct, #comp_ct, #cmode)) {         \
        do {                                                                       \
            using namespace dtype;                                                 \
            if (args.tensor_a.layout.dtype.enumv() == DTypeTrait<in_dt>::enumv &&  \
                args.tensor_c.layout.dtype.enumv() == DTypeTrait<out_dt>::enumv && \
                param.compute_mode == cmode) {                                     \
                in_ct* A = args.tensor_a.compatible_ptr<in_ct>();                  \
                in_ct* B = args.tensor_b.compatible_ptr<in_ct>();                  \
                out_ct* C = args.tensor_c.compatible_ptr<out_ct>();                \
                exec_gemm_naive<in_ct, in_ct, out_ct, comp_ct>(                    \
                        A, B, C, m, n, k, LDA, LDB, LDC, param.transposeA,         \
                        param.transposeB, cuda_stream(handle));                    \
                return;                                                            \
            }                                                                      \
        } while (0);                                                               \
    }                                                                              \
    MIDOUT_END();
#define DISPATCH(in_dt, out_dt, in_ct, out_ct, comp_ct) \
    DISPATCH_CMODE(in_dt, out_dt, in_ct, out_ct, comp_ct, ComputeMode::DEFAULT)

    DISPATCH(Float32, Float32, dt_float32, dt_float32, dt_float32);
    DISPATCH(Float16, Float16, dt_float16, dt_float16, dt_float16);
    DISPATCH(Int8, Int32, dt_int8, dt_int32, dt_int32);
    DISPATCH(QuantizedS8, QuantizedS32, dt_int8, dt_int32, dt_int32);
    DNN_INC_FLOAT16(DISPATCH_CMODE(
            Float16, Float16, dt_float16, dt_float16, dt_float32,
            ComputeMode::FLOAT32));
#undef DISPATCH_CMODE
#undef DISPATCH
    megdnn_throw(ssprintf(
            "runtime does not support MatMul(%s, %s) -> %s with cmode = %d\n"
            "now support case list: MatMul(FLOAT, FLOAT)\n"
            "                       MatMul(Int8, Int8)\n"
            "                       MatMul(Int16, Int16)\n"
            "                       MatMul(QuantizedS8, QuantizedS8)\n"
            "                       MatMul(Quantized8Asymm, Quantized8Asymm)\n"
            "                       MatMul(Quantized4Asymm, Quantized4Asymm)\n"
            "                       MatMul(QuantizedS4, QuantizedS4)\n",
            args.layout_a.dtype.name(), args.layout_b.dtype.name(),
            args.layout_c.dtype.name(), static_cast<int>(param.compute_mode)));
}

// vim: syntax=cpp.doxygen
