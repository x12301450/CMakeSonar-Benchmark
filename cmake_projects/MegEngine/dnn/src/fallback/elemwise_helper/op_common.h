#pragma once

#include "megdnn/dtype.h"
#include "src/fallback/elemwise_helper/kimpl/pow.h"

namespace megdnn {
namespace elemwise {

/*!
 * \brief broadcast type
 * BCAST_x[0]x[1]...: x[i] == !stride[i]
 */
enum BcastType {
    VEC,
    VEC_VEC,
    VEC_BCAST101,
    VEC_BCASTX0X,
    VEC_BCAST111C,
    VEC_BCAST101xX,
    VEC_SCALAR,
    SCALAR_VEC,
    BCAST101_VEC,
    BCASTX0X_VEC,
    BCAST111C_VEC,
    BCAST101xX_VEC,
    VEC_VEC_VEC,
    VEC_VEC_SCALAR,
    BCAST101_VEC_BCAST101,
    BCAST111C_VEC_BCAST111C,
    BCAST101xX_VEC_BCAST101xX,
    VEC_BCAST101_VEC,
    VEC_BCAST111C_VEC,
    VEC_BCAST101xX_VEC,
    VEC_SCALAR_VEC,
    VEC_SCALAR_SCALAR,
    UNKNOWN_BCAST_TYPE
};

///////////////////////////////// ParamElemVistor v2///////////////////////////
template <typename ctype>
struct ParamElemVisitorV2;

//! visitor single elemwise, and dup to vector
template <typename ctype>
struct ParamElemVisitorDupV2;

template <typename ctype>
struct ParamElemVisitorBcast101x4V2;

#define cb(_ctype, _inner_ctype, _simd_type, _fun_suffix, _simd_type_v2)         \
    template <>                                                                  \
    struct ParamElemVisitorV2<_ctype> {                                          \
        _simd_type_v2 operator()(const _ctype* src, const _ctype* src_1) const { \
            _simd_type_v2 ret;                                                   \
            GiSetSubVector##_fun_suffix##V2(ret, 0, GiLoad##_fun_suffix(src));   \
            GiSetSubVector##_fun_suffix##V2(ret, 1, GiLoad##_fun_suffix(src_1)); \
            return ret;                                                          \
        }                                                                        \
    };                                                                           \
    template <>                                                                  \
    struct ParamElemVisitorDupV2<_ctype> {                                       \
        _simd_type_v2 operator()(const _ctype* src) const {                      \
            _simd_type_v2 ret;                                                   \
            _simd_type tmp = GiBroadcast##_fun_suffix(                           \
                    *reinterpret_cast<const _inner_ctype*>(src));                \
            GiSetSubVector##_fun_suffix##V2(ret, 0, tmp);                        \
            GiSetSubVector##_fun_suffix##V2(ret, 1, tmp);                        \
            return ret;                                                          \
        }                                                                        \
    }
cb(dt_qint32, int32_t, GI_INT32_t, Int32, GI_INT32_V2_t);
cb(dt_qint8, int8_t, GI_INT8_t, Int8, GI_INT8_V2_t);

cb(dt_float32, float, GI_FLOAT32_t, Float32, GI_FLOAT32_V2_t);
cb(dt_int32, int32_t, GI_INT32_t, Int32, GI_INT32_V2_t);
cb(dt_int8, int8_t, GI_INT8_t, Int8, GI_INT8_V2_t);
#undef cb

template <typename ctype>
struct ParamElemVisitorBcast101x4V2;
#define cb(_ctype, _inner_ctype, _simd_type, _fun_suffix, rel_suffix, _simd_type_v2) \
    template <>                                                                      \
    struct ParamElemVisitorBcast101x4V2<_ctype> {                                    \
        _simd_type_v2 operator()(const _ctype* src) const {                          \
            _simd_type_v2 ret;                                                       \
            _simd_type tmp =                                                         \
                    GiReinter##rel_suffix##To##_fun_suffix(GiBroadcast##rel_suffix(  \
                            *reinterpret_cast<const _inner_ctype*>(src)));           \
            GiSetSubVector##_fun_suffix##V2(ret, 0, tmp);                            \
            GiSetSubVector##_fun_suffix##V2(ret, 1, tmp);                            \
            return ret;                                                              \
        }                                                                            \
    }

cb(dt_qint8, int32_t, GI_INT8_t, Int8, Int32, GI_INT8_V2_t);
cb(dt_int8, int32_t, GI_INT8_t, Int8, Int32, GI_INT8_V2_t);
#undef cb
#define cb(_ctype, _inner_ctype, _simd_type, _fun_suffix, _simd_type_v2) \
    template <>                                                          \
    struct ParamElemVisitorBcast101x4V2<_ctype> {                        \
        _simd_type_v2 operator()(const _ctype* src) const {              \
            _simd_type_v2 ret;                                           \
            _simd_type tmp = GiLoad##_fun_suffix(src);                   \
            GiSetSubVector##_fun_suffix##V2(ret, 0, tmp);                \
            GiSetSubVector##_fun_suffix##V2(ret, 1, tmp);                \
            return ret;                                                  \
        }                                                                \
    }

cb(dt_qint32, int32_t, GI_INT32_t, Int32, GI_INT32_V2_t);
cb(dt_float32, float, GI_FLOAT32_t, Float32, GI_FLOAT32_V2_t);
cb(dt_int32, int32_t, GI_INT32_t, Int32, GI_INT32_V2_t);
#undef cb

///////////////////////////////// OpCaller /////////////////////////////
template <typename Op, BcastType bcast_type>
struct OpCallerUnary;

template <typename Op>
struct OpCallerUnary<Op, VEC> {
    static void run(
            const typename Op::src_ctype* src, typename Op::dst_ctype* dst,
            DType src_dtype, DType dst_dtype, size_t nr_elems) {
        Op op(src_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis;
        size_t i = 0;
        for (; i + Op::SIMD_WIDTH * 2 <= nr_elems; i += Op::SIMD_WIDTH * 2) {
            op(vis(src, src + Op::SIMD_WIDTH), dst);
            src += Op::SIMD_WIDTH * 2;
            dst += Op::SIMD_WIDTH * 2;
        }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
        for (; i < nr_elems; i++) {
            op(*src, dst);
            src++;
            dst++;
        }
    }
};

template <typename Op, BcastType bcast_type, typename enbale = void>
struct OpCallerBinary;

///////////////////////// Pow ////////////////////////////////
template <typename ctype>
struct OpCallerBinary<fallback::PowOp<ctype, ctype>, VEC_VEC> {
    using Op = fallback::PowOp<ctype, ctype>;
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t nr_elems) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        size_t i = 0;
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
        for (; i < nr_elems; i++) {
            op(*src0, *src1, dst);
            src0++;
            src1++;
            dst++;
        }
    }
};

