#pragma once
#include "megdnn/oprs.h"
#include "src/common/algo_base.h"
#include "src/common/metahelper.h"
#include "src/cuda/cudnn_wrapper.h"
#include "src/cuda/utils.h"

namespace megdnn {
namespace cuda {

class SoftmaxForwardImpl final : public SoftmaxForward {
public:
    using SoftmaxForward::SoftmaxForward;

    std::vector<int> init_mode(_megdnn_tensor_in src, cudnnSoftmaxMode_t& mode) const;

    virtual void exec(
            _megdnn_tensor_in src, _megdnn_tensor_out dst,
            _megdnn_workspace workspace) override;
    size_t get_workspace_in_bytes(
            const TensorLayout&, /* src */
            const TensorLayout& /* dst */) override {
        return 0;
    }
};

class SoftmaxBackwardImpl final : public SoftmaxBackward {
public:
    using SoftmaxBackward::SoftmaxBackward;

    std::vector<int> init_mode(_megdnn_tensor_in src, cudnnSoftmaxMode_t& mode) const;

    size_t get_workspace_in_bytes(
            const TensorLayout& /* input */, const TensorLayout& /* diff */,
            const TensorLayout& /* grad_x */) override {
        return 0;
    }

    virtual void exec(
            _megdnn_tensor_in src, _megdnn_tensor_in diff, _megdnn_tensor_out grad,
            _megdnn_workspace workspace) override;
};

}  // namespace cuda
}  // namespace megdnn

// vim: syntax=cpp.doxygen
