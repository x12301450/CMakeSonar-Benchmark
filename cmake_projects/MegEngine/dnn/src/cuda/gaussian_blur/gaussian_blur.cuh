#pragma once
#include <cuda_runtime_api.h>
#include <cstddef>
#include "src/common/cv/enums.h"

#include <stdint.h>

namespace megdnn {
namespace cuda {
namespace gaussian_blur {

template <typename T, size_t CH, BorderMode bmode>
void gaussian_blur(
        const T* src, T* dst, size_t N, size_t H, size_t W, size_t stride0,
        size_t stride1, size_t stride2, size_t stride3, uint8_t* kernel_ptr,
        size_t kernel_height, size_t kernel_width, double sigma_x, double sigma_y,
        cudaStream_t stream);

}  // namespace gaussian_blur
}  // namespace cuda
}  // namespace megdnn

// vim: syntax=cpp.doxygen