template <typename ctype>
struct OpCallerBinary<fallback::PowOp<ctype, ctype>, VEC_SCALAR> {
    using Op = fallback::PowOp<ctype, ctype>;
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t nr_elems) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        size_t i = 0;
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
        for (; i < nr_elems; i++) {
            op(*src0, src1, dst);
            src0++;
            dst++;
        }
    }
};

template <typename ctype>
struct OpCallerBinary<fallback::PowOp<ctype, ctype>, VEC_BCAST101> {
    using Op = fallback::PowOp<ctype, ctype>;
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t channel, size_t channel_stride) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        for (size_t b = 0; b < batch; b++) {
            const typename Op::src_ctype* src1_ptr = src1;
            for (size_t c = 0; c < channel; c++) {
                size_t i = 0;
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                for (; i < channel_stride; i++) {
                    op(*src0, *src1_ptr, dst);
                    src0++;
                    dst++;
                }
                src1_ptr++;
            }
        }
    }
};

template <typename ctype>
struct OpCallerBinary<fallback::PowOp<ctype, ctype>, VEC_BCASTX0X> {
    using Op = fallback::PowOp<ctype, ctype>;
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t channel, size_t channel_stride) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        for (size_t b = 0; b < batch; b++) {
            const typename Op::src_ctype* src1_ptr_base = src1 + b * channel_stride;
            for (size_t c = 0; c < channel; c++) {
                size_t i = 0;
                auto src1_ptr = src1_ptr_base;
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                for (; i < channel_stride; i++) {
                    op(*src0, *src1_ptr, dst);
                    src0++;
                    src1_ptr++;
                    dst++;
                }
            }
        }
    }
};

template <typename ctype>
struct OpCallerBinary<fallback::PowOp<ctype, ctype>, VEC_BCAST111C> {
    using Op = fallback::PowOp<ctype, ctype>;
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t channel, size_t channel_stride) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        for (size_t b = 0; b < batch; b++) {
            for (size_t c = 0; c < channel; c++) {
                size_t i = 0;
                const typename Op::src_ctype* src1_ptr = src1;
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                for (; i < channel_stride; i++) {
                    op(*src0, *src1_ptr, dst);
                    src0++;
                    src1_ptr++;
                    dst++;
                }
            }
        }
    }
};

template <typename ctype>
struct OpCallerBinary<fallback::PowOp<ctype, ctype>, BCAST111C_VEC> {
    using Op = fallback::PowOp<ctype, ctype>;
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t channel, size_t channel_stride) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        for (size_t b = 0; b < batch; b++) {
            for (size_t c = 0; c < channel; c++) {
                size_t i = 0;
                const typename Op::src_ctype* src0_ptr = src0;
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                for (; i < channel_stride; i++) {
                    op(*src0_ptr, *src1, dst);
                    src0_ptr++;
                    src1++;
                    dst++;
                }
            }
        }
    }
};

template <typename ctype>
struct OpCallerBinary<fallback::PowOp<ctype, ctype>, SCALAR_VEC> {
    using Op = fallback::PowOp<ctype, ctype>;
    static void run(
            const typename Op::src_ctype src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t nr_elems) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        size_t i = 0;
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
        for (; i < nr_elems; i++) {
            op(src0, *src1, dst);
            src1++;
            dst++;
        }
    }
};

template <typename ctype>
struct OpCallerBinary<fallback::PowOp<ctype, ctype>, BCAST101_VEC> {
    using Op = fallback::PowOp<ctype, ctype>;
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t channel, size_t channel_stride) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        for (size_t b = 0; b < batch; b++) {
            auto src0_ptr = src0;
            for (size_t c = 0; c < channel; c++) {
                size_t i = 0;
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                for (; i < channel_stride; i++) {
                    op(*src0_ptr, *src1, dst);
                    src1++;
                    dst++;
                }
                src0_ptr++;
            }
        }
    }
};

template <typename ctype>
struct OpCallerBinary<fallback::PowOp<ctype, ctype>, BCASTX0X_VEC> {
    using Op = fallback::PowOp<ctype, ctype>;
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t channel, size_t channel_stride) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        for (size_t b = 0; b < batch; b++) {
            auto src0_ptr_base = src0 + b * channel_stride;
            for (size_t c = 0; c < channel; c++) {
                size_t i = 0;
                auto src0_ptr = src0_ptr_base;
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                for (; i < channel_stride; i++) {
                    op(*src0_ptr, *src1, dst);
                    src0_ptr++;
                    src1++;
                    dst++;
                }
            }
        }
    }
};

template <typename Op>
struct OpCallerBinary<Op, VEC_VEC> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t nr_elems) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis0;
        ParamElemVisitorV2<typename Op::src_ctype> vis1;
        size_t i = 0;
        for (; i + Op::SIMD_WIDTH * 2 <= nr_elems; i += Op::SIMD_WIDTH * 2) {
            op(vis0(src0, src0 + Op::SIMD_WIDTH), vis1(src1, src1 + Op::SIMD_WIDTH),
               dst);
            src0 += Op::SIMD_WIDTH * 2;
            src1 += Op::SIMD_WIDTH * 2;
            dst += Op::SIMD_WIDTH * 2;
        }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
        for (; i < nr_elems; i++) {
            op(*src0, *src1, dst);
            src0++;
            src1++;
            dst++;
        }
    }
};

template <typename Op>
struct OpCallerBinary<Op, VEC_BCAST101> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t channel, size_t channel_stride) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis0;
        ParamElemVisitorDupV2<typename Op::src_ctype> vis1;
        for (size_t b = 0; b < batch; b++) {
            const typename Op::src_ctype* src1_ptr = src1;
            for (size_t c = 0; c < channel; c++) {
                size_t i = 0;
                auto src1_simd_v2 = vis1(src1_ptr);
                for (; i + Op::SIMD_WIDTH * 2 <= channel_stride;
                     i += Op::SIMD_WIDTH * 2) {
                    op(vis0(src0, src0 + Op::SIMD_WIDTH), src1_simd_v2, dst);
                    src0 += Op::SIMD_WIDTH * 2;
                    dst += Op::SIMD_WIDTH * 2;
                }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                for (; i < channel_stride; i++) {
                    op(*src0, *src1_ptr, dst);
                    src0++;
                    dst++;
                }
                src1_ptr++;
            }
        }
    }
};

