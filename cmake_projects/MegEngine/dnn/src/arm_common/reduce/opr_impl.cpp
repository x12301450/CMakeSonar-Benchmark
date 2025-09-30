#include "src/arm_common/reduce/opr_impl.h"

#include <cstring>
#include "src/arm_common/quantized_converter.h"
#include "src/arm_common/simd_macro/marm_neon.h"
#include "src/common/reduce_helper.h"
#include "src/common/unroll_macro.h"
#include "src/common/utils.h"
#include "src/naive/handle.h"

using namespace megdnn;
using namespace arm_common;

#include "midout.h"
MIDOUT_DECL(megdnn_arm_common_reduce)

namespace {

//! FIXME: we should check this when update the compiler
#if __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
#if MEGDNN_ARMV7
typedef float fp16_fix_t;
#else
typedef __fp16 fp16_fix_t;
#endif
#endif

/*****************************Mean Reducer***********************/
template <typename dtype, typename ctype, typename comp_type, bool C1>
struct MeanReducer;

template <>
struct MeanReducer<dt_quint8, uint8_t, int32_t, true> {
    using ctype = uint8_t;
    static constexpr int SIMD_WIDTH = 16;

    int32_t res;
    int32_t zp;
    int32_t cnt;
    float coef;
    MeanReducer(DType src_dtype, size_t cnt) : res(0), cnt(cnt), coef(1.0 / cnt) {
        zp = src_dtype.param<dtype::Quantized8Asymm>().zero_point;
    }
    MeanReducer() = default;
    void feed(const uint8_t* val) {
#if MEGDNN_AARCH64
        res += vaddlvq_u8(vld1q_u8(val));
#elif MEGDNN_ARMV7
        auto sum = vreinterpretq_s32_u32(vpaddlq_u16(vpaddlq_u8(vld1q_u8(val))));
        res += (vgetq_lane_s32(sum, 0) + vgetq_lane_s32(sum, 1) +
                vgetq_lane_s32(sum, 2) + vgetq_lane_s32(sum, 3));
#else
#error "unsupport android arch"
#endif
    }
    void feed_remain(const uint8_t* val) { res += *val; }
    void post(uint8_t* dst) {
        float sum = (res - zp * cnt) * coef;
        *dst = std::round(sum) + zp;
    }
};

#if __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
template <>
struct MeanReducer<__fp16, __fp16, __fp16, true> {
    using ctype = __fp16;
    static constexpr int SIMD_WIDTH = 8;

    float16x8_t res;
    fp16_fix_t result;
    fp16_fix_t coef;
    MeanReducer(DType, size_t cnt) : result(0.0f), coef(1.0 / cnt) {
        res = vdupq_n_f16(0.0f);
    }
    MeanReducer() = default;
    void feed(const ctype* val) { res = vaddq_f16(vld1q_f16(val), res); }
    void feed_remain(const ctype* val) { result += *val; }
    void post(ctype* dst) {
        auto sum_tmp = vadd_f16(vget_low_f16(res), vget_high_f16(res));
        result +=
                (vget_lane_f16(sum_tmp, 0) + vget_lane_f16(sum_tmp, 1) +
                 vget_lane_f16(sum_tmp, 2) + vget_lane_f16(sum_tmp, 3));
        *dst = result * coef;
    }
};

template <>
struct MeanReducer<__fp16, __fp16, __fp16, false> {
    using ctype = __fp16;
    static constexpr int SIMD_WIDTH = 8;

    float16x8_t res;
    fp16_fix_t remain;
    fp16_fix_t coef;
    MeanReducer(DType, size_t cnt) : remain(0.0f), coef(1.0 / cnt) {
        res = vdupq_n_f16(0.0f);
    }
    MeanReducer() = default;
    void feed(const ctype* val) { res = vaddq_f16(vld1q_f16(val), res); }
    void feed_vector(const float16x8_t& vval) { res = vaddq_f16(vval, res); }
    void feed_remain(const ctype* val) { remain += *val; }
    void post(ctype* dst) {
        res = vmulq_n_f16(res, coef);
        vst1q_f16(dst, res);
    }
    void post_remain(ctype* dst) { *dst = remain * coef; }
};
#endif

template <>
struct MeanReducer<dt_quint8, uint8_t, int32_t, false> {
    using ctype = uint8_t;
    static constexpr int SIMD_WIDTH = 16;

