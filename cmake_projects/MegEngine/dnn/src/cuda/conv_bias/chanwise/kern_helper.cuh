#pragma once

#include "megdnn/dtype.h"
#include "src/cuda/query_blocksize.cuh"
#include "src/cuda/utils.cuh"

#include <cuda_runtime.h>
#include <stdint.h>
#include <algorithm>

namespace megdnn {
namespace cuda {
namespace conv_bias {
namespace chanwise {

/*!
 * \brief return a / b and set mod to a % b
 */
__device__ __forceinline__ uint32_t div_mod(uint32_t a, uint32_t b, uint32_t& mod) {
    uint32_t ret = a / b;
    mod = a - ret * b;
    return ret;
}

/*!
 * \brief copy a 2D matrix by all threads in a block
 * \param rs row stride
 */
template <typename T>
__device__ __forceinline__ void block_memcpy(T* dst, const T* src, uint32_t size) {
    for (uint32_t i = threadIdx.x; i < size; i += blockDim.x) {
        dst[i] = src[i];
    }
    __syncthreads();
}

}  // namespace chanwise
}  // namespace conv_bias
}  // namespace cuda
}  // namespace megdnn

// vim: syntax=cuda.doxygen