template <typename Op>
struct OpCallerBinary<Op, VEC_BCASTX0X> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t channel, size_t channel_stride) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis;
        for (size_t b = 0; b < batch; b++) {
            const typename Op::src_ctype* src1_ptr_base = src1 + b * channel_stride;
            for (size_t c = 0; c < channel; c++) {
                size_t i = 0;
                auto src1_ptr = src1_ptr_base;
                for (; i + Op::SIMD_WIDTH * 2 <= channel_stride;
                     i += Op::SIMD_WIDTH * 2) {
                    auto src0_simd01 = vis(src0, src0 + Op::SIMD_WIDTH);
                    auto src1_simd01 = vis(src1_ptr, src1_ptr + Op::SIMD_WIDTH);
                    op(src0_simd01, src1_simd01, dst);
                    src0 += Op::SIMD_WIDTH * 2;
                    src1_ptr += Op::SIMD_WIDTH * 2;
                    dst += Op::SIMD_WIDTH * 2;
                }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                for (; i < channel_stride; i++) {
                    op(*src0, *src1_ptr, dst);
                    src0++;
                    src1_ptr++;
                    dst++;
                }
            }
        }
    }
};

template <typename Op>
struct OpCallerBinary<Op, VEC_BCAST111C> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t channel, size_t channel_stride) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis;
        for (size_t b = 0; b < batch; b++) {
            for (size_t c = 0; c < channel; c++) {
                size_t rest = channel_stride;
                const typename Op::src_ctype* src1_ptr = src1;
                while (rest >= Op::SIMD_WIDTH * 2) {
                    auto src0_simd01 = vis(src0, src0 + Op::SIMD_WIDTH);
                    auto src1_simd01 = vis(src1_ptr, src1_ptr + Op::SIMD_WIDTH);
                    src0 += Op::SIMD_WIDTH * 2;
                    src1_ptr += Op::SIMD_WIDTH * 2;
                    op(src0_simd01, src1_simd01, dst);
                    dst += Op::SIMD_WIDTH * 2;
                    rest -= Op::SIMD_WIDTH * 2;
                }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                while (rest > 0) {
                    op(*src0, *src1_ptr, dst);
                    dst++;
                    src0++;
                    src1_ptr++;
                    rest--;
                }
            }
        }
    }
};

template <typename Op>
struct OpCallerBinary<Op, BCAST111C_VEC> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t channel, size_t channel_stride) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis;
        for (size_t b = 0; b < batch; b++) {
            for (size_t c = 0; c < channel; c++) {
                size_t rest = channel_stride;
                const typename Op::src_ctype* src0_ptr = src0;
                while (rest >= Op::SIMD_WIDTH * 2) {
                    auto src0_simd01 = vis(src0_ptr, src0_ptr + Op::SIMD_WIDTH);
                    auto src1_simd01 = vis(src1, src1 + Op::SIMD_WIDTH);
                    src0_ptr += Op::SIMD_WIDTH * 2;
                    src1 += Op::SIMD_WIDTH * 2;
                    op(src0_simd01, src1_simd01, dst);
                    dst += Op::SIMD_WIDTH * 2;
                    rest -= Op::SIMD_WIDTH * 2;
                }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                while (rest > 0) {
                    op(*src0_ptr, *src1, dst);
                    dst++;
                    src0_ptr++;
                    src1++;
                    rest--;
                }
            }
        }
    }
};

template <typename ctype>
struct OpCallerBinary<fallback::PowOp<ctype, ctype>, BCAST101xX_VEC> {
    using Op = fallback::PowOp<ctype, ctype>;
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t nr_channel_blocks,
            size_t channel_stride, size_t channel_block_dim) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        for (size_t b = 0; b < batch; b++) {
            auto src0_ptr = src0;
            for (size_t cb = 0; cb < nr_channel_blocks; cb++) {
                auto src0_block_ptr = src0_ptr + cb * channel_block_dim;
                for (size_t i = 0; i < channel_stride; i++) {
                    for (size_t c_iter = 0; c_iter < channel_block_dim; c_iter++) {
                        op(*(src0_block_ptr + c_iter), *src1, dst);
                        src1++;
                        dst++;
                    }
                }
            }
        }
    }
};

template <typename src_ctype, size_t channel_block_dim>
struct OpCallerBinaryBcast101xXVec {
    template <typename Op>
    static void run(
            const src_ctype* src0, const src_ctype* src1, typename Op::dst_ctype* dst,
            const Op& op, size_t batch, size_t nr_channel_blocks,
            size_t channel_stride) {
        for (size_t b = 0; b < batch; b++) {
            auto src0_ptr = src0;
            for (size_t cb = 0; cb < nr_channel_blocks; cb++) {
                auto src0_block_ptr = src0_ptr + cb * channel_block_dim;
                for (size_t img_index = 0; img_index < channel_stride; img_index++) {
                    for (size_t c_iter = 0; c_iter < channel_block_dim; c_iter++) {
                        op(*(src0_block_ptr + c_iter), *src1, dst);
                        src1++;
                        dst++;
                    }
                }
            }
        }
    }
};

template <typename src_ctype, size_t channel_block_dim>
struct OpCallerBinaryBcast101xDVec {
    template <typename Op, typename Vis0, typename Vis1>
    static void run(
            const src_ctype* src0, const src_ctype* src1, typename Op::dst_ctype* dst,
            const Op& op, const Vis0& vis0, const Vis1& vis1, size_t batch,
            size_t nr_channel_blocks, size_t channel_stride) {
        for (size_t b = 0; b < batch; b++) {
            auto src0_ptr = src0;
            for (size_t cb = 0; cb < nr_channel_blocks; cb++) {
                auto src0_block_ptr = src0_ptr + cb * channel_block_dim;
                auto channel_block_vec_v2 = vis0(src0_block_ptr);
                size_t img_index = 0;
                auto src1_offset = Op::SIMD_WIDTH / channel_block_dim;
                for (; img_index + 2 * src1_offset <= channel_stride;
                     img_index += 2 * src1_offset) {
                    op(channel_block_vec_v2, vis1(src1, src1 + Op::SIMD_WIDTH), dst);
                    src1 += Op::SIMD_WIDTH * 2;
                    dst += Op::SIMD_WIDTH * 2;
                }
                // TODO:all elemwise_multi_type op imp one simd mode
                for (; img_index < channel_stride; img_index++) {
                    for (size_t c_iter = 0; c_iter < channel_block_dim; c_iter++) {
                        op(*(src0_block_ptr + c_iter), *src1, dst);
                        src1++;
                        dst++;
                    }
                }
            }
        }
    }
};

