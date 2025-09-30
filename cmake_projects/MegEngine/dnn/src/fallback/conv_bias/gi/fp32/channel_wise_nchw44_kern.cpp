#include "src/fallback/conv_bias/gi/fp32/channel_wise_nchw44_kern.h"
#include "src/common/unroll_macro.h"
#include "src/common/utils.h"
#include "src/fallback/conv_bias/common.h"
#include "src/fallback/conv_bias/gi/fp32/channel_wise_3x3_s1p1_nchw44_kern.h"
#include "src/fallback/conv_bias/gi/fp32/channel_wise_5x5_s1p2_nchw44_kern.h"
#include "src/fallback/elemwise_helper/elemwise_op.h"

using namespace megdnn;
using namespace fallback;

namespace {

#define cb(_simd_fixlen_type, _fun_suffix, _simd_type_v2)                         \
    struct ParamElemFixLenVisitorV2None {                                         \
        _simd_type_v2 operator()(                                                 \
                const _simd_fixlen_type& s0, const _simd_fixlen_type& s1) const { \
            _simd_type_v2 ret;                                                    \
            GiSetSubVector##_fun_suffix##V2(                                      \
                    ret, 0, GiFixLenType2Gi##_fun_suffix##Type(s0));              \
            GiSetSubVector##_fun_suffix##V2(                                      \
                    ret, 1, GiFixLenType2Gi##_fun_suffix##Type(s1));              \
            return ret;                                                           \
        }                                                                         \
    };

cb(GI_FLOAT32_FIXLEN_t, Float32, GI_FLOAT32_V2_t);
#undef cb

template <int size>
void load_vec(GI_FLOAT32_FIXLEN_t* dst, const float* src);

#define cb(i) dst[i] = GiFloat32Type2FixLenType(GiLoadFloat32(src + i * 4));
#define LOAD_MACRO(n)                                                      \
    template <>                                                            \
    inline void load_vec<n>(GI_FLOAT32_FIXLEN_t * dst, const float* src) { \
        UNROLL_CALL_NOWRAPPER(n, cb);                                      \
    }
LOAD_MACRO(2);
LOAD_MACRO(3);
LOAD_MACRO(4);
LOAD_MACRO(5);
LOAD_MACRO(6);
LOAD_MACRO(7);
LOAD_MACRO(8);
LOAD_MACRO(9);
#undef cb
#undef LOAD_MACRO

template <int size>
void compute_vec(
        GI_FLOAT32_FIXLEN_t& dst, GI_FLOAT32_FIXLEN_t* src,
        GI_FLOAT32_FIXLEN_t* filter);

#define cb(i)                                                                    \
    dst = GiFloat32Type2FixLenType(GiMlaqFloat32(                                \
            GiFixLenType2GiFloat32Type(dst), GiFixLenType2GiFloat32Type(src[i]), \
            GiFixLenType2GiFloat32Type(filter[i])));
#define COMPUTE_MACRO(n)                                          \
    template <>                                                   \
    inline void compute_vec<n>(                                   \
            GI_FLOAT32_FIXLEN_t & dst, GI_FLOAT32_FIXLEN_t * src, \
            GI_FLOAT32_FIXLEN_t * filter) {                       \
        UNROLL_CALL_NOWRAPPER(n, cb);                             \
    }
COMPUTE_MACRO(2);
COMPUTE_MACRO(3);
COMPUTE_MACRO(5);
#undef cb
#undef COMPUTE_MACRO

template <BiasMode bias_mode, int size>
struct load_bias_vec;

#define cb_bias(i) dst[i] = GiFloat32Type2FixLenType(GiLoadFloat32((bptr) + i * 4));
#define cb_init(i) dst[i] = GiFloat32Type2FixLenType(init);

#define INIT_BIAS_MACRO(n)                                          \
    template <BiasMode bias_mode>                                   \
    struct load_bias_vec<bias_mode, n> {                            \
        static void impl(                                           \
                GI_FLOAT32_FIXLEN_t* dst, const GI_FLOAT32_t& init, \
                const float* bptr) {                                \
            if (bias_mode == BiasMode::BIAS) {                      \
                UNROLL_CALL_NOWRAPPER(n, cb_bias);                  \
            } else {                                                \
                UNROLL_CALL_NOWRAPPER(n, cb_init);                  \
            }                                                       \
        }                                                           \
    };

INIT_BIAS_MACRO(1);
INIT_BIAS_MACRO(2);
INIT_BIAS_MACRO(4);
#undef cb_bias
#undef cb_init
#undef INIT_BIAS_MACRO
}  // namespace

#define COMPUTE_PADDING_KERNEL()                                                       \
    do {                                                                               \
        int iw = ow * stride - PW;                                                     \
        GI_FLOAT32_FIXLEN_t result;                                                    \
        load_bias_vec<bias_mode, 1>::impl(&result, init, bias + oh * OW * 4 + ow * 4); \
        for (int kh = 0; kh < fh; kh++) {                                              \
            if (kh + ih < 0 || kh + ih >= static_cast<int>(IH))                        \
                continue;                                                              \
            for (int kw = 0; kw < fh; kw++) {                                          \
                if (kw + iw < 0 || kw + iw >= static_cast<int>(IW))                    \
                    continue;                                                          \
                const float* sptr = src + (kh + ih) * IW * 4 + (kw + iw) * 4;          \
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(                       \
                        GiFixLenType2GiFloat32Type(result),                            \
                        GiFixLenType2GiFloat32Type(kernel[kh * fh + kw]),              \
                        GiLoadFloat32(sptr)));                                         \
            }                                                                          \
        }                                                                              \
        float* output = dst + oh * OW * 4 + ow * 4;                                    \
        op(GiFixLenType2GiFloat32Type(result), output);                                \
    } while (0)

template <BiasMode bias_mode, typename Op>
struct PaddingCompute {
    static void compute(
            const float* src, const float* bias, float* dst, const int fh,
            const int stride, const size_t IH, const size_t IW, const size_t OH,
            const size_t OW, const size_t PH, const size_t PW,
            const GI_FLOAT32_FIXLEN_t* kernel, const GI_FLOAT32_t& init) {
        size_t oh_start = (PH + stride - 1) / stride;
        size_t ow_start = (PW + stride - 1) / stride;
        size_t oh_end = (IH + PH - fh) / stride + 1;
        size_t ow_end = (IW + PW - fh) / stride + 1;
        Op op;
        for (size_t oh = 0; oh < oh_start; oh++) {
            int ih = oh * stride - PH;
            for (size_t ow = 0; ow < OW; ow++) {
                COMPUTE_PADDING_KERNEL();
            }
        }
        for (size_t oh = oh_start; oh < oh_end; oh++) {
            int ih = oh * stride - PH;
            for (size_t ow = 0; ow < ow_start; ow++) {
                COMPUTE_PADDING_KERNEL();
            }
            for (size_t ow = ow_end; ow < OW; ow++) {
                COMPUTE_PADDING_KERNEL();
            }
        }
        for (size_t oh = oh_end; oh < OH; oh++) {
            int ih = oh * stride - PH;
            for (size_t ow = 0; ow < OW; ow++) {
                COMPUTE_PADDING_KERNEL();
            }
        }
    }
};

