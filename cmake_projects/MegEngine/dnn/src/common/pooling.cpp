#include "megdnn/oprs.h"

#include "src/common/utils.h"

namespace megdnn {

void PoolingBase::deduce_layout_impl(
        const TensorLayout& src, const Param& param, TensorLayout& dst) {
    auto pformat = param.format;

    // the overhead of generating error message is about 18x of the other part of this
    // function so we use a function to wrap the error message and get it only when need.
    auto get_errmsg = [&](void) -> std::string {
        std::string errmsg =
                megdnn_layout_msg(src) + ", " + megdnn_layout_msg(dst) + ", " +
                "pad_h=" + std::to_string(param.pad_h) + ", " +
                "pad_w=" + std::to_string(param.pad_w) + ", " +
                "stride_h=" + std::to_string(param.stride_h) + ", " +
                "stride_w=" + std::to_string(param.stride_w) + ", " +
                "window_h=" + std::to_string(param.window_h) + ", " +
                "window_w=" + std::to_string(param.window_w) + ", " +
                "is_max=" + std::to_string(param.mode == Mode::MAX) + ", " +
                "is_nhwc=" + std::to_string(pformat == Param::Format::NHWC) + ", " +
                "is_nhwcd4=" + std::to_string(pformat == Param::Format::NHWCD4);
        return errmsg;
    };

    MEGDNN_MARK_USED_VAR(get_errmsg);
    if (!src.is_empty()) {
        megdnn_assert_contiguous(src);
    } else {
        megdnn_assert(
                src.ndim == 4 && (pformat == Param::Format::NCHW ||
                                  pformat == Param::Format::NHWC),
                "Pooling: empty input is only support when input format is NHWC or "
                "NCHW");
    }
    size_t spatial_pos, c_pos, batch_pos = 0;
    if (pformat == Param::Format::NCHW) {
        megdnn_assert(src.ndim == 4_z, "%s", get_errmsg().c_str());

        spatial_pos = 2;
        c_pos = 1;
    } else if (pformat == Param::Format::NHWC) {
        megdnn_assert(src.ndim == 4_z, "%s", get_errmsg().c_str());

        spatial_pos = 1;
        c_pos = 3;
    } else if (
            pformat == Param::Format::NCHW4 || pformat == Param::Format::NCHW44 ||
            pformat == Param::Format::NCHW88 || pformat == Param::Format::NCHW32 ||
            pformat == Param::Format::NCHW64) {
        megdnn_assert(src.ndim == 5_z, "%s", get_errmsg().c_str());

        spatial_pos = 2;
        c_pos = 1;
    } else if (pformat == Param::Format::CHWN4) {
        spatial_pos = 1;
        c_pos = 0;
        batch_pos = 3;
    } else {
        megdnn_assert(
                pformat == Param::Format::NHWCD4 && src.ndim == 5_z, "%s",
                get_errmsg().c_str());
        spatial_pos = 1;
        c_pos = 2;
    }
    size_t n = src[batch_pos];
    size_t c = src[c_pos];
    size_t ih = src[spatial_pos];
    size_t iw = src[spatial_pos + 1];
    if (pformat == Param::Format::NHWCD4) {
        c *= 4;
        iw = src[spatial_pos + 2];
    }
    if (pformat == Param::Format::NCHW4 || pformat == Param::Format::NCHW44 ||
        pformat == Param::Format::CHWN4) {
        c *= 4;
    }
    if (pformat == Param::Format::NCHW88) {
        c *= 8;
    }
    if (pformat == Param::Format::NCHW32) {
        c *= 32;
    }
    if (pformat == Param::Format::NCHW64) {
        c *= 64;
    }
    size_t oh, ow;
    size_t fh = param.window_h;
    size_t fw = param.window_w;
    size_t sh = param.stride_h;
    size_t sw = param.stride_w;
    size_t ph = param.pad_h;
    size_t pw = param.pad_w;

    // moving some python assert to here
    // megdnn_assert()

    if (ph >= fh || pw >= fw) {
        megdnn_log_warn(
                "pooling padding size (%zu %zu) should not be bigger than "
                "window size (%zu %zu), it only can be used in CaffePooling",
                pw, ph, fw, fh);
    }
    infer_conv_shape2d(ih, iw, fh, fw, sh, sw, ph, pw, oh, ow);
    if (pformat == Param::Format::NCHW) {
        dst = TensorLayout(TensorShape({n, c, oh, ow}), src.dtype);
    } else if (pformat == Param::Format::NHWC) {
        megdnn_assert(pformat == Param::Format::NHWC, "invalid pooling format");
        dst = TensorLayout({n, oh, ow, c}, src.dtype, src.format);
    } else if (pformat == Param::Format::NCHW4 || pformat == Param::Format::NCHW44) {
        dst = TensorLayout{{n, c / 4, oh, ow, 4}, src.dtype, src.format};
    } else if (pformat == Param::Format::NCHW88) {
        dst = TensorLayout{{n, c / 8, oh, ow, 8}, src.dtype, src.format};
    } else if (pformat == Param::Format::NCHW32) {
        dst = TensorLayout{{n, c / 32, oh, ow, 32}, src.dtype, src.format};
    } else if (pformat == Param::Format::NCHW64) {
        dst = TensorLayout{{n, c / 64, oh, ow, 64}, src.dtype, src.format};
    } else if (pformat == Param::Format::CHWN4) {
        dst = TensorLayout{{c / 4, oh, ow, n, 4}, src.dtype, src.format};
    } else {
        megdnn_assert(pformat == Param::Format::NHWCD4, "invalid pooling format");
        dst = TensorLayout{{n, oh, c / 4, ow, 4}, src.dtype, src.format};
    }
}

void PoolingBase::deduce_layout_fwd(const TensorLayout& src, TensorLayout& dst) {
    deduce_layout_impl(src, param(), dst);
}

void PoolingBase::check_layout_fwd(const TensorLayout& src, const TensorLayout& dst) {
    TensorLayout dst_expected;
    megdnn_assert_eq_dtype(src, dst);
    deduce_layout_fwd(src, dst_expected);
    megdnn_assert_eq_layout(dst_expected, dst);
    megdnn_assert(
            src.dtype.category() == DTypeCategory::FLOAT ||
            src.dtype == dtype::Int8() ||
            src.dtype.category() == DTypeCategory::QUANTIZED);
}

void PoolingForward::deduce_layout(const TensorLayout& src, TensorLayout& dst) {
    deduce_layout_fwd(src, dst);
}

void PoolingForward::check_exec(
        const TensorLayout& src, const TensorLayout& dst, size_t workspace_in_bytes) {
    check_layout_fwd(src, dst);
    auto required_workspace_in_bytes = get_workspace_in_bytes(src, dst);
    megdnn_assert(
            workspace_in_bytes >= required_workspace_in_bytes, "need %zu, get %zu",
            required_workspace_in_bytes, workspace_in_bytes);
}

void PoolingBackward::check_exec(
        const TensorLayout& src, const TensorLayout& dst, const TensorLayout& diff,
        const TensorLayout& grad, size_t workspace_in_bytes) {
    check_layout_fwd(src, dst);
    megdnn_assert_eq_layout(src, grad);
    megdnn_assert_eq_layout(dst, diff);
    auto required_workspace_in_bytes = get_workspace_in_bytes(src, dst, diff, grad);
    megdnn_assert(workspace_in_bytes >= required_workspace_in_bytes);
}

}  // namespace megdnn

// vim: syntax=cpp.doxygen