template <typename src_ctype>
struct OpCallerBinaryBcast101xXVec<src_ctype, 4> {
    template <typename Op>
    static void run(
            const src_ctype* src0, const src_ctype* src1, typename Op::dst_ctype* dst,
            const Op& op, size_t batch, size_t nr_channel_blocks,
            size_t channel_stride) {
        ParamElemVisitorBcast101x4V2<typename Op::src_ctype> vis0;
        ParamElemVisitorV2<typename Op::src_ctype> vis1;
        OpCallerBinaryBcast101xDVec<src_ctype, 4>::run(
                src0, src1, dst, op, vis0, vis1, batch, nr_channel_blocks,
                channel_stride);
    }
};

template <typename Op>
struct OpCallerBinary<Op, BCAST101xX_VEC> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t nr_channel_blocks,
            size_t channel_stride, size_t channel_block_dim) {
        megdnn_assert(
                channel_block_dim == 4 || channel_block_dim == 8,
                "only imp for nchw44/nchw88");
        Op op(src0_dtype, src1_dtype, dst_dtype);
        if (channel_block_dim == 4) {
            OpCallerBinaryBcast101xXVec<typename Op::src_ctype, 4>::run(
                    src0, src1, dst, op, batch, nr_channel_blocks, channel_stride);
        } else {
            OpCallerBinaryBcast101xXVec<typename Op::src_ctype, 8>::run(
                    src0, src1, dst, op, batch, nr_channel_blocks, channel_stride);
        }
    }
};

template <typename ctype>
struct OpCallerBinary<fallback::PowOp<ctype, ctype>, VEC_BCAST101xX> {
    using Op = fallback::PowOp<ctype, ctype>;
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t nr_channel_blocks,
            size_t channel_stride, size_t channel_block_dim) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        for (size_t b = 0; b < batch; b++) {
            auto src1_ptr = src1;
            for (size_t cb = 0; cb < nr_channel_blocks; cb++) {
                auto src1_block_ptr = src1_ptr + cb * channel_block_dim;
                for (size_t i = 0; i < channel_stride; i++) {
                    for (size_t c_iter = 0; c_iter < channel_block_dim; c_iter++) {
                        op(*(src0), *(src1_block_ptr + c_iter), dst);
                        src0++;
                        dst++;
                    }
                }
            }
        }
    }
};

template <typename src_ctype, size_t channel_block_dim>
struct OpCallerBinaryVecBcast101xX {
    template <typename Op>
    static void run(
            const src_ctype* src0, const src_ctype* src1, typename Op::dst_ctype* dst,
            const Op& op, size_t batch, size_t nr_channel_blocks,
            size_t channel_stride) {
        for (size_t b = 0; b < batch; b++) {
            auto src1_ptr = src1;
            for (size_t cb = 0; cb < nr_channel_blocks; cb++) {
                auto src1_block_ptr = src1_ptr + cb * channel_block_dim;
                for (size_t img_index = 0; img_index < channel_stride; img_index++) {
                    for (size_t c_iter = 0; c_iter < channel_block_dim; c_iter++) {
                        op(*src0, *(src1_block_ptr + c_iter), dst);
                        src0++;
                        dst++;
                    }
                }
            }
        }
    }
};

template <typename src_ctype, size_t channel_block_dim>
struct OpCallerBinaryVecBcast101xD {
    template <typename Op, typename Vis0, typename Vis1>
    static void run(
            const src_ctype* src0, const src_ctype* src1, typename Op::dst_ctype* dst,
            const Op& op, const Vis0& vis0, const Vis1& vis1, size_t batch,
            size_t nr_channel_blocks, size_t channel_stride) {
        for (size_t b = 0; b < batch; b++) {
            auto src1_ptr = src1;
            for (size_t cb = 0; cb < nr_channel_blocks; cb++) {
                auto src1_block_ptr = src1_ptr + cb * channel_block_dim;
                auto channel_block_vec_v2 = vis1(src1_block_ptr);
                size_t img_index = 0;
                auto src0_offset = Op::SIMD_WIDTH / channel_block_dim;
                for (; img_index + 2 * src0_offset <= channel_stride;
                     img_index += 2 * src0_offset) {
                    op(vis0(src0, src0 + Op::SIMD_WIDTH), channel_block_vec_v2, dst);
                    src0 += Op::SIMD_WIDTH * 2;
                    dst += Op::SIMD_WIDTH * 2;
                }
                // TODO:all elemwise_multi_type op imp one simd mode
                for (; img_index < channel_stride; img_index++) {
                    for (size_t c_iter = 0; c_iter < channel_block_dim; c_iter++) {
                        op(*src0, *(src1_block_ptr + c_iter), dst);
                        src0++;
                        dst++;
                    }
                }
            }
        }
    }
};

template <typename src_ctype>
struct OpCallerBinaryVecBcast101xX<src_ctype, 4> {
    template <typename Op>
    static void run(
            const src_ctype* src0, const src_ctype* src1, typename Op::dst_ctype* dst,
            const Op& op, size_t batch, size_t nr_channel_blocks,
            size_t channel_stride) {
        ParamElemVisitorV2<src_ctype> vis0;
        ParamElemVisitorBcast101x4V2<src_ctype> vis1;
        OpCallerBinaryVecBcast101xD<src_ctype, 4>::run(
                src0, src1, dst, op, vis0, vis1, batch, nr_channel_blocks,
                channel_stride);
    }
};

template <typename Op>
struct OpCallerBinary<Op, VEC_BCAST101xX> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t nr_channel_blocks,
            size_t channel_stride, size_t channel_block_dim) {
        megdnn_assert(
                channel_block_dim == 4 || channel_block_dim == 8,
                "only imp for nchw44/nchw88");
        Op op(src0_dtype, src1_dtype, dst_dtype);
        if (channel_block_dim == 4) {
            OpCallerBinaryVecBcast101xX<typename Op::src_ctype, 4>::run(
                    src0, src1, dst, op, batch, nr_channel_blocks, channel_stride);
        } else {
            OpCallerBinaryVecBcast101xX<typename Op::src_ctype, 8>::run(
                    src0, src1, dst, op, batch, nr_channel_blocks, channel_stride);
        }
    }
};

