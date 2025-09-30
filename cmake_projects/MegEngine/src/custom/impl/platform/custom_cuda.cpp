#include "megbrain/common.h"
#include "megbrain_build_config.h"

#if MGB_CUSTOM_OP

#include "megbrain/comp_node_env.h"
#include "megbrain/custom/adaptor.h"
#include "megbrain/custom/platform/custom_cuda.h"

using namespace mgb;

namespace custom {

#if MGB_CUDA

const CudaRuntimeArgs get_cuda_runtime_args(const RuntimeArgs& rt_args) {
    mgb_assert(
            rt_args.device().enumv() == DeviceEnum::cuda,
            "devive type should be cuda.");
    const CompNodeEnv& env =
            CompNodeEnv::from_comp_node(to_builtin<CompNode, Device>(rt_args.device()));
    const CompNodeEnv::CudaEnv& cuda_env = env.cuda_env();
    return {cuda_env.device, cuda_env.stream};
}

int get_cuda_device_id(Device device) {
    auto cn = to_builtin<CompNode>(device);
    return CompNodeEnv::from_comp_node(cn).cuda_env().device;
}

const cudaDeviceProp* get_cuda_device_props(Device device) {
    auto cn = to_builtin<CompNode>(device);
    return &CompNodeEnv::from_comp_node(cn).cuda_env().device_prop;
}

cudaStream_t get_cuda_stream(Device device) {
    auto cn = to_builtin<CompNode>(device);
    return CompNodeEnv::from_comp_node(cn).cuda_env().stream;
}

int get_max_threads_per_block(Device device) {
    auto cn = to_builtin<CompNode>(device);
    return CompNodeEnv::from_comp_node(cn).cuda_env().device_prop.maxThreadsPerBlock;
}

const int* get_max_threads_dim(Device device) {
    auto cn = to_builtin<CompNode>(device);
    return CompNodeEnv::from_comp_node(cn).cuda_env().device_prop.maxThreadsDim;
}

const int* get_max_grid_size(Device device) {
    auto cn = to_builtin<CompNode>(device);
    return CompNodeEnv::from_comp_node(cn).cuda_env().device_prop.maxGridSize;
}

#else

const CudaRuntimeArgs get_cuda_runtime_args(const RuntimeArgs& rt_args) {
    mgb_assert(
            false,
            "megbrain is not support cuda now, please rebuild megbrain with CUDA "
            "ENABLED");
}

int get_cuda_device_id(Device device) {
    mgb_assert(
            false,
            "megbrain is not support cuda now, please rebuild megbrain with CUDA "
            "ENABLED");
}

const cudaDeviceProp* get_cuda_device_props(Device device) {
    mgb_assert(
            false,
            "megbrain is not support cuda now, please rebuild megbrain with CUDA "
            "ENABLED");
}

cudaStream_t get_cuda_stream(Device device) {
    mgb_assert(
            false,
            "megbrain is not support cuda now, please rebuild megbrain with CUDA "
            "ENABLED");
}

#define CUDA_DISABLED(func_def)                                                        \
    func_def {                                                                         \
        mgb_assert(                                                                    \
                false,                                                                 \
                "megbrain is not support cuda now, please rebuild megbrain with CUDA " \
                "ENABLED");                                                            \
    }

CUDA_DISABLED(int get_max_threads_per_block(Device device))

CUDA_DISABLED(const int* get_max_threads_dim(Device device))

CUDA_DISABLED(const int* get_max_grid_size(Device device))

#endif

}  // namespace custom

#endif
