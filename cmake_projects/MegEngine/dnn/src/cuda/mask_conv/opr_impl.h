#pragma once

#include "megdnn/oprs.h"
#include "src/cuda/handle.h"

namespace megdnn {
namespace cuda {

class MaskConvForwardImpl : public MaskConvForward {
public:
    MaskConvForwardImpl(Handle* handle);

    void exec(
            _megdnn_tensor_in src, _megdnn_tensor_in filter, _megdnn_tensor_in mask,
            _megdnn_tensor_out dst, _megdnn_workspace workspace) override;

    size_t get_workspace_in_bytes(
            const TensorLayout& src, const TensorLayout& filter,
            const TensorLayout& mask, const TensorLayout& dst) override {
        MEGDNN_MARK_USED_VAR(mask);
        m_conv_opr->param() = param();
        return m_conv_opr->get_workspace_in_bytes(src, filter, dst, nullptr);
    }

private:
    std::unique_ptr<ConvolutionForward> m_conv_opr;
};

class MaskPropagateImpl : public MaskPropagate {
public:
    MaskPropagateImpl(Handle* handle) : MaskPropagate(handle) {}

    void exec(
            _megdnn_tensor_in src, _megdnn_tensor_out dst,
            _megdnn_workspace worksapce) override final;
    size_t get_workspace_in_bytes(
            const TensorLayout&, const TensorLayout&) override final {
        return 0;
    }
};

}  // namespace cuda
}  // namespace megdnn