template <typename Op>
struct OpCallerBinary<Op, VEC_SCALAR> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t nr_elems) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis0;
        ParamElemVisitorDupV2<typename Op::src_ctype> vis1;
        auto vis1_simd_v2 = vis1(&src1);
        size_t i = 0;
        for (; i + Op::SIMD_WIDTH * 2 <= nr_elems; i += Op::SIMD_WIDTH * 2) {
            op(vis0(src0, src0 + Op::SIMD_WIDTH), vis1_simd_v2, dst);
            src0 += Op::SIMD_WIDTH * 2;
            dst += Op::SIMD_WIDTH * 2;
        }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
        for (; i < nr_elems; i++) {
            op(*src0, src1, dst);
            src0++;
            dst++;
        }
    }
};

//! this only for nonswap op, like SUB and DIV
template <typename Op>
struct OpCallerBinary<Op, SCALAR_VEC> {
    static void run(
            const typename Op::src_ctype src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t nr_elems) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        ParamElemVisitorDupV2<typename Op::src_ctype> vis0;
        ParamElemVisitorV2<typename Op::src_ctype> vis1;
        auto vis0_simd_v2 = vis0(&src0);
        size_t i = 0;
        for (; i + Op::SIMD_WIDTH * 2 <= nr_elems; i += Op::SIMD_WIDTH * 2) {
            op(vis0_simd_v2, vis1(src1, src1 + Op::SIMD_WIDTH), dst);
            src1 += Op::SIMD_WIDTH * 2;
            dst += Op::SIMD_WIDTH * 2;
        }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
        for (; i < nr_elems; i++) {
            op(src0, *src1, dst);
            src1++;
            dst++;
        }
    }
};

template <typename Op>
struct OpCallerBinary<Op, BCAST101_VEC> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t channel, size_t channel_stride) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        ParamElemVisitorDupV2<typename Op::src_ctype> vis0;
        ParamElemVisitorV2<typename Op::src_ctype> vis1;
        for (size_t b = 0; b < batch; b++) {
            auto src0_ptr = src0;
            for (size_t c = 0; c < channel; c++) {
                auto vis0_simd_v2 = vis0(src0_ptr);
                size_t i = 0;
                for (; i + Op::SIMD_WIDTH * 2 <= channel_stride;
                     i += Op::SIMD_WIDTH * 2) {
                    op(vis0_simd_v2, vis1(src1, src1 + Op::SIMD_WIDTH), dst);
                    src1 += Op::SIMD_WIDTH * 2;
                    dst += Op::SIMD_WIDTH * 2;
                }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                for (; i < channel_stride; i++) {
                    op(*src0_ptr, *src1, dst);
                    src1++;
                    dst++;
                }
                src0_ptr++;
            }
        }
    }
};

template <typename Op>
struct OpCallerBinary<Op, BCASTX0X_VEC> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType dst_dtype, size_t batch, size_t channel, size_t channel_stride) {
        Op op(src0_dtype, src1_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis;
        for (size_t b = 0; b < batch; b++) {
            auto src0_ptr_base = src0 + b * channel_stride;
            for (size_t c = 0; c < channel; c++) {
                auto src0_ptr = src0_ptr_base;
                size_t i = 0;
                for (; i + Op::SIMD_WIDTH * 2 <= channel_stride;
                     i += Op::SIMD_WIDTH * 2) {
                    auto src0_simd01 = vis(src0_ptr, src0_ptr + Op::SIMD_WIDTH);
                    auto src1_simd01 = vis(src1, src1 + Op::SIMD_WIDTH);
                    op(src0_simd01, src1_simd01, dst);
                    src0_ptr += Op::SIMD_WIDTH * 2;
                    src1 += Op::SIMD_WIDTH * 2;
                    dst += Op::SIMD_WIDTH * 2;
                }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                for (; i < channel_stride; i++) {
                    op(*src0_ptr, *src1, dst);
                    src0_ptr++;
                    src1++;
                    dst++;
                }
            }
        }
    }
};

template <typename Op, BcastType bcast_type>
struct OpCallerTernary;

template <typename Op>
struct OpCallerTernary<Op, VEC_VEC_VEC> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            const typename Op::src_ctype* src2, typename Op::dst_ctype* dst,
            DType src0_dtype, DType src1_dtype, DType src2_dtype, DType dst_dtype,
            size_t nr_elems) {
        Op op(src0_dtype, src1_dtype, src2_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis0;
        ParamElemVisitorV2<typename Op::src_ctype> vis1;
        ParamElemVisitorV2<typename Op::src_ctype> vis2;
        size_t i = 0;
        for (; i + Op::SIMD_WIDTH * 2 <= nr_elems; i += Op::SIMD_WIDTH * 2) {
            op(vis0(src0, src0 + Op::SIMD_WIDTH), vis1(src1, src1 + Op::SIMD_WIDTH),
               vis2(src2, src2 + Op::SIMD_WIDTH), dst);
            src0 += Op::SIMD_WIDTH * 2;
            src1 += Op::SIMD_WIDTH * 2;
            src2 += Op::SIMD_WIDTH * 2;
            dst += Op::SIMD_WIDTH * 2;
        }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
        for (; i < nr_elems; i++) {
            op(*src0, *src1, *src2, dst);
            src0++;
            src1++;
            src2++;
            dst++;
        }
    }
};

//! src0: vector, src1: vector, src2: scalar
template <typename Op>
struct OpCallerTernary<Op, VEC_VEC_SCALAR> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            const typename Op::src_ctype src2, typename Op::dst_ctype* dst,
            DType src0_dtype, DType src1_dtype, DType src2_dtype, DType dst_dtype,
            size_t nr_elems) {
        Op op(src0_dtype, src1_dtype, src2_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis0;
        ParamElemVisitorV2<typename Op::src_ctype> vis1;
        ParamElemVisitorDupV2<typename Op::src_ctype> vis2;
        auto vis2_simd_v2 = vis2(&src2);
        size_t i = 0;
        for (; i + Op::SIMD_WIDTH * 2 <= nr_elems; i += Op::SIMD_WIDTH * 2) {
            op(vis0(src0, src0 + Op::SIMD_WIDTH), vis1(src1, src1 + Op::SIMD_WIDTH),
               vis2_simd_v2, dst);
            src0 += Op::SIMD_WIDTH * 2;
            src1 += Op::SIMD_WIDTH * 2;
            dst += Op::SIMD_WIDTH * 2;
        }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
        for (; i < nr_elems; i++) {
            op(*src0, *src1, src2, dst);
            src0++;
            src1++;
            dst++;
        }
    }
};

