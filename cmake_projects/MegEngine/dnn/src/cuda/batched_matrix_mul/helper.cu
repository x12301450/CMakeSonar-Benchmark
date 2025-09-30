#include "src/cuda/batched_matrix_mul/helper.cuh"

namespace {

template <typename T>
__global__ void kernel(T* Xs, T start, uint32_t step, uint32_t n) {
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        Xs[i] = start + i * step;
    }
}

}  // anonymous namespace

namespace megdnn {
namespace cuda {
namespace batched_matrix_mul {

template <typename T>
void arange(T* Xs, T start, uint32_t step, uint32_t n, cudaStream_t stream) {
    uint32_t threads = NR_THREADS;
    uint32_t blocks = DIVUP(n, threads);
    kernel<T><<<blocks, threads, 0, stream>>>(Xs, start, step, n);
    after_kernel_launch();
}

template void arange<uintptr_t>(
        uintptr_t*, uintptr_t, uint32_t, uint32_t, cudaStream_t);

}  // namespace batched_matrix_mul
}  // namespace cuda
}  // namespace megdnn

// vim: syntax=cpp.doxygen
