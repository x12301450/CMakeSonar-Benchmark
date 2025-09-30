/**
 * \file dnn/src/fallback/elemwise_helper/kimpl/relu.h
 */
#pragma once

#include "src/fallback/elemwise_helper/kimpl/op_base.h"

namespace megdnn {
namespace fallback {

template <typename src_ctype, typename dst_ctype = src_ctype>
struct ReluOpBase : UnaryOpBase<src_ctype, dst_ctype> {
    using UnaryOpBase<src_ctype, dst_ctype>::UnaryOpBase;
    void operator()(const src_ctype& src, dst_ctype* dst) const {
        *dst = operator()(src);
    }
    dst_ctype operator()(const src_ctype& src) const { return src > 0 ? src : 0; }
};

template <typename src_ctype, typename dst_type = src_ctype>
struct ReluOp;

#define OP(_ctype, _simd_type, _simd_type2, _func_suffix, _simd_width, zero_num)    \
    template <>                                                                     \
    struct ReluOp<_ctype> : ReluOpBase<_ctype> {                                    \
        using ReluOpBase::ReluOpBase;                                               \
        using ReluOpBase::operator();                                               \
        constexpr static size_t SIMD_WIDTH = _simd_width;                           \
        void operator()(const _simd_type2& src, _ctype* dst) const {                \
            auto vitem = operator()(src);                                           \
            GiStore##_func_suffix(dst, GiGetSubVector##_func_suffix##V2(vitem, 0)); \
            GiStore##_func_suffix(                                                  \
                    dst + SIMD_WIDTH, GiGetSubVector##_func_suffix##V2(vitem, 1));  \
        }                                                                           \
        _simd_type2 operator()(const _simd_type2& src) const {                      \
            _simd_type zero = GiBroadcast##_func_suffix(zero_num);                  \
            auto vitem0 = GiMaximum##_func_suffix(                                  \
                    GiGetSubVector##_func_suffix##V2(src, 0), zero);                \
            auto vitem1 = GiMaximum##_func_suffix(                                  \
                    GiGetSubVector##_func_suffix##V2(src, 1), zero);                \
            _simd_type2 ret;                                                        \
            GiSetSubVector##_func_suffix##V2(ret, 0, vitem0);                       \
            GiSetSubVector##_func_suffix##V2(ret, 1, vitem1);                       \
            return ret;                                                             \
        }                                                                           \
        void operator()(const _simd_type& src, _ctype* dst) const {                 \
            auto vitem = operator()(src);                                           \
            GiStore##_func_suffix(dst, vitem);                                      \
        }                                                                           \
        _simd_type operator()(const _simd_type& src) const {                        \
            _simd_type zero = GiBroadcast##_func_suffix(zero_num);                  \
            return GiMaximum##_func_suffix(src, zero);                              \
        }                                                                           \
    };

OP(dt_float32, GI_FLOAT32_t, GI_FLOAT32_V2_t, Float32, GI_SIMD_LEN_BYTE / sizeof(float),
   0.0f)
#if defined(GI_SUPPORT_F16)
OP(gi_float16_t, GI_FLOAT16_t, GI_FLOAT16_V2_t, Float16,
   GI_SIMD_LEN_BYTE / sizeof(gi_float16_t), 0.0)
#endif
OP(dt_int32, GI_INT32_t, GI_INT32_V2_t, Int32, GI_SIMD_LEN_BYTE / sizeof(int32_t), 0)
OP(dt_int8, GI_INT8_t, GI_INT8_V2_t, Int8, GI_SIMD_LEN_BYTE / sizeof(int8_t), 0)
#undef OP

template <>
struct ReluOpBase<dt_qint8, dt_qint8> : UnaryOpBase<dt_qint8, dt_qint8> {
    using UnaryOpBase::UnaryOpBase;
    void operator()(const dt_qint8& src, dt_qint8* dst) const {
        *dst = operator()(src);
    }
    dt_qint8 operator()(const dt_qint8& src) const {
        float fsrc = src.as_int8() * this->scale;
        fsrc = std::max<float>(fsrc, 0.f);
        return QConverter::convert<dt_qint8, float>(fsrc);
    }
};

template <>
struct ReluOp<dt_qint8, dt_qint8> : ReluOpBase<dt_qint8, dt_qint8> {
    using ReluOpBase::ReluOpBase;
    constexpr static size_t SIMD_WIDTH = GI_SIMD_LEN_BYTE / sizeof(int8_t);
    using ReluOpBase::operator();

