#pragma once
#include "megdnn/oprs.h"

namespace megdnn {
namespace cuda {

class IndexingRemapForwardImpl final : public IndexingRemapForward {
public:
    using IndexingRemapForward::IndexingRemapForward;
    void exec(
            _megdnn_tensor_in src, _megdnn_tensor_in map, _megdnn_tensor_out dst,
            _megdnn_workspace workspace) override;
    size_t get_workspace_in_bytes(
            const TensorLayout&, const TensorLayout&, const TensorLayout&) override {
        return 0;
    }
};

class IndexingRemapBackwardImpl final : public IndexingRemapBackward {
public:
    using IndexingRemapBackward::IndexingRemapBackward;
    void exec(
            _megdnn_tensor_in diff, _megdnn_tensor_in map, _megdnn_tensor_out grad,
            _megdnn_workspace workspace) override;
    size_t get_workspace_in_bytes(
            const TensorLayout&, const TensorLayout&, const TensorLayout&) override {
        return 0;
    }
};

}  // namespace cuda
}  // namespace megdnn

// vim: syntax=cpp.doxygen
