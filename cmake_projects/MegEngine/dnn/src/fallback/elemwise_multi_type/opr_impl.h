#pragma once

#include "src/naive/elemwise_multi_type/opr_impl.h"

namespace megdnn {
namespace fallback {

class ElemwiseMultiTypeImpl : public naive::ElemwiseMultiTypeImpl {
    template <typename ctype>
    void dispatch_fma3_iXxf32xf32xi8_bcast_1x(
            const ElemwiseOpParamN<3>& param, const Broadcast1xInfo& binfo,
            const TensorND& dst);
    template <typename ctype, typename dst_ctype>
    void dispatch_round_shr_saturate_iXxi8xiX_bcast_scalar(
            const ElemwiseOpParamN<2>& param, const TensorND& dst);

    template <typename ctype>
    void dispatch_fuse_add_rmulh_round_shr_saturate_bcast_1c11(
            const ElemwiseOpParamN<6>& param, const TensorND& dst,
            const BroadcastChannelInfo& broadcast_info);

protected:
    void on_fuse_mul_add3_int16x32x32x32(
            const ElemwiseOpParamN<3>& param, const TensorND& dst) override;
    void on_fuse_mul_add3_iXxf32xf32xi8(
            const ElemwiseOpParamN<3>& param, const TensorND& dst) override;
    void on_round_shr_saturate_iXxi8xi8(
            const ElemwiseOpParamN<2>& param, const TensorND& dst) override;
    void on_fuse_add_rmulh_round_shr_saturate_int16x16x16x8(
            const ElemwiseOpParamN<6>& param, const TensorND& dst) override;
    void on_fuse_add_rmulh_round_shr_saturate_int32x32x32x8(
            const ElemwiseOpParamN<6>& param, const TensorND& dst) override;
    void on_round_shr_saturate_iXxi8xi16(
            const ElemwiseOpParamN<2>& param, const TensorND& dst) override;
    void on_fuse_mul_add3_int16xf32xf32xf32(
            const ElemwiseOpParamN<3>& param, const TensorND& dst) override;
    void on_mul_int16xf32xf32(
            const ElemwiseOpParamN<2>& param, const TensorND& dst) override;
    void on_fuse_mul_add3_uint8xf32xf32xf32(
            const ElemwiseOpParamN<3>& param, const TensorND& dst) override;

    void on_quantized_mode(
            const ElemwiseOpParamN<1>& param, const TensorND& dst,
            Elemwise::Mode mode) override;

    void on_quantized_mode(
            const ElemwiseOpParamN<2>& param, const TensorND& dst,
            Elemwise::Mode mode) override;

    void on_quantized_mode(
            const ElemwiseOpParamN<3>& param, const TensorND& dst,
            Elemwise::Mode mode) override;

public:
    using naive::ElemwiseMultiTypeImpl::ElemwiseMultiTypeImpl;
};

}  // namespace fallback
}  // namespace megdnn

// vim: syntax=cpp.doxygen