//! src0: 1C11, src1: vector, src2:  1C11
template <typename Op>
struct OpCallerTernary<Op, BCAST101_VEC_BCAST101> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            const typename Op::src_ctype* src2, typename Op::dst_ctype* dst,
            DType src0_dtype, DType src1_dtype, DType src2_dtype, DType dst_dtype,
            size_t batch_size, size_t channel_size, size_t channel_stride,
            size_t batch_offset) {
        Op op(src0_dtype, src1_dtype, src2_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis1;
        ParamElemVisitorDupV2<typename Op::src_ctype> vis0;
        ParamElemVisitorDupV2<typename Op::src_ctype> vis2;
        for (size_t batch = 0; batch < batch_size; batch++) {
            auto src0_ptr = src0;
            auto src2_ptr = src2;
            auto b_offset = batch_offset;
            for (size_t channel = 0; channel < channel_size; channel++) {
                size_t i = 0;
                auto src0_simd_v2 = vis0(src0_ptr);
                auto src2_simd_v2 = vis2(src2_ptr);
                for (; i + Op::SIMD_WIDTH * 2 <= channel_stride;
                     i += Op::SIMD_WIDTH * 2) {
                    op(src0_simd_v2, vis1(src1, src1 + Op::SIMD_WIDTH), src2_simd_v2,
                       dst);
                    src1 += Op::SIMD_WIDTH * 2;
                    dst += Op::SIMD_WIDTH * 2;
                    b_offset -= Op::SIMD_WIDTH * 2;
                }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                for (; i < channel_stride; i++) {
                    op(*src0_ptr, *src1, *src2_ptr, dst);
                    src1++;
                    dst++;
                    b_offset--;
                }
                src0_ptr++;
                src2_ptr++;
            }
            src1 += b_offset;
            dst += b_offset;
        }
    }
};

//! src0: 111C, src1: vector, src2:  111C, src1 may not be contig
template <typename Op>
struct OpCallerTernary<Op, BCAST111C_VEC_BCAST111C> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            size_t src1_offset, const typename Op::src_ctype* src2,
            typename Op::dst_ctype* dst, DType src0_dtype, DType src1_dtype,
            DType src2_dtype, DType dst_dtype, size_t batch_size, size_t channel_size,
            size_t channel_stride, size_t batch_offset) {
        Op op(src0_dtype, src1_dtype, src2_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis;
        for (size_t batch = 0; batch < batch_size; batch++) {
            auto b_offset = batch_offset;
            for (size_t channel = 0; channel < channel_size; channel++) {
                auto src0_ptr = src0;
                auto src2_ptr = src2;
                size_t i = 0;
                for (; i + Op::SIMD_WIDTH * 2 <= channel_stride;
                     i += Op::SIMD_WIDTH * 2) {
                    auto src0_simd01 = vis(src0_ptr, src0_ptr + Op::SIMD_WIDTH);
                    auto src1_simd01 = vis(src1, src1 + Op::SIMD_WIDTH);
                    auto src2_simd01 = vis(src2_ptr, src2_ptr + Op::SIMD_WIDTH);
                    op(src0_simd01, src1_simd01, src2_simd01, dst);
                    src0_ptr += Op::SIMD_WIDTH * 2;
                    src1 += Op::SIMD_WIDTH * 2;
                    src2_ptr += Op::SIMD_WIDTH * 2;
                    dst += Op::SIMD_WIDTH * 2;
                    b_offset -= Op::SIMD_WIDTH * 2;
                }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                for (; i < channel_stride; i++) {
                    op(*src0_ptr, *src1, *src2_ptr, dst);
                    src0_ptr++;
                    src1++;
                    src2_ptr++;
                    dst++;
                    b_offset--;
                }
                src1 += src1_offset;
            }
            src1 += b_offset;
            dst += b_offset;
        }
    }
};

template <typename src_ctype, size_t channel_block_dim>
struct OpCallerTernaryBcast101xXVecBcast101xX {
    template <typename Op>
    static void run(
            const src_ctype* src0, const src_ctype* src1, const src_ctype* src2,
            typename Op::dst_ctype* dst, const Op& op, size_t batch,
            size_t nr_channel_blocks, size_t channel_stride) {
        for (size_t b = 0; b < batch; b++) {
            auto src0_ptr = src0;
            auto src2_ptr = src2;
            for (size_t cb = 0; cb < nr_channel_blocks; cb++) {
                auto src0_block_ptr = src0_ptr + cb * channel_block_dim;
                auto src2_block_ptr = src2_ptr + cb * channel_block_dim;
                for (size_t img_index = 0; img_index < channel_stride; img_index++) {
                    for (size_t c_iter = 0; c_iter < channel_block_dim; c_iter++) {
                        op(*(src0_block_ptr + c_iter), *src1,
                           *(src2_block_ptr + c_iter), dst);
                        src1++;
                        dst++;
                    }
                }
            }
        }
    }
};

template <typename src_ctype, size_t channel_block_dim>
struct OpCallerTernaryBcast101xDVecBcast101xD {
    template <typename Op, typename Vis0, typename Vis1, typename Vis2>
    static void run(
            const src_ctype* src0, const src_ctype* src1, const src_ctype* src2,
            typename Op::dst_ctype* dst, const Op& op, const Vis0& vis0,
            const Vis1& vis1, const Vis2& vis2, size_t batch, size_t nr_channel_blocks,
            size_t channel_stride) {
        for (size_t b = 0; b < batch; b++) {
            auto src0_ptr = src0;
            auto src2_ptr = src2;
            for (size_t cb = 0; cb < nr_channel_blocks; cb++) {
                auto src0_block_ptr = src0_ptr + cb * channel_block_dim;
                auto src2_block_ptr = src2_ptr + cb * channel_block_dim;
                auto channel_block_vec0_v2 = vis0(src0_block_ptr);
                auto channel_block_vec2_v2 = vis2(src2_block_ptr);
                size_t img_index = 0;
                auto src1_offset = Op::SIMD_WIDTH / channel_block_dim;
                for (; img_index + 2 * src1_offset <= channel_stride;
                     img_index += 2 * src1_offset) {
                    op(channel_block_vec0_v2, vis1(src1, src1 + Op::SIMD_WIDTH),
                       channel_block_vec2_v2, dst);
                    src1 += Op::SIMD_WIDTH * 2;
                    dst += Op::SIMD_WIDTH * 2;
                }
                // TODO:all elemwise_multi_type op imp one simd mode
                for (; img_index < channel_stride; img_index++) {
                    for (size_t c_iter = 0; c_iter < channel_block_dim; c_iter++) {
                        op(*(src0_block_ptr + c_iter), *src1,
                           *(src2_block_ptr + c_iter), dst);
                        src1++;
                        dst++;
                    }
                }
            }
        }
    }
};

