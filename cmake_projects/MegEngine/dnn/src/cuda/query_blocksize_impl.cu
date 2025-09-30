#include "src/cuda/kernel_common/diagnostic_prologue.cuh"
#include "src/cuda/query_blocksize.cuh"
#include "src/cuda/utils.cuh"

using namespace megdnn;
using namespace cuda;

/*
 * Note: cudaOccupancyMaxPotentialBlockSizeVariableSMem is only available when
 * compiled by nvcc, but it is implemented as a __host__ __device__ function. So
 * we implement a device wrapper
 */
namespace {

struct SmemGetterWrapper {
    SmemGetter getter;

    __device__ __host__ int operator()(int block_size) const {
#if __CUDA_ARCH__
        // device func should never be called
        int* ptr = 0;
        *ptr = 23;
#else
        if (getter.func) {
            return getter.func(block_size, getter.user_data);
        }
#endif
        return 0;
    }
};

}  // anonymous namespace

LaunchConfig cuda::detail::query_launch_config_for_kernel_uncached(
        const void* kern, const SmemGetter& smem) {
    SmemGetterWrapper s;
    s.getter = smem;
    LaunchConfig ret;
    cuda_check(cudaOccupancyMaxPotentialBlockSizeVariableSMem(
            &ret.grid_size, &ret.block_size, kern, s));
    return ret;
}

#include "src/cuda/kernel_common/diagnostic_epilogue.cuh"
// vim: ft=cpp syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
