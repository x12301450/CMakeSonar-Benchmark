#pragma once
#include "src/fallback/matrix_mul/gemm_common.h"

#if __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
namespace megdnn {
namespace armv7 {
namespace matmul {

MEGDNN_REG_GEMM_STRATEGY(
        dt_float16, dt_float16, dt_float16, 4, 16, 1, false, true, hgemm_4x16);

MEGDNN_REG_GEMM_STRATEGY_NOPACK(
        dt_float16, dt_float16, dt_float16, 4, 8, 1, false, true, gemm_nopack_f16_4x8);

}  // namespace matmul
}  // namespace armv7
}  // namespace megdnn
#endif
// vim: syntax=cpp.doxygen