    int32x4_t res[4];
    int32_t remain;
    int32_t zp;
    int32x4_t vzp;
    int32_t cnt;
    int32x4_t vcnt;
    float coef;
    float32x4_t vcoef;
    MeanReducer(DType src_dtype, size_t cnt) : remain(0), cnt(cnt), coef(1.0 / cnt) {
        zp = src_dtype.param<dtype::Quantized8Asymm>().zero_point;
        vzp = vdupq_n_s32(zp);
        memset(res, 0, sizeof(res));
        vcnt = vdupq_n_s32(cnt);
        vcoef = vdupq_n_f32(coef);
    }
    MeanReducer() = default;
    void feed(const uint8_t* val) { feed_vector(vld1q_u8(val)); }
    void feed_vector(const uint8x16_t& vval) {
        const uint16x8_t vval_low = vmovl_u8(vget_low_u8(vval));
        const uint16x8_t vval_high = vmovl_u8(vget_high_u8(vval));

        const uint32x4_t vval_low_low = vmovl_u16(vget_low_u16(vval_low));
        const uint32x4_t vval_low_high = vmovl_u16(vget_high_u16(vval_low));
        const uint32x4_t vval_high_low = vmovl_u16(vget_low_u16(vval_high));
        const uint32x4_t vval_high_high = vmovl_u16(vget_high_u16(vval_high));

        res[0] = vaddq_s32(res[0], vreinterpretq_s32_u32(vval_low_low));
        res[1] = vaddq_s32(res[1], vreinterpretq_s32_u32(vval_low_high));
        res[2] = vaddq_s32(res[2], vreinterpretq_s32_u32(vval_high_low));
        res[3] = vaddq_s32(res[3], vreinterpretq_s32_u32(vval_high_high));
    }
    void feed_remain(const uint8_t* val) { remain += *val; }
    void post(uint8_t* dst) {
        for (int i = 0; i < 4; i += 2) {
            int32x4_t tmp = vmulq_s32(vzp, vcnt);
            int32x4_t tmp0 = vsubq_s32(res[i], tmp);
            int32x4_t tmp1 = vsubq_s32(res[i + 1], tmp);
            float32x4_t vitem0 = vmulq_f32(vcvtq_f32_s32(tmp0), vcoef);
            float32x4_t vitem1 = vmulq_f32(vcvtq_f32_s32(tmp1), vcoef);

            vst1_u8(dst, (QConverter::convert<uint8x8_t, float32x4x2_t>(
                                 {{vitem0, vitem1}}, vzp)));
            dst += 8;
        }
    }
    void post_remain(uint8_t* dst) {
        float sum = (remain - zp * cnt) * coef;
        *dst = std::round(sum) + zp;
    }
};

/******************************max min Reducer****************************/
template <typename dtype, typename ctype, typename comp_type, bool C1>
struct maxReducer;
template <typename dtype, typename ctype, typename comp_type, bool C1>
struct minReducer;

#define REDUCER_MAX_MIN_C1(_mode, _dtype, _ctype, _comp_type, _stype, __stype, _init)  \
    template <>                                                                        \
    struct _mode##Reducer<_dtype, _ctype, _comp_type, true> {                          \
        using ctype = _ctype;                                                          \
        static constexpr int SIMD_WIDTH = 16;                                          \
        __stype##8x16_t res;                                                           \
        _mode##Reducer(DType, size_t) { res = vdupq_n_##_stype##8(_init); }            \
        _mode##Reducer() = default;                                                    \
        void feed(const ctype* val) {                                                  \
            __stype##8x16_t vval = vld1q_##_stype##8(val);                             \
            res = v##_mode##q_##_stype##8(vval, res);                                  \
        }                                                                              \
        void feed_remain(const ctype* val) {                                           \
            __stype##8x16_t vval = vdupq_n_##_stype##8(*val);                          \
            res = v##_mode##q_##_stype##8(vval, res);                                  \
        }                                                                              \
        void post(ctype* dst) {                                                        \
            __stype##16x8_t vval_low = vmovl_##_stype##8(vget_low_##_stype##8(res));   \
            __stype##16x8_t vval_high = vmovl_##_stype##8(vget_high_##_stype##8(res)); \
            __stype##16x8_t vval_m = v##_mode##q_##_stype##16(vval_low, vval_high);    \
                                                                                       \
            __stype##32x4_t vval_m_low =                                               \
                    vmovl_##_stype##16(vget_low_##_stype##16(vval_m));                 \
            __stype##32x4_t vval_m_high =                                              \
                    vmovl_##_stype##16(vget_high_##_stype##16(vval_m));                \
            __stype##32x4_t vval_m_m =                                                 \
                    v##_mode##q_##_stype##32(vval_m_low, vval_m_high);                 \
            using namespace std;                                                       \
            *dst =                                                                     \
                    _mode({vgetq_lane_##_stype##32(vval_m_m, 0),                       \
                           vgetq_lane_##_stype##32(vval_m_m, 1),                       \
                           vgetq_lane_##_stype##32(vval_m_m, 2),                       \
                           vgetq_lane_##_stype##32(vval_m_m, 3)});                     \
        }                                                                              \
    }

REDUCER_MAX_MIN_C1(max, dt_quint8, uint8_t, uint8_t, u, uint, 0);
REDUCER_MAX_MIN_C1(min, dt_quint8, uint8_t, uint8_t, u, uint, 255);
#undef REDUCER_MAX_MIN_C1

#define REDUCER_MAX_MIN_C(_mode, _dtype, _ctype, _comp_type, _stype, __stype, _init) \
    template <>                                                                      \
    struct _mode##Reducer<_dtype, _ctype, _comp_type, false> {                       \
        using ctype = _ctype;                                                        \
        static constexpr int SIMD_WIDTH = 16;                                        \
        __stype##8x16_t res, remain;                                                 \
        _mode##Reducer(DType, size_t) {                                              \
            res = vdupq_n_##_stype(_init);                                           \
            remain = vdupq_n_##_stype(_init);                                        \
        }                                                                            \
        _mode##Reducer() = default;                                                  \
        void feed(const ctype* val) { feed_vector(vld1q_##_stype(val)); }            \
        void inline feed_vector(const __stype##8x16_t & vval) {                      \
            res = v##_mode##q_##_stype(vval, res);                                   \
        }                                                                            \
        void feed_remain(const ctype* val) {                                         \
            __stype##8x16_t vval = vdupq_n_##_stype(*val);                           \
            remain = v##_mode##q_##_stype(vval, remain);                             \
        }                                                                            \
        void post(ctype* dst) { vst1q_##_stype(dst, res); }                          \
        void post_remain(ctype* dst) { vst1q_lane_##_stype(dst, remain, 0); }        \
    }

REDUCER_MAX_MIN_C(max, dt_quint8, uint8_t, uint8_t, u8, uint, 0);
REDUCER_MAX_MIN_C(min, dt_quint8, uint8_t, uint8_t, u8, uint, 255);
#undef REDUCER_MAX_MIN_C

#if __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
#define REDUCER_MAX_MIN_C1(                                                          \
        _mode, _dtype, _ctype, _comp_type, _stype, __stype, _num, _init)             \
    template <>                                                                      \
    struct _mode##Reducer<_dtype, _ctype, _comp_type, true> {                        \
        using ctype = _ctype;                                                        \
        static constexpr int SIMD_WIDTH = _num;                                      \
        __stype res;                                                                 \
        _mode##Reducer(DType, size_t) { res = vdupq_n_##_stype(_init); }             \
        _mode##Reducer() = default;                                                  \
        void feed(const ctype* val) {                                                \
            __stype vval = vld1q_##_stype(val);                                      \
            res = v##_mode##q_##_stype(vval, res);                                   \
        }                                                                            \
        void feed_remain(const ctype* val) {                                         \
            __stype vval = vdupq_n_##_stype(*val);                                   \
            res = v##_mode##q_##_stype(vval, res);                                   \
        }                                                                            \
        void post(ctype* dst) {                                                      \
            auto val = v##_mode##_##_stype(                                          \
                    vget_low_##_stype(res), vget_high_##_stype(res));                \
            using namespace std;                                                     \
            *dst =                                                                   \
                    _mode({vget_lane_##_stype(val, 0), vget_lane_##_stype(val, 1),   \
                           vget_lane_##_stype(val, 2), vget_lane_##_stype(val, 3)}); \
        }                                                                            \
    }

REDUCER_MAX_MIN_C1(
        max, __fp16, __fp16, __fp16, f16, float16x8_t, 8,
        std::numeric_limits<dt_float16>::lowest());
REDUCER_MAX_MIN_C1(
        min, __fp16, __fp16, __fp16, f16, float16x8_t, 8,
        std::numeric_limits<dt_float16>::max());
#undef REDUCER_MAX_MIN_C1
#endif

#if __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
#define REDUCER_MAX_MIN_C(                                                   \
        _mode, _dtype, _ctype, _comp_type, _stype, __stype, _num, _init)     \
    template <>                                                              \
    struct _mode##Reducer<_dtype, _ctype, _comp_type, false> {               \
        using ctype = _ctype;                                                \
        static constexpr int SIMD_WIDTH = _num;                              \
        __stype res;                                                         \
        fp16_fix_t remain;                                                   \
        _mode##Reducer(DType, size_t) {                                      \
            res = vdupq_n_##_stype(_init);                                   \
            remain = _init;                                                  \
        }                                                                    \
        _mode##Reducer() = default;                                          \
        void feed(const ctype* val) { feed_vector(vld1q_##_stype(val)); }    \
        void inline feed_vector(const __stype& vval) {                       \
            res = v##_mode##q_##_stype(vval, res);                           \
        }                                                                    \
        void feed_remain(const ctype* val) {                                 \
            using namespace std;                                             \
            remain = _mode(*val, static_cast<__fp16>(remain));               \
        }                                                                    \
        void post(ctype* dst) { vst1q_##_stype(dst, res); }                  \
        void post_remain(ctype* dst) { *dst = static_cast<__fp16>(remain); } \
    }

REDUCER_MAX_MIN_C(
        max, __fp16, __fp16, __fp16, f16, float16x8_t, 8,
        std::numeric_limits<dt_float16>::lowest());
REDUCER_MAX_MIN_C(
        min, __fp16, __fp16, __fp16, f16, float16x8_t, 8,
        std::numeric_limits<dt_float16>::max());
#undef REDUCER_MAX_MIN_C
#endif

/***************************Sum Product Reducer***************************/
template <typename dtype, typename ctype, typename comp_type, bool C1>
struct SumReducer;
template <typename dtype, typename ctype, typename comp_type, bool C1>
struct ProductReducer;

#define REDUCER_SUM_PRODUCT_C(                                                      \
        _mode, _dtype, _ctype, _comp_type, _stype, __stype, _num, _init, _act, _op) \
    template <>                                                                     \
    struct _mode##Reducer<_dtype, _ctype, _comp_type, false> {                      \
        using ctype = _ctype;                                                       \
        static constexpr int SIMD_WIDTH = _num;                                     \
        __stype res;                                                                \
        ctype remain;                                                               \
        _mode##Reducer(DType, size_t) {                                             \
            res = vdupq_n_##_stype(_init);                                          \
            remain = _init;                                                         \
        }                                                                           \
        _mode##Reducer() = default;                                                 \
        void feed(const ctype* val) { feed_vector(vld1q_##_stype(val)); }           \
        void feed_vector(const __stype& vval) {                                     \
            res = v##_act##q_##_stype(vval, res);                                   \
        }                                                                           \
        void feed_remain(const ctype* val) {                                        \
            using namespace std;                                                    \
            auto op = _op<ctype>();                                                 \
            remain = op(remain, (*val));                                            \
        }                                                                           \
        void post(ctype* dst) { vst1q_##_stype(dst, res); }                         \
        void post_remain(ctype* dst) { *dst = remain; }                             \
    }

#if __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
REDUCER_SUM_PRODUCT_C(Sum, __fp16, __fp16, __fp16, f16, float16x8_t, 8, 0, add, plus);
REDUCER_SUM_PRODUCT_C(
        Product, __fp16, __fp16, __fp16, f16, float16x8_t, 8, 1, mul, multiplies);
#endif
#undef REDUCER_SUM_PRODUCT_C

#if __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
#define REDUCER_SUM_PRODUCT_C1(                                                      \
        _mode, _dtype, _ctype, _comp_type, _stype, __stype, _num, _init, _act, _op)  \
    template <>                                                                      \
    struct _mode##Reducer<_dtype, _ctype, _comp_type, true> {                        \
        using ctype = _ctype;                                                        \
        static constexpr int SIMD_WIDTH = _num;                                      \
        __stype res;                                                                 \
        fp16_fix_t remain;                                                           \
        _mode##Reducer(DType, size_t) {                                              \
            res = vdupq_n_##_stype(_init);                                           \
            remain = _init;                                                          \
        }                                                                            \
        _mode##Reducer() = default;                                                  \
        void feed(const ctype* val) {                                                \
            __stype vval = vld1q_##_stype(val);                                      \
            res = v##_act##q_##_stype(vval, res);                                    \
        }                                                                            \
        void feed_remain(const ctype* val) {                                         \
            using namespace std;                                                     \
            auto op = _op<ctype>();                                                  \
            remain = op(remain, *val);                                               \
        }                                                                            \
        void post(ctype* dst) {                                                      \
            using namespace std;                                                     \
            auto val = v##_act##_##_stype(                                           \
                    vget_low_##_stype(res), vget_high_##_stype(res));                \
            auto op = _op<ctype>();                                                  \
            *dst = op(                                                               \
                    remain,                                                          \
                    op(op(vget_lane_##_stype(val, 0), vget_lane_##_stype(val, 1)),   \
                       op(vget_lane_##_stype(val, 2), vget_lane_##_stype(val, 3)))); \
        }                                                                            \
    }

REDUCER_SUM_PRODUCT_C1(Sum, __fp16, __fp16, __fp16, f16, float16x8_t, 8, 0, add, plus);
REDUCER_SUM_PRODUCT_C1(
        Product, __fp16, __fp16, __fp16, f16, float16x8_t, 8, 1, mul, multiplies);
#undef REDUCER_SUM_PRODUCT_C1
#endif

/***************************SumSqr Reducer***************************/
template <typename dtype, typename ctype, typename comp_type, bool C1>
struct SumSqrReducer;

#if __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
template <>
struct SumSqrReducer<__fp16, __fp16, __fp16, true> {
    using ctype = __fp16;
    static constexpr int SIMD_WIDTH = 8;

    float16x8_t res;
    //! We set the dtype of result to float instead of __fp16, As in compile
    //! armv7+fp16, it may trigger result error.
    //! ldr instrucation need alignment of 4bytes, while __fp16 result placed in
    //! text segments is not satisfied.
    //! FIXME: we should check it if we upgrade compiler.
    fp16_fix_t result;
    SumSqrReducer(DType, size_t cnt) : result(0.0f) { res = vdupq_n_f16(0.0f); }
    SumSqrReducer() = default;
    void feed(const __fp16* val) {
        float16x8_t vval = vld1q_f16(val);
        res = vaddq_f16(vmulq_f16(vval, vval), res);
    }
    void feed_remain(const __fp16* val) {
        __fp16 vval = *val;
        result += vval * vval;
    }
    void post(__fp16* dst) {
        auto sum_temp = vpadd_f16(vget_low_f16(res), vget_high_f16(res));
        result += (vget_lane_f16(sum_temp, 0) + vget_lane_f16(sum_temp, 1)) +
                  (vget_lane_f16(sum_temp, 2) + vget_lane_f16(sum_temp, 3));
        *dst = result;
    }
};
template <>
struct SumSqrReducer<__fp16, __fp16, __fp16, false> {
    using ctype = __fp16;
    static constexpr int SIMD_WIDTH = 8;

    float16x8_t res;
    //! We set the dtype of result to float instead of __fp16, As in compile
    //! armv7+fp16, it may trigger result error.
    //! ldr instrucation need alignment of 4bytes, while __fp16 result placed in
    //! text segments is not satisfied.
    //! FIXME: we should check it if we upgrade compiler.
    fp16_fix_t remain;
    SumSqrReducer(DType, size_t cnt) : remain(0.0f) { res = vdupq_n_f16(0.0f); }
    SumSqrReducer() = default;
    void feed(const __fp16* val) { feed_vector(vld1q_f16(val)); }
    void inline feed_vector(const float16x8_t& vval) {
        res = vaddq_f16(vmulq_f16(vval, vval), res);
    }
    void feed_remain(const __fp16* val) { remain += (*val) * (*val); }
    void post(__fp16* dst) { vst1q_f16(dst, res); }
    void post_remain(__fp16* dst) { *dst = remain; }
};
#endif

/**************************************do reduce*************************/

template <typename Reducer, bool C1>
struct Exec {
    static void do_reduce(
            const typename Reducer::ctype* src, const typename Reducer::ctype* dst,
            DType src_dtype, size_t A, size_t B, size_t C);
};

template <typename Reducer>
struct Exec<Reducer, true> {
    static void do_reduce(
            const typename Reducer::ctype* src, typename Reducer::ctype* dst,
            DType src_dtype, size_t A, size_t B, size_t) {
        size_t a = 0;
        for (; a < A; a++) {
            Reducer reducer0(src_dtype, B);
            auto temp_src0 = src + a * B;
            size_t b = 0;
            for (; b + Reducer::SIMD_WIDTH <= B; b += Reducer::SIMD_WIDTH) {
                reducer0.feed(temp_src0);
                temp_src0 += Reducer::SIMD_WIDTH;
            }
            for (; b < B; b++) {
                reducer0.feed_remain(temp_src0);
                temp_src0++;
            }
            reducer0.post(dst);
            dst++;
        }
    }
};

template <typename Reducer>
struct Exec<Reducer, false> {
    static void do_reduce(
            const typename Reducer::ctype* src, typename Reducer::ctype* dst,
            DType src_dtype, size_t A, size_t B, size_t C) {
        for (size_t a = 0; a < A; a++) {
            size_t c = 0;
            for (; c + Reducer::SIMD_WIDTH <= C; c += Reducer::SIMD_WIDTH) {
                Reducer reducer(src_dtype, B);
                for (size_t b = 0; b < B; b++)
                    reducer.feed(src + c + C * b);
                reducer.post(dst);
                dst += Reducer::SIMD_WIDTH;
            }
            for (; c < C; c++) {
                Reducer reducer(src_dtype, B);
                for (size_t b = 0; b < B; b++)
                    reducer.feed_remain(src + c + C * b);
                reducer.post_remain(dst);
                dst++;
            }
            src += B * C;
        }
    }
};

template <typename Reducer, typename dtype, size_t B>
struct ExecC1SmallB {
    static void do_reduce(
            const dtype* src, dtype* dst, DType src_dtype, size_t A, size_t, size_t C);
};

#define ImplementC1SmallB(_ctype, _simd_prefix, _simd_suffix)                      \
    template <typename Reducer, size_t B>                                          \
    struct ExecC1SmallB<Reducer, _ctype, B> {                                      \
        static void do_reduce(                                                     \
                const _ctype* src, _ctype* dst, DType src_dtype, size_t A, size_t, \
                size_t) {                                                          \
            size_t a = 0;                                                          \
            for (; a + Reducer::SIMD_WIDTH < A; a += Reducer::SIMD_WIDTH) {        \
                Reducer reducer(src_dtype, B);                                     \
                auto src_ptr = src + a * B;                                        \
                if (B == 4) {                                                      \
                    _simd_prefix##x4_t data_v4 = vld4q_##_simd_suffix(src_ptr);    \
                    reducer.feed_vector(data_v4.val[0]);                           \
                    reducer.feed_vector(data_v4.val[1]);                           \
                    reducer.feed_vector(data_v4.val[2]);                           \
                    reducer.feed_vector(data_v4.val[3]);                           \
                }                                                                  \
                if (B == 3) {                                                      \
                    _simd_prefix##x3_t data_v3 = vld3q_##_simd_suffix(src_ptr);    \
                    reducer.feed_vector(data_v3.val[0]);                           \
                    reducer.feed_vector(data_v3.val[1]);                           \
                    reducer.feed_vector(data_v3.val[2]);                           \
                }                                                                  \
                if (B == 2) {                                                      \
                    _simd_prefix##x2_t data_v2 = vld2q_##_simd_suffix(src_ptr);    \
                    reducer.feed_vector(data_v2.val[0]);                           \
                    reducer.feed_vector(data_v2.val[1]);                           \
                }                                                                  \
                reducer.post(dst);                                                 \
                dst += Reducer::SIMD_WIDTH;                                        \
            }                                                                      \
            for (; a < A; a++) {                                                   \
                Reducer reducer(src_dtype, B);                                     \
                auto src_ptr = src + a * B;                                        \
                for (size_t i = 0; i < B; i++)                                     \
                    reducer.feed_remain(src_ptr + i);                              \
                reducer.post_remain(dst);                                          \
                dst++;                                                             \
            }                                                                      \
        }                                                                          \
    }

ImplementC1SmallB(uint8_t, uint8x16, u8);

#if __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
ImplementC1SmallB(__fp16, float16x8, f16);
#endif

}  // anonymous namespace

void ReduceImpl::exec(
        _megdnn_tensor_in src, _megdnn_tensor_out dst, _megdnn_workspace workspace) {
    check_exec(src.layout, dst.layout, workspace.size);
    size_t A, B, C;
    reduce::get_ABC(src.layout, A, B, C, param().axis);
    bool execed = false;
    using Mode = param::Reduce::Mode;
#define DISPATCH_FUNC(Reducer, _dtype, ctype, comp_type)                            \
    if (C == 1) {                                                                   \
        using _Reducer = Reducer<_dtype, ctype, comp_type, true>;                   \
        using _ReducerC1SmallB = Reducer<_dtype, ctype, comp_type, false>;          \
        std::function<void(const ctype*, ctype*, DType, size_t, size_t, size_t)>    \
                do_reduce = Exec<_Reducer, true>::do_reduce;                        \
        if (src.layout.dtype.category() != DTypeCategory::FLOAT) {                  \
            if (B == 2)                                                             \
                do_reduce = ExecC1SmallB<_ReducerC1SmallB, ctype, 2>::do_reduce;    \
            if (B == 3)                                                             \
                do_reduce = ExecC1SmallB<_ReducerC1SmallB, ctype, 3>::do_reduce;    \
            if (B == 4)                                                             \
                do_reduce = ExecC1SmallB<_ReducerC1SmallB, ctype, 4>::do_reduce;    \
        }                                                                           \
        MIDOUT_BEGIN(                                                               \
                megdnn_arm_common_reduce, ctype, _dtype, comp_type, midout_iv(1)) { \
            MEGDNN_DISPATCH_CPU_KERN_OPR(do_reduce(                                 \
                    reinterpret_cast<ctype*>(src.raw_ptr()),                        \
                    reinterpret_cast<ctype*>(dst.raw_ptr()), src_type, A, B, C));   \
            execed = true;                                                          \
        }                                                                           \
        MIDOUT_END();                                                               \
    } else {                                                                        \
        using _Reducer = Reducer<_dtype, ctype, comp_type, false>;                  \
        std::function<void(const ctype*, ctype*, DType, size_t, size_t, size_t)>    \
                do_reduce = Exec<_Reducer, false>::do_reduce;                       \
        MIDOUT_BEGIN(                                                               \
                megdnn_arm_common_reduce, ctype, _dtype, comp_type, midout_iv(1)) { \
            MEGDNN_DISPATCH_CPU_KERN_OPR(do_reduce(                                 \
                    reinterpret_cast<ctype*>(src.raw_ptr()),                        \
                    reinterpret_cast<ctype*>(dst.raw_ptr()), src_type, A, B, C));   \
            execed = true;                                                          \
        }                                                                           \
        MIDOUT_END();                                                               \
    }

#define DISPATCH_MODE_QUANTIZED(dtype, ctype, comp_type)         \
    switch (param().mode) {                                      \
        case Mode::MEAN:                                         \
            DISPATCH_FUNC(MeanReducer, dtype, ctype, comp_type); \
            break;                                               \
        case Mode::MAX:                                          \
            DISPATCH_FUNC(maxReducer, dtype, ctype, ctype);      \
            break;                                               \
        case Mode::MIN:                                          \
            DISPATCH_FUNC(minReducer, dtype, ctype, ctype);      \
            break;                                               \
        default:                                                 \
            break;                                               \
    }

#define DISPATCH_MODE_FLOAT(dtype, ctype, comp_type)             \
    switch (param().mode) {                                      \
        case Mode::MEAN:                                         \
            DISPATCH_FUNC(MeanReducer, dtype, ctype, comp_type); \
            break;                                               \
        case Mode::MAX:                                          \
            DISPATCH_FUNC(maxReducer, dtype, ctype, ctype);      \
            break;                                               \
        case Mode::MIN:                                          \
            DISPATCH_FUNC(minReducer, dtype, ctype, ctype);      \
            break;                                               \
        case Mode::SUM:                                          \
            DISPATCH_FUNC(SumReducer, dtype, ctype, ctype);      \
            break;                                               \
        case Mode::SUM_SQR:                                      \
            DISPATCH_FUNC(SumSqrReducer, dtype, ctype, ctype);   \
            break;                                               \
        case Mode::PRODUCT:                                      \
            DISPATCH_FUNC(ProductReducer, dtype, ctype, ctype);  \
            break;                                               \
        default:                                                 \
            break;                                               \
    }

    if (src.layout.is_contiguous() &&
        src.layout.dtype.category() == DTypeCategory::QUANTIZED &&
        param().data_type == param::Reduce::DataType::DEFAULT) {
        DType src_type = src.layout.dtype;

        if (src.layout.dtype.enumv() == DTypeEnum::Quantized8Asymm) {
            DISPATCH_MODE_QUANTIZED(dt_quint8, uint8_t, int32_t)
        }
    } else if (
            src.layout.is_contiguous() &&
            src.layout.dtype.category() == DTypeCategory::FLOAT &&
            param().data_type == param::Reduce::DataType::DEFAULT) {
        DType src_type = src.layout.dtype;
        MEGDNN_MARK_USED_VAR(src_type);
#if __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
        if (src.layout.dtype.enumv() == DTypeEnum::Float16) {
            DNN_INC_FLOAT16(DISPATCH_MODE_FLOAT(__fp16, __fp16, __fp16));
        }
#endif
    }
#undef DISPATCH_FUNC
#undef DISPATCH_MODE_QUANTIZED
#undef DISPATCH_MODE_FLOAT

    if (!execed) {
        return fallback::ReduceImpl::exec(src, dst, workspace);
    }
}

// vim: syntax=cpp.doxygen
