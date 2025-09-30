#pragma once
#include "megdnn/internal/opr_header_prologue.h"

namespace megdnn {

class WarpPerspectiveBase : public OperatorBase {
    DEF_OPR_IMPL_CTOR(WarpPerspectiveBase, OperatorBase);
    DEF_OPR_PARAM(WarpPerspective);

public:
    using InterpolationMode = Param::InterpolationMode;
    using BorderMode = Param::BorderMode;

protected:
    void check_layout_fwd(
            const TensorLayout& src, const TensorLayout& mat, const TensorLayout& dst) {
        check_layout_fwd(src, mat, {}, dst);
    }
    void check_layout_fwd(
            const TensorLayoutArray& srcs, const TensorLayout& mat,
            const TensorLayout& dst) {
        check_layout_fwd(srcs, mat, {}, dst);
    }

    void check_layout_fwd(
            const TensorLayout& src, const TensorLayout& mat,
            const TensorLayout& mat_idx, const TensorLayout& dst);
    void check_layout_fwd(
            const TensorLayoutArray& srcs, const TensorLayout& mat,
            const TensorLayout& mat_idx, const TensorLayout& dst);
    std::string param_msg() const;
    int get_real_coord(int p, int len);
};

class WarpPerspectiveForward : public WarpPerspectiveBase {
    DEF_OPR_IMPL(WarpPerspectiveForward, WarpPerspectiveBase, 0, 1);

public:
    /**
     * \param[in] src (n, channel, in_height, in_width)
     * \param[in] mat (n, 3, 3)
     * \param[out] dst (n, channel, out_height, out_width)
     *
     * \see
     * http://docs.opencv.org/2.4/modules/imgproc/doc/geometric_transformations.html?highlight=warpaffine
     *
     * denominator = mat[2][0]*w+mat[2][1]*h+mat[2][2]
     * dst(h, w) = src((mat[1][0]*w+mat[1][1]*h+mat[1][2])/denominator,
     *                 (mat[0][0]*w+mat[0][1]*h+mat[0][2])/denominator)
     *
     * src and dst can have different shapes, as long as their n and c agree.
     * src, mat and dst should be contiguous.
     */
    void exec(
            _megdnn_tensor_in src, _megdnn_tensor_in mat, _megdnn_tensor_out dst,
            _megdnn_workspace workspace) {
        exec(src, mat, {}, dst, workspace);
    }
    /**
     * \param[in] srcs consists of n TensorNDs, each TensorND has shape (1, channel,
     * in_height, in_width) \param[in] mat (n, 3, 3) \param[out] dst (n, channel,
     * out_height, out_width)
     *
     * \note
     * srcs and dst can have different shapes, as long as their c agree and the size of
     * srcs is equal to n. every element of srcs, mat and dst should be contiguous.
     *
     * equivalent to:
     * TensorND src{nullptr, TensorLayout({n, channel, in_height, in_width},
     * srcs[0].layout.dtype)}; auto concat = handle()->create_operator<Concat>();
     * concat->exec(srcs, src);
     * auto warp = handle()->create_operator<WarpPerspectiveForward>();
     * warp->exec(src, mat, dst, workspace);
     */
    void exec(
            _megdnn_in const TensorNDArray& srcs, _megdnn_tensor_in mat,
            _megdnn_tensor_out dst, _megdnn_workspace workspace) {
        exec(srcs, mat, {}, dst, workspace);
    }

    /**
     * \p src should have batch size m, and \p mat and \p mat_idx should
     * both have batch size n. Each item in \p mat_idx must be in the range
     * of [0, m-1].
     *
     * \param mat_idx the indices of input image that each matrix in \p mat
     *      should act on. It can also be empty and in such case \p mat
     *      should have the same batch size as \p src.
     */
    virtual void exec(
            _megdnn_tensor_in src, _megdnn_tensor_in mat, _megdnn_tensor_in mat_idx,
            _megdnn_tensor_out dst, _megdnn_workspace workspace) = 0;
    /**
     * \p srcs should have m elements, and \p mat and \p mat_idx should
     * both have batch size n. Each item in \p mat_idx must be in the range
     * of [0, m-1].
     *
     * \param mat_idx the indices of input image that each matrix in \p mat
     *      should act on. It can also be empty and in such case \p mat batch size
     *      should be the same as the number of elements in \p srcs .
     */
    virtual void exec(
            _megdnn_in const TensorNDArray& srcs, _megdnn_tensor_in mat,
            _megdnn_tensor_in mat_idx, _megdnn_tensor_out dst,
            _megdnn_workspace workspace) {
        static_cast<void>(srcs);
        static_cast<void>(mat);
        static_cast<void>(mat_idx);
        static_cast<void>(dst);
        static_cast<void>(workspace);
    }

    size_t get_workspace_in_bytes(
            const TensorLayout& src, const TensorLayout& mat, const TensorLayout& dst) {
        return get_workspace_in_bytes(src, mat, {}, dst);
    }

    size_t get_workspace_in_bytes(
            const TensorLayoutArray& srcs, const TensorLayout& mat,
            const TensorLayout& dst) {
        return get_workspace_in_bytes(srcs, mat, {}, dst);
    }

    virtual size_t get_workspace_in_bytes(
            const TensorLayout& src, const TensorLayout& mat,
            const TensorLayout& mat_idx, const TensorLayout& dst) = 0;

    virtual size_t get_workspace_in_bytes(
            const TensorLayoutArray& srcs, const TensorLayout& mat,
            const TensorLayout& mat_idx, const TensorLayout& dst) {
        static_cast<void>(srcs);
        static_cast<void>(mat);
        static_cast<void>(mat_idx);
        static_cast<void>(dst);
        return 0;
    }

protected:
    void check_exec(
            const TensorLayout& src, const TensorLayout& mat,
            const TensorLayout& mat_idx, const TensorLayout& dst,
            size_t workspace_in_bytes);

