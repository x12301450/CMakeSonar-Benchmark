#include "megdnn/oprs.h"

#include "src/common/utils.h"

namespace megdnn {

void FlipBase::deduce_layout_fwd(const TensorLayout& src, TensorLayout& dst) {
    auto errmsg = [&]() { return megdnn_layout_msg(src); };
    MEGDNN_MARK_USED_VAR(errmsg);

    megdnn_assert(
            src.ndim == 4_z && (src.shape[3] == 1_z || src.shape[3] == 3_z), "%s",
            errmsg().c_str());

    size_t in = src.shape[0];
    size_t ih = src.shape[1];
    size_t iw = src.shape[2];
    size_t ic = src.shape[3];

    dst = TensorLayout{{in, ih, iw, ic}, src.dtype};
}

void FlipBase::check_layout_fwd(const TensorLayout& src, const TensorLayout& dst) {
    TensorLayout dst_expected;
    megdnn_assert_eq_dtype(src, dst);
    deduce_layout_fwd(src, dst_expected);
    megdnn_assert_eq_shape(dst_expected, dst);
}

void Flip::deduce_layout(const TensorLayout& src, TensorLayout& dst) {
    deduce_layout_fwd(src, dst);
}

void Flip::check_exec(
        const TensorLayout& src, const TensorLayout& dst, size_t workspace_in_bytes) {
    check_layout_fwd(src, dst);
    auto required_workspace_in_bytes = get_workspace_in_bytes(src, dst);
    megdnn_assert(workspace_in_bytes >= required_workspace_in_bytes);
}

}  // namespace megdnn

// vim: syntax=cpp.doxygen