//! src0: CHW44, src1: vector, src2:  CHW44
template <typename src_ctype>
struct OpCallerTernaryBcast101xXVecBcast101xX<src_ctype, 4> {
    template <typename Op>
    static void run(
            const src_ctype* src0, const src_ctype* src1, const src_ctype* src2,
            typename Op::dst_ctype* dst, const Op& op, size_t batch,
            size_t nr_channel_blocks, size_t channel_stride) {
        ParamElemVisitorBcast101x4V2<src_ctype> vis0;
        ParamElemVisitorV2<src_ctype> vis1;
        ParamElemVisitorBcast101x4V2<src_ctype> vis2;
        OpCallerTernaryBcast101xDVecBcast101xD<src_ctype, 4>::run(
                src0, src1, src2, dst, op, vis0, vis1, vis2, batch, nr_channel_blocks,
                channel_stride);
    }
};

template <typename Op>
struct OpCallerTernary<Op, BCAST101xX_VEC_BCAST101xX> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            const typename Op::src_ctype* src2, typename Op::dst_ctype* dst,
            DType src0_dtype, DType src1_dtype, DType src2_dtype, DType dst_dtype,
            size_t batch, size_t nr_channel_blocks, size_t channel_stride,
            size_t channel_block_dim) {
        megdnn_assert(
                channel_block_dim == 4 || channel_block_dim == 8,
                "only imp for nchw44/nchw88");
        Op op(src0_dtype, src1_dtype, src2_dtype, dst_dtype);
        if (channel_block_dim == 4) {
            OpCallerTernaryBcast101xXVecBcast101xX<typename Op::src_ctype, 4>::run(
                    src0, src1, src2, dst, op, batch, nr_channel_blocks,
                    channel_stride);
        } else {
            OpCallerTernaryBcast101xXVecBcast101xX<typename Op::src_ctype, 8>::run(
                    src0, src1, src2, dst, op, batch, nr_channel_blocks,
                    channel_stride);
        }
    }
};

//! src1: 1C11, src0 and src2 are contig
template <typename Op>
struct OpCallerTernary<Op, VEC_BCAST101_VEC> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            const typename Op::src_ctype* src2, typename Op::dst_ctype* dst,
            DType src0_dtype, DType src1_dtype, DType src2_dtype, DType dst_dtype,
            size_t batch_size, size_t channel_size, size_t channel_stride) {
        Op op(src0_dtype, src1_dtype, src2_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis0;
        ParamElemVisitorDupV2<typename Op::src_ctype> vis1;
        ParamElemVisitorV2<typename Op::src_ctype> vis2;
        for (size_t batch = 0; batch < batch_size; batch++) {
            auto src1_ptr = src1;
            for (size_t channel = 0; channel < channel_size; channel++) {
                size_t i = 0;
                auto src1_simd_v2 = vis1(src1_ptr);
                for (; i + Op::SIMD_WIDTH * 2 <= channel_stride;
                     i += Op::SIMD_WIDTH * 2) {
                    op(vis0(src0, src0 + Op::SIMD_WIDTH), src1_simd_v2,
                       vis2(src2, src2 + Op::SIMD_WIDTH), dst);
                    src0 += Op::SIMD_WIDTH * 2;
                    src2 += Op::SIMD_WIDTH * 2;
                    dst += Op::SIMD_WIDTH * 2;
                }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                for (; i < channel_stride; i++) {
                    op(*src0, *src1_ptr, *src2, dst);
                    src0++;
                    src2++;
                    dst++;
                }
                src1_ptr++;
            }
        }
    }
};

