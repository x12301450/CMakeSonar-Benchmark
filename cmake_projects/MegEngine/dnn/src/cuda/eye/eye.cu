#include "megdnn/dtype.h"
#include "src/cuda/eye/eye.cuh"
#include "src/cuda/utils.cuh"

namespace {

template <typename T>
__global__ void kernel(T* dst, uint32_t m, uint32_t n, int k) {
    int32_t i = threadIdx.x + blockIdx.x * blockDim.x;
    int32_t x = i % n;
    int32_t y = i / n;
    if (i < m * n) {
        dst[i] = (y + k == x);
    }
}

}  // anonymous namespace

namespace megdnn {
namespace cuda {
namespace eye {

template <typename T>
void exec_internal(T* dst, size_t m, size_t n, int k, cudaStream_t stream) {
    kernel<T><<<DIVUP(m * n, NR_THREADS), NR_THREADS, 0, stream>>>(dst, m, n, k);
    after_kernel_launch();
}

#define INST(T)   template void exec_internal<T>(T*, size_t, size_t, int, cudaStream_t);
#define cb(DType) INST(typename DTypeTrait<DType>::ctype)
MEGDNN_FOREACH_COMPUTING_DTYPE(cb)
cb(::megdnn::dtype::Bool)

}  // namespace eye
}  // namespace cuda
}  // namespace megdnn
// vim: syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