    void operator()(const GI_INT8_V2_t& vsrc, dt_qint8* dst) const {
        OPERATOR_UNARY_QINT8_FALLBACK;
    }
    GI_INT8_t operator()(const GI_INT32_V2_t& vsrc) const {
        GI_FLOAT32_t vfzero = GiBroadcastFloat32(0.0f);
        auto vitem0 = GiMultiplyFloat32(
                GiCastToFloat32(GiGetSubVectorInt32V2(vsrc, 0)),
                GiFixLenType2GiFloat32Type(this->vscale));
        auto vitem1 = GiMultiplyFloat32(
                GiCastToFloat32(GiGetSubVectorInt32V2(vsrc, 1)),
                GiFixLenType2GiFloat32Type(this->vscale));
        vitem0 = GiMaximumFloat32(vitem0, vfzero);
        vitem1 = GiMaximumFloat32(vitem1, vfzero);
        GI_FLOAT32_V2_t tmp;
        GiSetSubVectorFloat32V2(tmp, 0, vitem0);
        GiSetSubVectorFloat32V2(tmp, 1, vitem1);
        return QConverter::convert<GI_INT8_t, GI_FLOAT32_V2_t>(tmp);
    }
};

template <>
struct ReluOpBase<dt_qint32, dt_qint8> : UnaryOpBase<dt_qint32, dt_qint8> {
    using UnaryOpBase::UnaryOpBase;
    void operator()(const dt_qint32& src, dt_qint8* dst) const {
        *dst = operator()(src);
    }

    dt_qint8 operator()(const dt_qint32& src) const {
        float fsrc = src.as_int32() * this->scale;
        fsrc = std::max<float>(fsrc, 0.f);
        return QConverter::convert<dt_qint8, float>(fsrc);
    }
};

//! if old armv7, special define relu with fixup
#if defined(__ARM_ARCH) && __ARM_ARCH < 8
template <>
struct ReluOp<dt_qint32, dt_qint8> : ReluOpBase<dt_qint32, dt_qint8>, FixupBase {
    using ReluOpBase::operator();
    constexpr static size_t SIMD_WIDTH = 4;
    GI_INT32_t vzero = GiBroadcastInt32(0);
    GI_FLOAT32_t vfzero = GiBroadcastFloat32(0.0f);

    ReluOp(DType src_dtype, DType dst_dtype)
            : ReluOpBase(src_dtype, dst_dtype), FixupBase(scale) {}

    ReluOp(float src_scale, float dst_scale)
            : ReluOpBase(src_scale, dst_scale), FixupBase(scale) {}