//! src1: 111C, src0 and src2 may not be contig
template <typename Op>
struct OpCallerTernary<Op, VEC_BCAST111C_VEC> {
    static void run(
            const typename Op::src_ctype* src0, size_t src0_offset,
            const typename Op::src_ctype* src1, const typename Op::src_ctype* src2,
            size_t src2_offset, typename Op::dst_ctype* dst, DType src0_dtype,
            DType src1_dtype, DType src2_dtype, DType dst_dtype, size_t batch_size,
            size_t channel_size, size_t channel_stride) {
        Op op(src0_dtype, src1_dtype, src2_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis0;
        ParamElemVisitorV2<typename Op::src_ctype> vis1;
        ParamElemVisitorV2<typename Op::src_ctype> vis2;
        for (size_t batch = 0; batch < batch_size; batch++) {
            for (size_t channel = 0; channel < channel_size; channel++) {
                auto src1_ptr = src1;
                size_t i = 0;
                for (; i + Op::SIMD_WIDTH * 2 <= channel_stride;
                     i += Op::SIMD_WIDTH * 2) {
                    op(vis0(src0, src0 + Op::SIMD_WIDTH),
                       vis1(src1_ptr, src1_ptr + Op::SIMD_WIDTH),
                       vis2(src2, src2 + Op::SIMD_WIDTH), dst);
                    src0 += Op::SIMD_WIDTH * 2;
                    src1_ptr += Op::SIMD_WIDTH * 2;
                    src2 += Op::SIMD_WIDTH * 2;
                    dst += Op::SIMD_WIDTH * 2;
                }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
                for (; i < channel_stride; i++) {
                    op(*src0, *src1_ptr, *src2, dst);
                    src0++;
                    src1_ptr++;
                    src2++;
                    dst++;
                }
                src0 += src0_offset;
                src2 += src2_offset;
            }
        }
    }
};

template <typename src_ctype, size_t channel_block_dim>
struct OpCallerTernaryVecBcast101xXVec {
    template <typename Op>
    static void run(
            const src_ctype* src0, const src_ctype* src1, const src_ctype* src2,
            typename Op::dst_ctype* dst, const Op& op, size_t batch,
            size_t nr_channel_blocks, size_t channel_stride) {
        for (size_t b = 0; b < batch; b++) {
            auto src1_ptr = src1;
            for (size_t cb = 0; cb < nr_channel_blocks; cb++) {
                auto src1_block_ptr = src1_ptr + cb * channel_block_dim;
                for (size_t img_index = 0; img_index < channel_stride; img_index++) {
                    for (size_t c_iter = 0; c_iter < channel_block_dim; c_iter++) {
                        op(*src0, *(src1_block_ptr + c_iter), *src2, dst);
                        src0++;
                        src2++;
                        dst++;
                    }
                }
            }
        }
    }
};

//! src1: CHW44, src0 and src2 are contig
template <typename src_ctype, size_t channel_block_dim>
struct OpCallerTernaryVecBcast101xDVec {
    template <typename Op, typename Vis0, typename Vis1, typename Vis2>
    static void run(
            const src_ctype* src0, const src_ctype* src1, const src_ctype* src2,
            typename Op::dst_ctype* dst, const Op& op, const Vis0& vis0,
            const Vis1& vis1, const Vis2& vis2, size_t batch, size_t nr_channel_blocks,
            size_t channel_stride) {
        for (size_t b = 0; b < batch; b++) {
            auto src1_ptr = src1;
            for (size_t cb = 0; cb < nr_channel_blocks; cb++) {
                auto src1_block_ptr = src1_ptr + cb * channel_block_dim;
                auto channel_block_vec_v2 = vis1(src1_block_ptr);
                size_t img_index = 0;
                auto offset = Op::SIMD_WIDTH / channel_block_dim;
                for (; img_index + 2 * offset <= channel_stride;
                     img_index += 2 * offset) {
                    op(vis0(src0, src0 + Op::SIMD_WIDTH), channel_block_vec_v2,
                       vis2(src2, src2 + Op::SIMD_WIDTH), dst);
                    src0 += Op::SIMD_WIDTH * 2;
                    src2 += Op::SIMD_WIDTH * 2;
                    dst += Op::SIMD_WIDTH * 2;
                }
                // TODO:all elemwise_multi_type op imp one simd mode
                for (; img_index < channel_stride; img_index++) {
                    for (size_t c_iter = 0; c_iter < channel_block_dim; c_iter++) {
                        op(*src0, *(src1_block_ptr + c_iter), *src2, dst);
                        src0++;
                        src2++;
                        dst++;
                    }
                }
            }
        }
    }
};

template <typename src_ctype>
struct OpCallerTernaryVecBcast101xXVec<src_ctype, 4> {
    template <typename Op>
    static void run(
            const src_ctype* src0, const src_ctype* src1, const src_ctype* src2,
            typename Op::dst_ctype* dst, const Op& op, size_t batch,
            size_t nr_channel_blocks, size_t channel_stride) {
        ParamElemVisitorV2<src_ctype> vis0;
        ParamElemVisitorBcast101x4V2<src_ctype> vis1;
        ParamElemVisitorV2<src_ctype> vis2;
        OpCallerTernaryVecBcast101xDVec<src_ctype, 4>::run(
                src0, src1, src2, dst, op, vis0, vis1, vis2, batch, nr_channel_blocks,
                channel_stride);
    }
};

template <typename Op>
struct OpCallerTernary<Op, VEC_BCAST101xX_VEC> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype* src1,
            const typename Op::src_ctype* src2, typename Op::dst_ctype* dst,
            DType src0_dtype, DType src1_dtype, DType src2_dtype, DType dst_dtype,
            size_t batch, size_t nr_channel_blocks, size_t channel_stride,
            size_t channel_block_dim) {
        megdnn_assert(
                channel_block_dim == 4 || channel_block_dim == 8,
                "only imp for nchw44/nchw88");

        Op op(src0_dtype, src1_dtype, src2_dtype, dst_dtype);
        if (channel_block_dim == 4) {
            OpCallerTernaryVecBcast101xXVec<typename Op::src_ctype, 4>::run(
                    src0, src1, src2, dst, op, batch, nr_channel_blocks,
                    channel_stride);
        } else {
            OpCallerTernaryVecBcast101xXVec<typename Op::src_ctype, 8>::run(
                    src0, src1, src2, dst, op, batch, nr_channel_blocks,
                    channel_stride);
        }
    }
};

//! src1: scalar, src0 and src2 has the same shape
template <typename Op>
struct OpCallerTernary<Op, VEC_SCALAR_VEC> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype src1,
            const typename Op::src_ctype* src2, typename Op::dst_ctype* dst,
            DType src0_dtype, DType src1_dtype, DType src2_dtype, DType dst_dtype,
            size_t nr_elems) {
        Op op(src0_dtype, src1_dtype, src2_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis0;
        ParamElemVisitorDupV2<typename Op::src_ctype> vis1;
        ParamElemVisitorV2<typename Op::src_ctype> vis2;
        auto vis1_simd_v2 = vis1(&src1);
        size_t i = 0;
        for (; i + Op::SIMD_WIDTH * 2 <= nr_elems; i += Op::SIMD_WIDTH * 2) {
            op(vis0(src0, src0 + Op::SIMD_WIDTH), vis1_simd_v2,
               vis2(src2, src2 + Op::SIMD_WIDTH), dst);
            src0 += Op::SIMD_WIDTH * 2;
            src2 += Op::SIMD_WIDTH * 2;
            dst += Op::SIMD_WIDTH * 2;
        }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
        for (; i < nr_elems; i++) {
            op(*src0, src1, *src2, dst);
            src0++;
            src2++;
            dst++;
        }
    }
};

//! src1, src2: scalar, src0 is vector
template <typename Op>
struct OpCallerTernary<Op, VEC_SCALAR_SCALAR> {
    static void run(
            const typename Op::src_ctype* src0, const typename Op::src_ctype src1,
            const typename Op::src_ctype src2, typename Op::dst_ctype* dst,
            DType src0_dtype, DType src1_dtype, DType src2_dtype, DType dst_dtype,
            size_t nr_elems) {
        Op op(src0_dtype, src1_dtype, src2_dtype, dst_dtype);
        ParamElemVisitorV2<typename Op::src_ctype> vis0;
        ParamElemVisitorDupV2<typename Op::src_ctype> vis1;
        ParamElemVisitorDupV2<typename Op::src_ctype> vis2;
        auto vis1_simd_v2 = vis1(&src1);
        auto vis2_simd_v2 = vis2(&src2);
        size_t i = 0;
        for (; i + Op::SIMD_WIDTH * 2 <= nr_elems; i += Op::SIMD_WIDTH * 2) {
            op(vis0(src0, src0 + Op::SIMD_WIDTH), vis1_simd_v2, vis2_simd_v2, dst);
            src0 += Op::SIMD_WIDTH * 2;
            dst += Op::SIMD_WIDTH * 2;
        }
#if MEGDNN_FIX_AARCH32_BUG
// FIXME: as llvm may cause cannot select error if enable vectorize
#pragma clang loop vectorize(disable)
#endif
        for (; i < nr_elems; i++) {
            op(*src0, src1, src2, dst);
            src0++;
            dst++;
        }
    }
};

}  // namespace elemwise
}  // namespace megdnn

// vim: syntax=cpp.doxygen
