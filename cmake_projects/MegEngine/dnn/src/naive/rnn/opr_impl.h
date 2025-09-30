#pragma once
#include "megdnn/oprs.h"

namespace megdnn {
namespace naive {

class RNNImpl : public RNN {
public:
    using RNN::RNN;

    void exec(
            _megdnn_tensor_in input, _megdnn_tensor_in hx,
            _megdnn_tensor_in flatten_weights, _megdnn_tensor_out output,
            _megdnn_tensor_out hy, _megdnn_tensor_out reserve_space,
            _megdnn_workspace workspace) override;

    size_t get_workspace_in_bytes(
            const TensorLayout& input, const TensorLayout& hx,
            const TensorLayout& flatten_weights, const TensorLayout& output,
            const TensorLayout& hy, const TensorLayout& reserve_space) override;
    size_t get_reserve_size_in_bytes(const TensorLayout& input) override;
    bool is_thread_safe() const override { return true; }
};

class RNNBackwardImpl : public RNNBackward {
public:
    using RNNBackward::RNNBackward;

    virtual void exec(
            _megdnn_tensor_in x, _megdnn_tensor_in y, _megdnn_tensor_in hx,
            _megdnn_tensor_in dy, _megdnn_tensor_in dhy,
            _megdnn_tensor_in flatten_weights, _megdnn_tensor_in reserve_space,
            _megdnn_tensor_out dx, _megdnn_tensor_out dhx, _megdnn_tensor_out dw,
            _megdnn_workspace workspace) override;

    virtual size_t get_workspace_in_bytes(
            const TensorLayout& x, const TensorLayout& y, const TensorLayout& hx,
            const TensorLayout& dy, const TensorLayout& dhy,
            const TensorLayout& flatten_weights, const TensorLayout& reserve_space,
            const TensorLayout& dx, const TensorLayout& dhx,
            const TensorLayout& dw) override;
    bool is_thread_safe() const override { return true; }
};

}  // namespace naive
}  // namespace megdnn