    void check_exec_allow_nhwc_mat_idx(
            const TensorLayout& src, const TensorLayout& mat,
            const TensorLayout& mat_idx, const TensorLayout& dst,
            size_t workspace_in_bytes);
    void check_exec_allow_nhwc_mat_idx(
            const TensorLayoutArray& srcs, const TensorLayout& mat,
            const TensorLayout& mat_idx, const TensorLayout& dst,
            size_t workspace_in_bytes);
};
using WarpPerspective = WarpPerspectiveForward;

class WarpPerspectiveBackwardData : public WarpPerspectiveBase {
    DEF_OPR_IMPL(WarpPerspectiveBackwardData, WarpPerspectiveBase, 2, 1);

public:
    /**
     * \param[in] mat the `mat' parameter in WarpPerspectiveForward::exec
     * \param[in] diff the backpropagated gradient wrt. dst
     * \param[out] grad the backpropagated gradient wrt. src
     * \param[out] workspace temporary workspace to perform backward
     */
    void exec(
            _megdnn_tensor_in mat, _megdnn_tensor_in diff, _megdnn_tensor_out grad,
            _megdnn_workspace workspace) {
        exec(mat, {}, diff, grad, workspace);
    }

    virtual void exec(
            _megdnn_tensor_in mat, _megdnn_tensor_in mat_idx, _megdnn_tensor_in diff,
            _megdnn_tensor_out grad, _megdnn_workspace workspace) = 0;

    size_t get_workspace_in_bytes(
            const TensorLayout& mat, const TensorLayout& diff,
            const TensorLayout& grad) {
        return get_workspace_in_bytes(mat, {}, diff, grad);
    }

    virtual size_t get_workspace_in_bytes(
            const TensorLayout& mat, const TensorLayout& mat_idx,
            const TensorLayout& diff, const TensorLayout& grad) = 0;

protected:
    void check_exec(
            const TensorLayout& mat, const TensorLayout& mat_idx,
            const TensorLayout& diff, const TensorLayout& grad,
            size_t workspace_in_bytes);
};

class WarpPerspectiveBackwardMat : public WarpPerspectiveBase {
    DEF_OPR_IMPL(WarpPerspectiveBackwardMat, WarpPerspectiveBase, 3, 1);

public:
    /**
     * \param[in] src the `src' parameter in WarpPerspectiveForward::exec
     * \param[in] mat the `mat' parameter in WarpPerspectiveForward::exec
     * \param[in] diff the backpropagated gradient wrt. dst
     * \param[out] grad the backpropagated gradient wrt. mat
     * \param[out] workspace temporary workspace to perform backward
     */
    void exec(
            _megdnn_tensor_in src, _megdnn_tensor_in mat, _megdnn_tensor_in diff,
            _megdnn_tensor_out grad, _megdnn_workspace workspace) {
        exec(src, mat, {}, diff, grad, workspace);
    }

    virtual void exec(
            _megdnn_tensor_in src, _megdnn_tensor_in mat, _megdnn_tensor_in mat_idx,
            _megdnn_tensor_in diff, _megdnn_tensor_out grad,
            _megdnn_workspace workspace) = 0;

    size_t get_workspace_in_bytes(
            const TensorLayout& src, const TensorLayout& mat, const TensorLayout& diff,
            const TensorLayout& grad) {
        return get_workspace_in_bytes(src, mat, {}, diff, grad);
    }

    virtual size_t get_workspace_in_bytes(
            const TensorLayout& src, const TensorLayout& mat,
            const TensorLayout& mat_idx, const TensorLayout& diff,
            const TensorLayout& grad) = 0;

protected:
    void check_exec(
            const TensorLayout& src, const TensorLayout& mat,
            const TensorLayout& mat_idx, const TensorLayout& diff,
            const TensorLayout& grad, size_t workspace_in_bytes);
};

class DctChannelSelectForward : public OperatorBase {
    DEF_OPR_PARAM(DctChannelSelect);
    DEF_OPR_IMPL(DctChannelSelectForward, OperatorBase, 3, 1);

public:
    /**
     * \param[in] DctChannelSelectForward input, must be uint8 nchw tensor
     * \param[in] mask_offset input, must be int32 nchw tensor
     * \param[in] mask_val input, must be int32 nchw tensor
     * \param[dst] DctChannelSelectForward output, default fp32 nchw tensor
     * \param[out] workspace temporary workspace to perform forward
     */
    virtual void exec(
            _megdnn_tensor_in src, _megdnn_tensor_in mask_offset,
            _megdnn_tensor_in mask_val, _megdnn_tensor_out dst,
            _megdnn_workspace workspace) = 0;

    void deduce_layout(
            const TensorLayout& src, const TensorLayout& mask_offset,
            const TensorLayout& mask_val, TensorLayout& dst);

    virtual size_t get_workspace_in_bytes(
            const TensorLayout& src, const TensorLayout& mask_offset,
            const TensorLayout& mask_val, const TensorLayout& dst) = 0;

protected:
    void check_layout_fwd(
            const TensorLayout& src, const TensorLayout& mask_offset,
            const TensorLayout& mask_val, const TensorLayout& dst);

    void deduce_layout_fwd(
            const TensorLayout& src, const TensorLayout& mask_offset,
            const TensorLayout& mask_val, TensorLayout& dst);

    std::string param_msg() const;
};

}  // namespace megdnn

#include "megdnn/internal/opr_header_epilogue.h"

// vim: syntax=cpp.doxygen