template <BiasMode bias_mode, typename Op>
struct PaddingComputeK3P1 {
    static void compute(
            const float* src, const float* bias, float* dst, const size_t stride,
            const size_t IH, const size_t IW, const size_t OH, const size_t OW,
            const GI_FLOAT32_FIXLEN_t* kernel, const GI_FLOAT32_t& init) {
        constexpr size_t PH = 1, PW = 1, FH = 3;
        size_t oh_start = (PH + stride - 1) / stride;
        size_t ow_start = (PW + stride - 1) / stride;
        size_t oh_end = (IH + PH - FH) / stride + 1;
        size_t ow_end = (IW + PW - FH) / stride + 1;
        megdnn_assert(
                oh_start == ow_start && oh_start == 1,
                "channel wise padding param error");
        megdnn_assert(ow_end == OW - 1 || ow_end == OW, "padding PW error");
        megdnn_assert(oh_end == OH - 1 || oh_end == OH, "padding PH error");
        Op op;
        // line one left
        {
            GI_FLOAT32_FIXLEN_t result;
            load_bias_vec<bias_mode, 1>::impl(&result, init, bias);
            result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                    GiFixLenType2GiFloat32Type(result),
                    GiFixLenType2GiFloat32Type(kernel[4]), GiLoadFloat32(src)));
            result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                    GiFixLenType2GiFloat32Type(result),
                    GiFixLenType2GiFloat32Type(kernel[5]), GiLoadFloat32(src + 4)));
            result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                    GiFixLenType2GiFloat32Type(result),
                    GiFixLenType2GiFloat32Type(kernel[7]),
                    GiLoadFloat32(src + IW * 4)));
            result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                    GiFixLenType2GiFloat32Type(result),
                    GiFixLenType2GiFloat32Type(kernel[8]),
                    GiLoadFloat32(src + IW * 4 + 4)));
            float* output = dst;
            op(GiFixLenType2GiFloat32Type(result), output);
        }
        // line one mid
        for (size_t ow = ow_start; ow < ow_end; ow++) {
            int iw = ow * stride - PW;
            GI_FLOAT32_FIXLEN_t result;
            load_bias_vec<bias_mode, 1>::impl(&result, init, bias + ow * 4);
            const float* sptr = src + iw * 4;
            result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                    GiFixLenType2GiFloat32Type(result),
                    GiFixLenType2GiFloat32Type(kernel[3]), GiLoadFloat32(sptr)));
            result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                    GiFixLenType2GiFloat32Type(result),
                    GiFixLenType2GiFloat32Type(kernel[4]), GiLoadFloat32(sptr + 4)));
            result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                    GiFixLenType2GiFloat32Type(result),
                    GiFixLenType2GiFloat32Type(kernel[5]), GiLoadFloat32(sptr + 8)));
            result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                    GiFixLenType2GiFloat32Type(result),
                    GiFixLenType2GiFloat32Type(kernel[6]),
                    GiLoadFloat32(sptr + IW * 4)));
            result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                    GiFixLenType2GiFloat32Type(result),
                    GiFixLenType2GiFloat32Type(kernel[7]),
                    GiLoadFloat32(sptr + IW * 4 + 4)));
            result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                    GiFixLenType2GiFloat32Type(result),
                    GiFixLenType2GiFloat32Type(kernel[8]),
                    GiLoadFloat32(sptr + IW * 4 + 8)));
            float* output = dst + ow * 4;
            op(GiFixLenType2GiFloat32Type(result), output);
        }
        // line one right
        if (OW != ow_end) {
            GI_FLOAT32_FIXLEN_t result;
            load_bias_vec<bias_mode, 1>::impl(&result, init, bias + (OW - 1) * 4);
            const float* sptr = src + (ow_end * stride - PW) * 4;
            result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                    GiFixLenType2GiFloat32Type(result),
                    GiFixLenType2GiFloat32Type(kernel[3]), GiLoadFloat32(sptr)));
            result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                    GiFixLenType2GiFloat32Type(result),
                    GiFixLenType2GiFloat32Type(kernel[4]), GiLoadFloat32(sptr + 4)));
            result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                    GiFixLenType2GiFloat32Type(result),
                    GiFixLenType2GiFloat32Type(kernel[6]),
                    GiLoadFloat32(sptr + IW * 4)));
            result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                    GiFixLenType2GiFloat32Type(result),
                    GiFixLenType2GiFloat32Type(kernel[7]),
                    GiLoadFloat32(sptr + IW * 4 + 4)));
            float* output = dst + ow_end * 4;
            op(GiFixLenType2GiFloat32Type(result), output);
        }
        // mid line
        for (size_t oh = oh_start; oh < oh_end; oh++) {
            int ih = oh * stride - PH;
            // left
            {
                GI_FLOAT32_FIXLEN_t result;
                load_bias_vec<bias_mode, 1>::impl(&result, init, bias + oh * OW * 4);
                const float* sptr = src + ih * IW * 4;
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[1]), GiLoadFloat32(sptr)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[2]),
                        GiLoadFloat32(sptr + 4)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[4]),
                        GiLoadFloat32(sptr + IW * 4)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[5]),
                        GiLoadFloat32(sptr + IW * 4 + 4)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[7]),
                        GiLoadFloat32(sptr + 2 * IW * 4)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[8]),
                        GiLoadFloat32(sptr + 2 * IW * 4 + 4)));
                float* output = dst + oh * OW * 4;
                op(GiFixLenType2GiFloat32Type(result), output);
            }
            // right
            if (OW != ow_end) {
                GI_FLOAT32_FIXLEN_t result;
                load_bias_vec<bias_mode, 1>::impl(
                        &result, init, bias + oh * OW * 4 + (OW - 1) * 4);
                const float* sptr = src + ih * IW * 4 + (ow_end * stride - PW) * 4;
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[0]), GiLoadFloat32(sptr)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[1]),
                        GiLoadFloat32(sptr + 4)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[3]),
                        GiLoadFloat32(sptr + IW * 4)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[4]),
                        GiLoadFloat32(sptr + IW * 4 + 4)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[6]),
                        GiLoadFloat32(sptr + 2 * IW * 4)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[7]),
                        GiLoadFloat32(sptr + 2 * IW * 4 + 4)));
                float* output = dst + oh * OW * 4 + ow_end * 4;
                op(GiFixLenType2GiFloat32Type(result), output);
            }
        }
        // last line left
        if (OH != oh_end) {
            size_t oh = OH - 1;
            {
                GI_FLOAT32_FIXLEN_t result;
                load_bias_vec<bias_mode, 1>::impl(&result, init, bias + oh * OW * 4);
                const float* sptr = src + (oh_end * stride - PH) * IW * 4;
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[1]), GiLoadFloat32(sptr)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[2]),
                        GiLoadFloat32(sptr + 4)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[4]),
                        GiLoadFloat32(sptr + IW * 4)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[5]),
                        GiLoadFloat32(sptr + IW * 4 + 4)));
                float* output = dst + oh_end * OW * 4;
                op(GiFixLenType2GiFloat32Type(result), output);
            }
            // last line mid
            for (size_t ow = ow_start; ow < ow_end; ow++) {
                int iw = ow * stride - PW;
                GI_FLOAT32_FIXLEN_t result;
                load_bias_vec<bias_mode, 1>::impl(
                        &result, init, bias + oh * OW * 4 + ow * 4);
                const float* sptr = src + (oh_end * stride - PH) * IW * 4 + iw * 4;
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[0]), GiLoadFloat32(sptr)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[1]),
                        GiLoadFloat32(sptr + 4)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[2]),
                        GiLoadFloat32(sptr + 8)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[3]),
                        GiLoadFloat32(sptr + IW * 4)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[4]),
                        GiLoadFloat32(sptr + IW * 4 + 4)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[5]),
                        GiLoadFloat32(sptr + IW * 4 + 8)));
                float* output = dst + oh_end * OW * 4 + ow * 4;
                op(GiFixLenType2GiFloat32Type(result), output);
            }
            // last line right
            if (OW != ow_end) {
                GI_FLOAT32_FIXLEN_t result;
                load_bias_vec<bias_mode, 1>::impl(
                        &result, init, bias + oh * OW * 4 + (OW - 1) * 4);
                const float* sptr = src + (oh_end * stride - PH) * IW * 4 +
                                    (ow_end * stride - PW) * 4;
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[0]), GiLoadFloat32(sptr)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[1]),
                        GiLoadFloat32(sptr + 4)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[3]),
                        GiLoadFloat32(sptr + IW * 4)));
                result = GiFloat32Type2FixLenType(GiMlaqFloat32(
                        GiFixLenType2GiFloat32Type(result),
                        GiFixLenType2GiFloat32Type(kernel[4]),
                        GiLoadFloat32(sptr + IW * 4 + 4)));
                float* output = dst + oh_end * OW * 4 + ow_end * 4;
                op(GiFixLenType2GiFloat32Type(result), output);
            }
        }
    }
};

