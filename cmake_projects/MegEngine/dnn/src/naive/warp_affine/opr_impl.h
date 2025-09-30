#pragma once
#include "megdnn/oprs.h"
#include "src/common/utils.h"

namespace megdnn {
namespace naive {

class WarpAffineImpl : public WarpAffine {
public:
    using Format = Param::Format;
    template <typename ctype, typename mtype>
    struct KernParam {
        Format format;
        size_t n_src, n_mat, c, ih, iw, oh, ow;
        RefPtr src_ptr, dst_ptr, mat_ptr;
        Workspace workspace;

        static KernParam from_tensors(
                Format format, _megdnn_tensor_in src, _megdnn_tensor_in mat,
                _megdnn_tensor_out dst, _megdnn_workspace workspace) {
            KernParam ret;
            ret.format = format;
            ret.n_src = src.layout.shape[0];
            ret.n_mat = ret.n_src;
            if (format == Format::NCHW) {
                ret.c = src.layout.shape[1], ret.ih = src.layout.shape[2];
                ret.iw = src.layout.shape[3];
                ret.oh = dst.layout.shape[2];
                ret.ow = dst.layout.shape[3];
            } else if (format == Format::NHWCD4) {
                ret.c = src.layout.shape[2];
                ret.ih = src.layout.shape[1];
                ret.iw = src.layout.shape[3];
                ret.oh = dst.layout.shape[1];
                ret.ow = dst.layout.shape[3];
            } else {
                megdnn_assert(format == Format::NHWC);
                ret.c = src.layout.shape[3], ret.ih = src.layout.shape[1];
                ret.iw = src.layout.shape[2];
                ret.oh = dst.layout.shape[1];
                ret.ow = dst.layout.shape[2];
            }
            if (src.layout.dtype.enumv() == DTypeEnum::Float32 ||
                DNN_FLOAT16_SELECT(
                        src.layout.dtype.enumv() == DTypeEnum::Float16, false) ||
                src.layout.dtype.enumv() == DTypeEnum::Int8 ||
                src.layout.dtype.enumv() == DTypeEnum::Uint8 ||
                src.layout.dtype.enumv() == DTypeEnum::QuantizedS8 ||
                src.layout.dtype.enumv() == DTypeEnum::Quantized8Asymm) {
                ret.src_ptr = src.get_ref_ptr();
                ret.mat_ptr = mat.get_ref_ptr();
                ret.dst_ptr = dst.get_ref_ptr();
            } else {
                ret.src_ptr = nullptr;
                ret.mat_ptr = nullptr;
                ret.dst_ptr = nullptr;
            }
            ret.workspace = workspace;
            return ret;
        }
    };

    // ctype: C type of input data type.
    // mtype: C type of transformation matrix data type.
    template <typename ctype, typename mtype>
    void kern_naive(const KernParam<ctype, mtype>& kern_param, size_t task_id);

    using WarpAffine::WarpAffine;

    void exec(
            _megdnn_tensor_in src, _megdnn_tensor_in mat, _megdnn_tensor_in dst,
            _megdnn_workspace workspace) override;

    size_t get_workspace_in_bytes(
            const TensorLayout&, const TensorLayout&, const TensorLayout&) override {
        return 0;
    }

private:
    template <typename ctype, typename mtype>
    void kern_naive_nhwc(const KernParam<ctype, mtype>& kern_param, size_t task_id);
    template <typename ctype, typename mtype>
    void kern_naive_nhwcd4(const KernParam<ctype, mtype>& kern_param, size_t task_id);
};

#define UNPACK_WARP_AFFINE_FWD_KERN_PARAM(p)                                         \
    auto N_SRC = p.n_src, N_MAT = p.n_mat, C = p.c, IH = p.ih, IW = p.iw, OH = p.oh, \
         OW = p.ow;                                                                  \
    ctype* __restrict sptr = static_cast<ctype*>(p.src_ptr.get_ptr());               \
    mtype* __restrict mptr = static_cast<mtype*>(p.mat_ptr.get_ptr());               \
    ctype* __restrict dptr = static_cast<ctype*>(p.dst_ptr.get_ptr());

}  // namespace naive
}  // namespace megdnn

// vim: syntax=cpp.doxygen