    void operator()(const int32x4x2_t& vsrc, dt_qint8* dst) const {
        vst1_s8(reinterpret_cast<int8_t*>(dst), vget_low_s8(operator()(vsrc)));
    }
    int8x16_t operator()(const int32x4x2_t& vsrc) const {
        int32x4_t vitem0 = vqrdmulhq_s32(GiGetSubVectorInt32V2(vsrc, 0), vmultiplier);
        int32x4_t vitem1 = vqrdmulhq_s32(GiGetSubVectorInt32V2(vsrc, 1), vmultiplier);
        vitem0 = vmaxq_s32(vitem0, vzero);
        vitem1 = vmaxq_s32(vitem1, vzero);
        auto tmp = vqmovn_s16(vcombine_s16(
                vqmovn_s32(vrshlq_s32(vitem0, vshift)),
                vqmovn_s32(vrshlq_s32(vitem1, vshift))));
        return vcombine_s8(tmp, tmp);
    }
    int8x16_t operator()(const float32x4_t& vsrc) const {
        int32x4_t vitem0 = vqrdmulhq_s32(vcvtq_s32_f32(vsrc), vmultiplier);
        vitem0 = vmaxq_s32(vitem0, vzero);
        vitem0 = vrshlq_s32(vitem0, vshift);
        int16x4_t vitem = vqmovn_s32(vitem0);
        auto tmp = vqmovn_s16(vcombine_s16(vitem, vitem));
        return vcombine_s8(tmp, tmp);
    }
    void operator()(const int32x4_t& src, dt_qint8* dst) const {
        auto vitem0 = vmulq_f32(vcvtq_f32_s32(src), this->vscale);
        vitem0 = vmaxq_f32(vitem0, vfzero);
        auto result = QConverter::convert<int8x16_t, float32x4_t>(vitem0);
        vst1q_lane_s32(reinterpret_cast<int32_t*>(dst), (int32x4_t)result, 0);
    }
    void operator()(const float32x4_t& src, dt_qint8* dst) const {
        auto vitem0 = vmulq_f32(src, this->vscale);
        vitem0 = vmaxq_f32(vitem0, vfzero);
        auto result = QConverter::convert<int8x16_t, float32x4_t>(vitem0);
        vst1q_lane_s32(reinterpret_cast<int32_t*>(dst), (int32x4_t)result, 0);
    }
};

#else
template <>
struct ReluOp<dt_qint32, dt_qint8> : ReluOpBase<dt_qint32, dt_qint8> {
    using ReluOpBase::ReluOpBase;
    using ReluOpBase::operator();
    constexpr static size_t SIMD_WIDTH = GI_SIMD_LEN_BYTE / sizeof(int32_t);

    void operator()(const GI_INT32_V2_t& vsrc, dt_qint8* dst) const {
        GiStoreLowInt8(reinterpret_cast<int8_t*>(dst), operator()(vsrc));
    }
    void operator()(const GI_INT32_t& src, dt_qint8* dst) const {
        GiStoreLane0Int32(
                reinterpret_cast<int32_t*>(dst),
                GiReinterpretInt8AsInt32(operator()(src)));
    }

    GI_INT8_t operator()(const GI_INT32_V2_t& vsrc) const {
        auto vitem0 = GiMultiplyFloat32(
                GiCastToFloat32(GiGetSubVectorInt32V2(vsrc, 0)),
                GiFixLenType2GiFloat32Type(this->vscale));
        auto vitem1 = GiMultiplyFloat32(
                GiCastToFloat32(GiGetSubVectorInt32V2(vsrc, 1)),
                GiFixLenType2GiFloat32Type(this->vscale));
        GI_FLOAT32_t vfzero = GiBroadcastFloat32(0.0f);
        vitem0 = GiMaximumFloat32(vitem0, vfzero);
        vitem1 = GiMaximumFloat32(vitem1, vfzero);

        GI_FLOAT32_V2_t tmp;
        GiSetSubVectorFloat32V2(tmp, 0, vitem0);
        GiSetSubVectorFloat32V2(tmp, 1, vitem1);
        return QConverter::convert<GI_INT8_t, GI_FLOAT32_V2_t>(tmp);
    }
    GI_INT8_t operator()(const GI_INT32_t& src) const {
        auto vitem0 = GiMultiplyFloat32(
                GiCastToFloat32(src), GiFixLenType2GiFloat32Type(this->vscale));
        GI_FLOAT32_t vfzero = GiBroadcastFloat32(0.0f);
        vitem0 = GiMaximumFloat32(vitem0, vfzero);
        return QConverter::convert<GI_INT8_t, GI_FLOAT32_t>(vitem0);
    }
    GI_INT8_t operator()(const GI_FLOAT32_t& src) const {
        auto vitem0 = GiMultiplyFloat32(src, GiFixLenType2GiFloat32Type(this->vscale));
        GI_FLOAT32_t vfzero = GiBroadcastFloat32(0.0f);
        vitem0 = GiMaximumFloat32(vitem0, vfzero);
        return QConverter::convert<GI_INT8_t, GI_FLOAT32_t>(vitem0);
    }
};

#endif

}  // namespace fallback
}  // namespace megdnn

// vim: syntax=cpp.doxygen