#undef COMPUTE_PADDING_KERNEL

template <BiasMode bias_mode, typename Op>
void channel_wise_nchw44_float::do_conv_kern_stride1_2x2(
        const float* src, const float* filter, const float* bias, float* dst,
        const size_t IH, const size_t IW, const size_t OH, const size_t OW,
        const size_t PH, const size_t PW) {
    GI_FLOAT32_FIXLEN_t kernel[4];
    load_vec<4>(kernel, filter);
    Op op;
    GI_FLOAT32_t init = GiZeroFloat32();
    if (bias_mode == BiasMode::BROADCAST_CHANNEL_BIAS) {
        init = GiLoadFloat32(bias);
    }
    size_t oh_start = PH;
    size_t ow_start = PW;
    size_t oh_end = IH + PH - 1;
    size_t ow_end = IW + PW - 1;
    if (PH || PW) {
        PaddingCompute<bias_mode, Op>::compute(
                src, bias, dst, 2, 1, IH, IW, OH, OW, PH, PW, kernel, init);
    }
#define COMPUTE_2X2(dst, src, kernel)        \
    compute_vec<2>(dst[0], &src[0], kernel); \
    compute_vec<2>(dst[1], &src[1], kernel); \
    compute_vec<2>(dst[2], &src[2], kernel); \
    compute_vec<2>(dst[3], &src[3], kernel)

    size_t oh = oh_start;
    for (; oh + 1 < oh_end; oh += 2) {
        size_t ih = oh - oh_start;
        size_t ow = ow_start;
        for (; ow + 3 < ow_end; ow += 4) {
            size_t iw = ow - ow_start;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v[2][4];
            load_bias_vec<bias_mode, 4>::impl(
                    dst_v[0], init, bias + oh * OW * 4 + ow * 4);
            load_bias_vec<bias_mode, 4>::impl(
                    dst_v[1], init, bias + (oh + 1) * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t src_v[3][5];
            load_vec<5>(src_v[0], input);
            COMPUTE_2X2(dst_v[0], src_v[0], &kernel[0]);
            load_vec<5>(src_v[1], input + IW * 4);
            COMPUTE_2X2(dst_v[0], src_v[1], &kernel[2]);
            COMPUTE_2X2(dst_v[1], src_v[1], &kernel[0]);
            load_vec<5>(src_v[2], input + 2 * IW * 4);
            COMPUTE_2X2(dst_v[1], src_v[2], &kernel[2]);

            ParamElemFixLenVisitorV2None vis;
            op(vis(dst_v[0][0], dst_v[0][1]), output);
            op(vis(dst_v[0][2], dst_v[0][3]), output + 8);
            op(vis(dst_v[1][0], dst_v[1][1]), output + OW * 4);
            op(vis(dst_v[1][2], dst_v[1][3]), output + OW * 4 + 8);
        }
        for (; ow < ow_end; ow++) {
            size_t iw = ow - ow_start;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v[2];
            load_bias_vec<bias_mode, 1>::impl(
                    &dst_v[0], init, bias + oh * OW * 4 + ow * 4);
            load_bias_vec<bias_mode, 1>::impl(
                    &dst_v[1], init, bias + (oh + 1) * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t src_v[3][2];
            load_vec<2>(src_v[0], input);
            compute_vec<2>(dst_v[0], &src_v[0][0], &kernel[0]);
            load_vec<2>(src_v[1], input + IW * 4);
            compute_vec<2>(dst_v[0], &src_v[1][0], &kernel[2]);
            compute_vec<2>(dst_v[1], &src_v[1][0], &kernel[0]);
            load_vec<2>(src_v[2], input + 2 * IW * 4);
            compute_vec<2>(dst_v[1], &src_v[2][0], &kernel[2]);

            op(GiFixLenType2GiFloat32Type(dst_v[0]), output);
            op(GiFixLenType2GiFloat32Type(dst_v[1]), output + OW * 4);
        }
    }
    for (; oh < oh_end; oh++) {
        size_t ih = oh - oh_start;
        size_t ow = ow_start;
        for (; ow + 3 < ow_end; ow += 4) {
            size_t iw = ow - ow_start;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v[1][4];
            load_bias_vec<bias_mode, 4>::impl(
                    dst_v[0], init, bias + oh * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t src_v[2][5];
            load_vec<5>(src_v[0], input);
            COMPUTE_2X2(dst_v[0], src_v[0], &kernel[0]);
            load_vec<5>(src_v[1], input + IW * 4);
            COMPUTE_2X2(dst_v[0], src_v[1], &kernel[2]);

            ParamElemFixLenVisitorV2None vis;
            op(vis(dst_v[0][0], dst_v[0][1]), output);
            op(vis(dst_v[0][2], dst_v[0][3]), output + 8);
        }
        for (; ow < ow_end; ow++) {
            size_t iw = ow - ow_start;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v;
            load_bias_vec<bias_mode, 1>::impl(
                    &dst_v, init, bias + oh * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t src_v[2][2];
            load_vec<2>(src_v[0], input);
            compute_vec<2>(dst_v, &src_v[0][0], &kernel[0]);
            load_vec<2>(src_v[1], input + IW * 4);
            compute_vec<2>(dst_v, &src_v[1][0], &kernel[2]);

            op(GiFixLenType2GiFloat32Type(dst_v), output);
        }
    }
#undef COMPUTE_2X2
}

template <BiasMode bias_mode, typename Op>
void channel_wise_nchw44_float::do_conv_kern_stride1_3x3(
        const float* src, const float* filter, const float* bias, float* dst,
        const size_t IH, const size_t IW, const size_t OH, const size_t OW,
        const size_t PH, const size_t PW) {
    if (IH == OH && IW == OW && IH >= 3 && IW >= 3 && PH == 1 && PW == 1) {
        channel_wise_nchw44_float::do_conv_kern_3x3_stride1_padding1<bias_mode, Op>(
                src, dst, filter, bias, OH, OW);
        return;
    }

    GI_FLOAT32_FIXLEN_t kernel[9];
    load_vec<9>(kernel, filter);
    Op op;
    GI_FLOAT32_t init = GiZeroFloat32();
    if (bias_mode == BiasMode::BROADCAST_CHANNEL_BIAS) {
        init = GiLoadFloat32(bias);
    }
    size_t oh_start = PH;
    size_t ow_start = PW;
    size_t oh_end = IH + PH - 2;
    size_t ow_end = IW + PW - 2;
    if (PH || PW) {
        PaddingCompute<bias_mode, Op>::compute(
                src, bias, dst, 3, 1, IH, IW, OH, OW, PH, PW, kernel, init);
    }
    size_t oh = oh_start;
    for (; oh + 1 < oh_end; oh += 2) {
        size_t ih = oh - PH;
        size_t ow = ow_start;
        for (; ow + 3 < ow_end; ow += 4) {
            size_t iw = ow - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v[2][4];
            load_bias_vec<bias_mode, 4>::impl(
                    dst_v[0], init, bias + oh * OW * 4 + ow * 4);
            load_bias_vec<bias_mode, 4>::impl(
                    dst_v[1], init, bias + (oh + 1) * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t src_v[2][6];
            load_vec<6>(src_v[0], input);
            compute_vec<3>(dst_v[0][0], &src_v[0][0], &kernel[0]);
            compute_vec<3>(dst_v[0][1], &src_v[0][1], &kernel[0]);
            compute_vec<3>(dst_v[0][2], &src_v[0][2], &kernel[0]);
            compute_vec<3>(dst_v[0][3], &src_v[0][3], &kernel[0]);
            load_vec<6>(src_v[1], input + IW * 4);
            compute_vec<3>(dst_v[0][0], &src_v[1][0], &kernel[3]);
            compute_vec<3>(dst_v[0][1], &src_v[1][1], &kernel[3]);
            compute_vec<3>(dst_v[0][2], &src_v[1][2], &kernel[3]);
            compute_vec<3>(dst_v[0][3], &src_v[1][3], &kernel[3]);
            compute_vec<3>(dst_v[1][0], &src_v[1][0], &kernel[0]);
            compute_vec<3>(dst_v[1][1], &src_v[1][1], &kernel[0]);
            compute_vec<3>(dst_v[1][2], &src_v[1][2], &kernel[0]);
            compute_vec<3>(dst_v[1][3], &src_v[1][3], &kernel[0]);
            load_vec<6>(src_v[0], input + 2 * IW * 4);
            compute_vec<3>(dst_v[0][0], &src_v[0][0], &kernel[6]);
            compute_vec<3>(dst_v[0][1], &src_v[0][1], &kernel[6]);
            compute_vec<3>(dst_v[0][2], &src_v[0][2], &kernel[6]);
            compute_vec<3>(dst_v[0][3], &src_v[0][3], &kernel[6]);
            compute_vec<3>(dst_v[1][0], &src_v[0][0], &kernel[3]);
            compute_vec<3>(dst_v[1][1], &src_v[0][1], &kernel[3]);
            compute_vec<3>(dst_v[1][2], &src_v[0][2], &kernel[3]);
            compute_vec<3>(dst_v[1][3], &src_v[0][3], &kernel[3]);
            load_vec<6>(src_v[1], input + 3 * IW * 4);
            compute_vec<3>(dst_v[1][0], &src_v[1][0], &kernel[6]);
            compute_vec<3>(dst_v[1][1], &src_v[1][1], &kernel[6]);
            compute_vec<3>(dst_v[1][2], &src_v[1][2], &kernel[6]);
            compute_vec<3>(dst_v[1][3], &src_v[1][3], &kernel[6]);

            ParamElemFixLenVisitorV2None vis;
            op(vis(dst_v[0][0], dst_v[0][1]), output);
            op(vis(dst_v[0][2], dst_v[0][3]), output + 8);
            op(vis(dst_v[1][0], dst_v[1][1]), output + OW * 4);
            op(vis(dst_v[1][2], dst_v[1][3]), output + OW * 4 + 8);
        }
        for (; ow < ow_end; ow++) {
            size_t iw = ow - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v[2];
            load_bias_vec<bias_mode, 1>::impl(
                    &dst_v[0], init, bias + oh * OW * 4 + ow * 4);
            load_bias_vec<bias_mode, 1>::impl(
                    &dst_v[1], init, bias + (oh + 1) * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t src_v[2][3];
            load_vec<3>(src_v[0], input);
            compute_vec<3>(dst_v[0], &src_v[0][0], &kernel[0]);
            load_vec<3>(src_v[1], input + IW * 4);
            compute_vec<3>(dst_v[0], &src_v[1][0], &kernel[3]);
            compute_vec<3>(dst_v[1], &src_v[1][0], &kernel[0]);
            load_vec<3>(src_v[0], input + 2 * IW * 4);
            compute_vec<3>(dst_v[0], &src_v[0][0], &kernel[6]);
            compute_vec<3>(dst_v[1], &src_v[0][0], &kernel[3]);
            load_vec<3>(src_v[1], input + 3 * IW * 4);
            compute_vec<3>(dst_v[1], &src_v[1][0], &kernel[6]);

            op(GiFixLenType2GiFloat32Type(dst_v[0]), output);
            op(GiFixLenType2GiFloat32Type(dst_v[1]), output + OW * 4);
        }
    }
    for (; oh < oh_end; oh++) {
        size_t ih = oh - PH;
        size_t ow = ow_start;
        for (; ow + 3 < ow_end; ow += 4) {
            size_t iw = ow - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v[4];
            load_bias_vec<bias_mode, 4>::impl(
                    &dst_v[0], init, bias + oh * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t src_v[2][6];
            load_vec<6>(src_v[0], input);
            compute_vec<3>(dst_v[0], &src_v[0][0], &kernel[0]);
            compute_vec<3>(dst_v[1], &src_v[0][1], &kernel[0]);
            compute_vec<3>(dst_v[2], &src_v[0][2], &kernel[0]);
            compute_vec<3>(dst_v[3], &src_v[0][3], &kernel[0]);
            load_vec<6>(src_v[1], input + IW * 4);
            compute_vec<3>(dst_v[0], &src_v[1][0], &kernel[3]);
            compute_vec<3>(dst_v[1], &src_v[1][1], &kernel[3]);
            compute_vec<3>(dst_v[2], &src_v[1][2], &kernel[3]);
            compute_vec<3>(dst_v[3], &src_v[1][3], &kernel[3]);
            load_vec<6>(src_v[0], input + 2 * IW * 4);
            compute_vec<3>(dst_v[0], &src_v[0][0], &kernel[6]);
            compute_vec<3>(dst_v[1], &src_v[0][1], &kernel[6]);
            compute_vec<3>(dst_v[2], &src_v[0][2], &kernel[6]);
            compute_vec<3>(dst_v[3], &src_v[0][3], &kernel[6]);
            ParamElemFixLenVisitorV2None vis;
            op(vis(dst_v[0], dst_v[1]), output);
            op(vis(dst_v[2], dst_v[3]), output + 8);
        }
        for (; ow < ow_end; ow++) {
            size_t iw = ow - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v;
            load_bias_vec<bias_mode, 1>::impl(
                    &dst_v, init, bias + oh * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t src_v[3][3];
            load_vec<3>(src_v[0], input);
            compute_vec<3>(dst_v, &src_v[0][0], &kernel[0]);
            load_vec<3>(src_v[1], input + IW * 4);
            compute_vec<3>(dst_v, &src_v[1][0], &kernel[3]);
            load_vec<3>(src_v[2], input + 2 * IW * 4);
            compute_vec<3>(dst_v, &src_v[2][0], &kernel[6]);
            op(GiFixLenType2GiFloat32Type(dst_v), output);
        }
    }
}

template <BiasMode bias_mode, typename Op>
void channel_wise_nchw44_float::do_conv_kern_stride1_5x5(
        const float* src, const float* filter, const float* bias, float* dst,
        const size_t IH, const size_t IW, const size_t OH, const size_t OW,
        const size_t PH, const size_t PW) {
    if (IH == OH && IW == OW && IH >= 5 && IW >= 5 && PH == 2 && PW == 2) {
        channel_wise_nchw44_float::do_conv_kern_5x5_stride1_padding2<bias_mode, Op>(
                src, dst, filter, bias, OH, OW);
        return;
    }

    Op op;
    GI_FLOAT32_t init = GiZeroFloat32();
    if (bias_mode == BiasMode::BROADCAST_CHANNEL_BIAS) {
        init = GiLoadFloat32(bias);
    }
    size_t oh_start = PH;
    size_t ow_start = PW;
    size_t oh_end = IH + PH - 4;
    size_t ow_end = IW + PW - 4;
    if (PH || PW) {
        PaddingCompute<bias_mode, Op>::compute(
                src, bias, dst, 5, 1, IH, IW, OH, OW, PH, PW,
                reinterpret_cast<const GI_FLOAT32_FIXLEN_t*>(filter), init);
    }
    size_t oh = oh_start;
    for (; oh + 1 < oh_end; oh += 2) {
        size_t ih = oh - PH;
        size_t ow = ow_start;
        for (; ow + 1 < ow_end; ow += 2) {
            size_t iw = ow - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v[2][2];
            load_bias_vec<bias_mode, 2>::impl(
                    dst_v[0], init, bias + oh * OW * 4 + ow * 4);
            load_bias_vec<bias_mode, 2>::impl(
                    dst_v[1], init, bias + (oh + 1) * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t kernel[2][5];
            GI_FLOAT32_FIXLEN_t src_v[2][6];
#define COMPUTE_5X5_4(i, dst, src, kernel0, kernel1) \
    load_vec<5>(kernel0, filter + i * 5 * 4);        \
    load_vec<6>(src, input + i * IW * 4);            \
    compute_vec<5>(dst[0][0], &src[0], kernel0);     \
    compute_vec<5>(dst[0][1], &src[1], kernel0);     \
    compute_vec<5>(dst[1][0], &src[0], kernel1);     \
    compute_vec<5>(dst[1][1], &src[1], kernel1)
            // line 0
            load_vec<5>(kernel[0], filter);
            load_vec<6>(src_v[0], input);
            compute_vec<5>(dst_v[0][0], &src_v[0][0], kernel[0]);
            compute_vec<5>(dst_v[0][1], &src_v[0][1], kernel[0]);
            // line 1
            COMPUTE_5X5_4(1, dst_v, src_v[1], kernel[1], kernel[0]);
            // line 2
            COMPUTE_5X5_4(2, dst_v, src_v[0], kernel[0], kernel[1]);
            // line 3
            COMPUTE_5X5_4(3, dst_v, src_v[1], kernel[1], kernel[0]);
            // line 4
            COMPUTE_5X5_4(4, dst_v, src_v[0], kernel[0], kernel[1]);
            // line 5
            load_vec<6>(src_v[1], input + 5 * IW * 4);
            compute_vec<5>(dst_v[1][0], &src_v[1][0], kernel[0]);
            compute_vec<5>(dst_v[1][1], &src_v[1][1], kernel[0]);
#undef COMPUTE_5X5_4
            ParamElemFixLenVisitorV2None vis;
            op(vis(dst_v[0][0], dst_v[0][1]), output);
            op(vis(dst_v[1][0], dst_v[1][1]), output + OW * 4);
        }
        for (; ow < ow_end; ow++) {
            size_t iw = ow - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v[2][1];
            load_bias_vec<bias_mode, 1>::impl(
                    dst_v[0], init, bias + oh * OW * 4 + ow * 4);
            load_bias_vec<bias_mode, 1>::impl(
                    dst_v[1], init, bias + (oh + 1) * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t kernel[2][5];
            GI_FLOAT32_FIXLEN_t src_v[2][5];
#define COMPUTE_5X5_2(i, dst, src, kernel0, kernel1) \
    load_vec<5>(kernel0, filter + i * 5 * 4);        \
    load_vec<5>(src, input + i * IW * 4);            \
    compute_vec<5>(dst[0][0], &src[0], kernel0);     \
    compute_vec<5>(dst[1][0], &src[0], kernel1);
            // line 0
            load_vec<5>(kernel[0], filter);
            load_vec<5>(src_v[0], input);
            compute_vec<5>(dst_v[0][0], &src_v[0][0], kernel[0]);
            // line 1
            COMPUTE_5X5_2(1, dst_v, src_v[1], kernel[1], kernel[0]);
            // line 2
            COMPUTE_5X5_2(2, dst_v, src_v[0], kernel[0], kernel[1]);
            // line 3
            COMPUTE_5X5_2(3, dst_v, src_v[1], kernel[1], kernel[0]);
            // line 4
            COMPUTE_5X5_2(4, dst_v, src_v[0], kernel[0], kernel[1]);
            // line 5
            load_vec<5>(src_v[1], input + 5 * IW * 4);
            compute_vec<5>(dst_v[1][0], &src_v[1][0], kernel[0]);
#undef COMPUTE_5X5_2
            op(GiFixLenType2GiFloat32Type(dst_v[0][0]), output);
            op(GiFixLenType2GiFloat32Type(dst_v[1][0]), output + OW * 4);
        }
    }
    for (; oh < oh_end; oh++) {
        size_t ih = oh - PH;
        size_t ow = ow_start;
        for (; ow + 1 < ow_end; ow += 2) {
            size_t iw = ow - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v[1][2];
            load_bias_vec<bias_mode, 2>::impl(
                    dst_v[0], init, bias + oh * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t kernel[2][5];
            GI_FLOAT32_FIXLEN_t src_v[2][6];
#define COMPUTE_5X5_2(i, dst, src, kernel)      \
    load_vec<5>(kernel, filter + i * 5 * 4);    \
    load_vec<6>(src, input + i * IW * 4);       \
    compute_vec<5>(dst[0][0], &src[0], kernel); \
    compute_vec<5>(dst[0][1], &src[1], kernel)
            // line 0
            COMPUTE_5X5_2(0, dst_v, src_v[0], kernel[0]);
            // line 1
            COMPUTE_5X5_2(1, dst_v, src_v[1], kernel[1]);
            // line 2
            COMPUTE_5X5_2(2, dst_v, src_v[0], kernel[0]);
            // line 3
            COMPUTE_5X5_2(3, dst_v, src_v[1], kernel[1]);
            // line 4
            COMPUTE_5X5_2(4, dst_v, src_v[0], kernel[0]);
#undef COMPUTE_5X5_2
            ParamElemFixLenVisitorV2None vis;
            op(vis(dst_v[0][0], dst_v[0][1]), output);
        }
        for (; ow < ow_end; ow++) {
            size_t iw = ow - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v;
            load_bias_vec<bias_mode, 1>::impl(
                    &dst_v, init, bias + oh * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t kernel[2][5];
            GI_FLOAT32_FIXLEN_t src_v[2][5];
#define COMPUTE_5X5_1(i, dst, src, kernel)   \
    load_vec<5>(kernel, filter + i * 5 * 4); \
    load_vec<5>(src, input + i * IW * 4);    \
    compute_vec<5>(dst, &src[0], kernel)
            // line 0
            COMPUTE_5X5_1(0, dst_v, src_v[0], kernel[0]);
            // line 1
            COMPUTE_5X5_1(1, dst_v, src_v[1], kernel[1]);
            // line 2
            COMPUTE_5X5_1(2, dst_v, src_v[0], kernel[0]);
            // line 3
            COMPUTE_5X5_1(3, dst_v, src_v[1], kernel[1]);
            // line 4
            COMPUTE_5X5_1(4, dst_v, src_v[0], kernel[0]);
#undef COMPUTE_5X5_1
            op(GiFixLenType2GiFloat32Type(dst_v), output);
        }
    }
}

template <BiasMode bias_mode, typename Op>
void channel_wise_nchw44_float::do_conv_kern_stride2_2x2(
        const float* src, const float* filter, const float* bias, float* dst,
        const size_t IH, const size_t IW, const size_t OH, const size_t OW,
        const size_t PH, const size_t PW) {
    GI_FLOAT32_FIXLEN_t kernel[4];
    load_vec<4>(kernel, filter);
    Op op;
    GI_FLOAT32_t init = GiZeroFloat32();
    if (bias_mode == BiasMode::BROADCAST_CHANNEL_BIAS) {
        init = GiLoadFloat32(bias);
    }
    size_t oh_start = (PH + 1) / 2;
    size_t ow_start = (PW + 1) / 2;
    size_t oh_end = (IH + PH) / 2;
    size_t ow_end = (IW + PW) / 2;
    if (PH || PW) {
        PaddingCompute<bias_mode, Op>::compute(
                src, bias, dst, 2, 2, IH, IW, OH, OW, PH, PW, kernel, init);
    }
#define COMPUTE_2X2(dst, src, kernel)        \
    compute_vec<2>(dst[0], &src[0], kernel); \
    compute_vec<2>(dst[1], &src[2], kernel); \
    compute_vec<2>(dst[2], &src[4], kernel); \
    compute_vec<2>(dst[3], &src[6], kernel)
    size_t oh = oh_start;
    for (; oh < oh_end; oh++) {
        size_t ih = oh * 2 - PH;
        size_t ow = ow_start;
        for (; ow + 3 < ow_end; ow += 4) {
            size_t iw = ow * 2 - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v[4];
            load_bias_vec<bias_mode, 4>::impl(
                    &dst_v[0], init, bias + oh * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t src_v[2][8];
            load_vec<8>(src_v[0], input);
            COMPUTE_2X2(dst_v, src_v[0], &kernel[0]);
            load_vec<8>(src_v[1], input + IW * 4);
            COMPUTE_2X2(dst_v, src_v[1], &kernel[2]);
#undef COMPUTE_2X2
            ParamElemFixLenVisitorV2None vis;
            op(vis(dst_v[0], dst_v[1]), output);
            op(vis(dst_v[2], dst_v[3]), output + 8);
        }
        for (; ow < ow_end; ow++) {
            size_t iw = ow * 2 - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v;
            load_bias_vec<bias_mode, 1>::impl(
                    &dst_v, init, bias + oh * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t src_v[2][2];
            load_vec<2>(src_v[0], input);
            compute_vec<2>(dst_v, &src_v[0][0], &kernel[0]);
            load_vec<2>(src_v[1], input + IW * 4);
            compute_vec<2>(dst_v, &src_v[1][0], &kernel[2]);

            op(GiFixLenType2GiFloat32Type(dst_v), output);
        }
    }
#undef COMPUTE_2X2
}

template <BiasMode bias_mode, typename Op>
void channel_wise_nchw44_float::do_conv_kern_stride2_3x3(
        const float* src, const float* filter, const float* bias, float* dst,
        const size_t IH, const size_t IW, const size_t OH, const size_t OW,
        const size_t PH, const size_t PW) {
    GI_FLOAT32_FIXLEN_t kernel[9];
    load_vec<9>(kernel, filter);
    Op op;
    GI_FLOAT32_t init = GiZeroFloat32();
    if (bias_mode == BiasMode::BROADCAST_CHANNEL_BIAS) {
        init = GiLoadFloat32(bias);
    }
    size_t oh_start = (PH + 1) / 2;
    size_t ow_start = (PW + 1) / 2;
    size_t oh_end = (IH + PH - 3) / 2 + 1;
    size_t ow_end = (IW + PW - 3) / 2 + 1;
    if (PH == 1 && PW == 1) {
        PaddingComputeK3P1<bias_mode, Op>::compute(
                src, bias, dst, 2, IH, IW, OH, OW, kernel, init);
    } else if (PH || PW) {
        PaddingCompute<bias_mode, Op>::compute(
                src, bias, dst, 3, 2, IH, IW, OH, OW, PH, PW, kernel, init);
    }
    size_t oh = oh_start;
    for (; oh + 1 < oh_end; oh += 2) {
        size_t ih = oh * 2 - PH;
        size_t ow = ow_start;
        for (; ow + 1 < ow_end; ow += 2) {
            size_t iw = ow * 2 - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v[2][2];
            load_bias_vec<bias_mode, 2>::impl(
                    dst_v[0], init, bias + oh * OW * 4 + ow * 4);
            load_bias_vec<bias_mode, 2>::impl(
                    dst_v[1], init, bias + (oh + 1) * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t src_v[2][5];
            load_vec<5>(src_v[0], input);
            compute_vec<3>(dst_v[0][0], &src_v[0][0], &kernel[0]);
            compute_vec<3>(dst_v[0][1], &src_v[0][2], &kernel[0]);
            load_vec<5>(src_v[1], input + IW * 4);
            compute_vec<3>(dst_v[0][0], &src_v[1][0], &kernel[3]);
            compute_vec<3>(dst_v[0][1], &src_v[1][2], &kernel[3]);
            load_vec<5>(src_v[0], input + 2 * IW * 4);
            compute_vec<3>(dst_v[0][0], &src_v[0][0], &kernel[6]);
            compute_vec<3>(dst_v[0][1], &src_v[0][2], &kernel[6]);
            compute_vec<3>(dst_v[1][0], &src_v[0][0], &kernel[0]);
            compute_vec<3>(dst_v[1][1], &src_v[0][2], &kernel[0]);
            load_vec<5>(src_v[1], input + 3 * IW * 4);
            compute_vec<3>(dst_v[1][0], &src_v[1][0], &kernel[3]);
            compute_vec<3>(dst_v[1][1], &src_v[1][2], &kernel[3]);
            load_vec<5>(src_v[0], input + 4 * IW * 4);
            compute_vec<3>(dst_v[1][0], &src_v[0][0], &kernel[6]);
            compute_vec<3>(dst_v[1][1], &src_v[0][2], &kernel[6]);

            ParamElemFixLenVisitorV2None vis;
            op(vis(dst_v[0][0], dst_v[0][1]), output);
            op(vis(dst_v[1][0], dst_v[1][1]), output + OW * 4);
        }
        for (; ow < ow_end; ow++) {
            size_t iw = ow * 2 - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v[2];
            load_bias_vec<bias_mode, 1>::impl(
                    &dst_v[0], init, bias + oh * OW * 4 + ow * 4);
            load_bias_vec<bias_mode, 1>::impl(
                    &dst_v[1], init, bias + (oh + 1) * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t src_v[2][3];
            load_vec<3>(src_v[0], input);
            compute_vec<3>(dst_v[0], &src_v[0][0], &kernel[0]);
            load_vec<3>(src_v[1], input + IW * 4);
            compute_vec<3>(dst_v[0], &src_v[1][0], &kernel[3]);
            load_vec<3>(src_v[0], input + 2 * IW * 4);
            compute_vec<3>(dst_v[0], &src_v[0][0], &kernel[6]);
            compute_vec<3>(dst_v[1], &src_v[0][0], &kernel[0]);
            load_vec<3>(src_v[1], input + 3 * IW * 4);
            compute_vec<3>(dst_v[1], &src_v[1][0], &kernel[3]);
            load_vec<3>(src_v[0], input + 4 * IW * 4);
            compute_vec<3>(dst_v[1], &src_v[0][0], &kernel[6]);

            op(GiFixLenType2GiFloat32Type(dst_v[0]), output);
            op(GiFixLenType2GiFloat32Type(dst_v[1]), output + OW * 4);
        }
    }
    for (; oh < oh_end; oh++) {
        size_t ih = oh * 2 - PH;
        size_t ow = ow_start;
        for (; ow + 1 < ow_end; ow += 2) {
            size_t iw = ow * 2 - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v[2];
            load_bias_vec<bias_mode, 2>::impl(
                    &dst_v[0], init, bias + oh * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t src_v[3][5];
            load_vec<5>(src_v[0], input);
            compute_vec<3>(dst_v[0], &src_v[0][0], &kernel[0]);
            compute_vec<3>(dst_v[1], &src_v[0][2], &kernel[0]);
            load_vec<5>(src_v[1], input + IW * 4);
            compute_vec<3>(dst_v[0], &src_v[1][0], &kernel[3]);
            compute_vec<3>(dst_v[1], &src_v[1][2], &kernel[3]);
            load_vec<5>(src_v[2], input + 2 * IW * 4);
            compute_vec<3>(dst_v[0], &src_v[2][0], &kernel[6]);
            compute_vec<3>(dst_v[1], &src_v[2][2], &kernel[6]);
            ParamElemFixLenVisitorV2None vis;
            op(vis(dst_v[0], dst_v[1]), output);
        }
        for (; ow < ow_end; ow++) {
            size_t iw = ow * 2 - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v;
            load_bias_vec<bias_mode, 1>::impl(
                    &dst_v, init, bias + oh * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t src_v[3][3];
            load_vec<3>(src_v[0], input);
            compute_vec<3>(dst_v, &src_v[0][0], &kernel[0]);
            load_vec<3>(src_v[1], input + IW * 4);
            compute_vec<3>(dst_v, &src_v[1][0], &kernel[3]);
            load_vec<3>(src_v[2], input + 2 * IW * 4);
            compute_vec<3>(dst_v, &src_v[2][0], &kernel[6]);
            op(GiFixLenType2GiFloat32Type(dst_v), output);
        }
    }
}

template <BiasMode bias_mode, typename Op>
void channel_wise_nchw44_float::do_conv_kern_stride2_5x5(
        const float* src, const float* filter, const float* bias, float* dst,
        const size_t IH, const size_t IW, const size_t OH, const size_t OW,
        const size_t PH, const size_t PW) {
    Op op;
    GI_FLOAT32_t init = GiZeroFloat32();
    if (bias_mode == BiasMode::BROADCAST_CHANNEL_BIAS) {
        init = GiLoadFloat32(bias);
    }
    constexpr size_t stride = 2;
    size_t oh_start = (PH + stride - 1) / stride;
    size_t ow_start = (PW + stride - 1) / stride;
    size_t oh_end = (IH + PH - 5) / stride + 1;
    size_t ow_end = (IW + PW - 5) / stride + 1;
    if (PH || PW) {
        PaddingCompute<bias_mode, Op>::compute(
                src, bias, dst, 5, stride, IH, IW, OH, OW, PH, PW,
                reinterpret_cast<const GI_FLOAT32_FIXLEN_t*>(filter), init);
    }
    size_t oh = oh_start;
    for (; oh + 1 < oh_end; oh += 2) {
        size_t ih = oh * stride - PH;
        size_t ow = ow_start;
        for (; ow + 1 < ow_end; ow += 2) {
            size_t iw = ow * stride - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v[2][2];
            load_bias_vec<bias_mode, 2>::impl(
                    dst_v[0], init, bias + oh * OW * 4 + ow * 4);
            load_bias_vec<bias_mode, 2>::impl(
                    dst_v[1], init, bias + (oh + 1) * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t kernel[3][5];
            GI_FLOAT32_FIXLEN_t src_v[2][7];
#define COMPUTE_5X5_4(i, dst, src, kernel0, kernel1) \
    load_vec<5>(kernel0, filter + i * 5 * 4);        \
    load_vec<7>(src, input + i * IW * 4);            \
    compute_vec<5>(dst[0][0], &src[0], kernel0);     \
    compute_vec<5>(dst[0][1], &src[2], kernel0);     \
    compute_vec<5>(dst[1][0], &src[0], kernel1);     \
    compute_vec<5>(dst[1][1], &src[2], kernel1)

#define COMPUTE_5X5_2(i, dst, src, kernel)   \
    load_vec<7>(src, input + i * IW * 4);    \
    compute_vec<5>(dst[0], &src[0], kernel); \
    compute_vec<5>(dst[1], &src[2], kernel)
            // line 0
            load_vec<5>(kernel[0], filter);
            COMPUTE_5X5_2(0, dst_v[0], src_v[0], kernel[0]);
            // line 1
            load_vec<5>(kernel[1], filter + 5 * 4);
            COMPUTE_5X5_2(1, dst_v[0], src_v[1], kernel[1]);
            // line 2
            COMPUTE_5X5_4(2, dst_v, src_v[0], kernel[2], kernel[0]);
            // line 3
            COMPUTE_5X5_4(3, dst_v, src_v[1], kernel[0], kernel[1]);
            // line 4
            COMPUTE_5X5_4(4, dst_v, src_v[0], kernel[1], kernel[2]);
            // line 5
            COMPUTE_5X5_2(5, dst_v[1], src_v[1], kernel[0]);
            // line 6
            COMPUTE_5X5_2(6, dst_v[1], src_v[0], kernel[1]);
#undef COMPUTE_5X5_4
#undef COMPUTE_5X5_2
            ParamElemFixLenVisitorV2None vis;
            op(vis(dst_v[0][0], dst_v[0][1]), output);
            op(vis(dst_v[1][0], dst_v[1][1]), output + OW * 4);
        }
        for (; ow < ow_end; ow++) {
            size_t iw = ow * stride - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v[2];
            load_bias_vec<bias_mode, 1>::impl(
                    &dst_v[0], init, bias + oh * OW * 4 + ow * 4);
            load_bias_vec<bias_mode, 1>::impl(
                    &dst_v[1], init, bias + (oh + 1) * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t kernel[3][5];
            GI_FLOAT32_FIXLEN_t src_v[2][5];
#define COMPUTE_5X5_2(i, dst, src, kernel0, kernel1) \
    load_vec<5>(kernel0, filter + i * 5 * 4);        \
    load_vec<5>(src, input + i * IW * 4);            \
    compute_vec<5>(dst[0], &src[0], kernel0);        \
    compute_vec<5>(dst[1], &src[0], kernel1);

#define COMPUTE_5X5_1(i, dst, src, kernel) \
    load_vec<5>(src, input + i * IW * 4);  \
    compute_vec<5>(dst, &src[0], kernel);  \
            // line 0
            load_vec<5>(kernel[0], filter);
            COMPUTE_5X5_1(0, dst_v[0], src_v[0], kernel[0]);
            // line 1
            load_vec<5>(kernel[1], filter + 5 * 4);
            COMPUTE_5X5_1(1, dst_v[0], src_v[1], kernel[1]);
            // line 2
            COMPUTE_5X5_2(2, dst_v, src_v[0], kernel[2], kernel[0]);
            // line 3
            COMPUTE_5X5_2(3, dst_v, src_v[1], kernel[0], kernel[1]);
            // line 4
            COMPUTE_5X5_2(4, dst_v, src_v[0], kernel[1], kernel[2]);
            // line 5
            COMPUTE_5X5_1(5, dst_v[1], src_v[1], kernel[0]);
            // line 6
            COMPUTE_5X5_1(6, dst_v[1], src_v[0], kernel[1]);
#undef COMPUTE_5X5_2
#undef COMPUTE_5X5_1
            op(GiFixLenType2GiFloat32Type(dst_v[0]), output);
            op(GiFixLenType2GiFloat32Type(dst_v[1]), output + OW * 4);
        }
    }
    for (; oh < oh_end; oh++) {
        size_t ih = oh * stride - PH;
        size_t ow = ow_start;
        for (; ow < ow_end; ow++) {
            size_t iw = ow * stride - PW;
            const float* input = src + ih * IW * 4 + iw * 4;
            float* output = dst + oh * OW * 4 + ow * 4;
            GI_FLOAT32_FIXLEN_t dst_v;
            load_bias_vec<bias_mode, 1>::impl(
                    &dst_v, init, bias + oh * OW * 4 + ow * 4);
            GI_FLOAT32_FIXLEN_t kernel[2][5];
            GI_FLOAT32_FIXLEN_t src_v[2][5];
#define COMPUTE_5X5_1(i, dst, src, kernel)   \
    load_vec<5>(kernel, filter + i * 5 * 4); \
    load_vec<5>(src, input + i * IW * 4);    \
    compute_vec<5>(dst, &src[0], kernel)
            // line 0
            COMPUTE_5X5_1(0, dst_v, src_v[0], kernel[0]);
            // line 1
            COMPUTE_5X5_1(1, dst_v, src_v[1], kernel[1]);
            // line 2
            COMPUTE_5X5_1(2, dst_v, src_v[0], kernel[0]);
            // line 3
            COMPUTE_5X5_1(3, dst_v, src_v[1], kernel[1]);
            // line 4
            COMPUTE_5X5_1(4, dst_v, src_v[0], kernel[0]);
#undef COMPUTE_5X5_1
            op(GiFixLenType2GiFloat32Type(dst_v), output);
        }
    }
}

#define INSTANTIATION(stride, i, bias, Op)                                          \
    template void                                                                   \
            channel_wise_nchw44_float::do_conv_kern_##stride##_##i##x##i<bias, Op>( \
                    const float*, const float*, const float*, float*, const size_t, \
                    const size_t, const size_t, const size_t, const size_t,         \
                    const size_t);

#define FOR_OP(stride, i, bias)                           \
    INSTANTIATION(stride, i, bias, SigmoidOp<dt_float32>) \
    INSTANTIATION(stride, i, bias, ReluOp<dt_float32>)    \
    INSTANTIATION(stride, i, bias, HSwishOp<dt_float32>)  \
    INSTANTIATION(stride, i, bias, NoneOp<dt_float32>)

#define FOR_BIAS(stride, i)                             \
    FOR_OP(stride, i, BiasMode::NO_BIAS)                \
    FOR_OP(stride, i, BiasMode::BROADCAST_CHANNEL_BIAS) \
    FOR_OP(stride, i, BiasMode::BIAS)

#define FOR_FILTER(stride) \
    FOR_BIAS(stride, 2)    \
    FOR_BIAS(stride, 3)    \
    FOR_BIAS(stride, 5)

#define FOR_STRIDE      \
    FOR_FILTER(stride1) \
    FOR_FILTER(stride2)

FOR_STRIDE

#undef FOR_STRIDE
#undef FOR_FILTER
#undef FOR_BIAS
#undef FOR_OP
#undef INSTANTIATION

// vim: syntax=cpp.doxygen
