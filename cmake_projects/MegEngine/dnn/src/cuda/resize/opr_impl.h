#pragma once
#include "megdnn/oprs.h"

namespace megdnn {
namespace cuda {
class ResizeImpl : public Resize {
public:
    using Resize::Resize;

    void exec(
            _megdnn_tensor_in src, _megdnn_tensor_out dst,
            _megdnn_workspace workspace) override;

    size_t get_workspace_in_bytes(const TensorLayout&, const TensorLayout&) override;
};

class ResizeBackwardImpl final : public ResizeBackward {
public:
    using ResizeBackward::ResizeBackward;
    void exec(
            _megdnn_tensor_in diff, _megdnn_tensor_out grad,
            _megdnn_workspace workspace) override;
    size_t get_workspace_in_bytes(
            const TensorLayout& diff, const TensorLayout& grad) override;
};

class Resize3DImpl final : public Resize3D {
public:
    using Resize3D::Resize3D;
    void exec(
            _megdnn_tensor_in src, _megdnn_tensor_out dst,
            _megdnn_workspace workspace) override;
    size_t get_workspace_in_bytes(
            const TensorLayout& src, const TensorLayout& dst) override;
};

}  // namespace cuda
}  // namespace megdnn

// vim: syntax=cpp.doxygen
