#include "src/arm_common/conv_bias/int8/direct_kernels/int8_direct_nchw_nchw44_common.h"
#include "src/arm_common/conv_bias/int8/direct_nchw_nchw44_kern.h"

namespace megdnn {
namespace arm_common {
namespace {
/**
 * @brief core code for calculation patten
 *
 * @tparam src_idx is offset of src reg
 * @tparam weight_idx is offset of weight reg
 * @tparam c_dim is output channel
 * @tparam Func mla operation funcion
 * @tparam stride
 * @tparam T outpur regs type
 * @tparam T2 src regs type
 * @tparam T3 weight regs type
 * @tparam T4 temp regs type
 */

template <
        int src_idx, int weight_idx, int c_dim, int stride, typename T, typename T2,
        typename T3, typename T4>
struct ShiftCalHelper {
    static MEGDNN_ALWAYS_INLINE void impl(T& c, T2& src, T3& weight, T4& temp);
    static MEGDNN_ALWAYS_INLINE void impl(T& c, T2& src, T3& weight);
};
template <
        int src_idx, int weight_idx, int c_dim, int stride, typename T, typename T2,
        typename T3, typename T4>
MEGDNN_ALWAYS_INLINE void cal_helper(T& c, T2& src, T3& weight, T4& temp) {
    ShiftCalHelper<src_idx, weight_idx, c_dim, stride, T, T2, T3, T4>::impl(
            c, src, weight, temp);
}
template <
        int src_idx, int weight_idx, int c_dim, int stride, typename T, typename T2,
        typename T3>
MEGDNN_ALWAYS_INLINE void cal_helper(T& c, T2& src, T3& weight) {
    ShiftCalHelper<src_idx, weight_idx, c_dim, stride, T, T2, T3, int>::impl(
            c, src, weight);
};
template <
        int src_idx, int weight_idx, typename T, typename T2, typename T3, typename T4>
struct ShiftCalHelper<src_idx, weight_idx, 2, 1, T, T2, T3, T4> {
    static MEGDNN_ALWAYS_INLINE void impl(T& c, T2& src, T3& weight, T4& temp) {
        c[0][0] = vdotq_s32_h(
                src[(0 + src_idx) % 8], weight[0][weight_idx], c[0][0], temp[0]);
        c[1][0] = vdotq_s32_h(
                src[(0 + src_idx) % 8], weight[1][weight_idx], c[1][0], temp[1]);
        c[0][1] = vdotq_s32_h(
                src[(1 + src_idx) % 8], weight[0][weight_idx], c[0][1], temp[2]);
        c[1][1] = vdotq_s32_h(
                src[(1 + src_idx) % 8], weight[1][weight_idx], c[1][1], temp[3]);
        c[0][2] = vdotq_s32_h(
                src[(2 + src_idx) % 8], weight[0][weight_idx], c[0][2], temp[0]);
        c[1][2] = vdotq_s32_h(
                src[(2 + src_idx) % 8], weight[1][weight_idx], c[1][2], temp[1]);
        c[0][3] = vdotq_s32_h(
                src[(3 + src_idx) % 8], weight[0][weight_idx], c[0][3], temp[2]);
        c[1][3] = vdotq_s32_h(
                src[(3 + src_idx) % 8], weight[1][weight_idx], c[1][3], temp[3]);

        c[0][4] = vdotq_s32_h(
                src[(4 + src_idx) % 8], weight[0][weight_idx], c[0][4], temp[0]);
        c[1][4] = vdotq_s32_h(
                src[(4 + src_idx) % 8], weight[1][weight_idx], c[1][4], temp[1]);
        c[0][5] = vdotq_s32_h(
                src[(5 + src_idx) % 8], weight[0][weight_idx], c[0][5], temp[2]);
        c[1][5] = vdotq_s32_h(
                src[(5 + src_idx) % 8], weight[1][weight_idx], c[1][5], temp[3]);
        c[0][6] = vdotq_s32_h(
                src[(6 + src_idx) % 8], weight[0][weight_idx], c[0][6], temp[0]);
        c[1][6] = vdotq_s32_h(
                src[(6 + src_idx) % 8], weight[1][weight_idx], c[1][6], temp[1]);
        c[0][7] = vdotq_s32_h(
                src[(7 + src_idx) % 8], weight[0][weight_idx], c[0][7], temp[2]);
        c[1][7] = vdotq_s32_h(
                src[(7 + src_idx) % 8], weight[1][weight_idx], c[1][7], temp[3]);
    }
    static MEGDNN_ALWAYS_INLINE void impl(T&, T2&, T3&);
};
template <
        int src_idx, int weight_idx, typename T, typename T2, typename T3, typename T4>
struct ShiftCalHelper<src_idx, weight_idx, 1, 1, T, T2, T3, T4> {
    static MEGDNN_ALWAYS_INLINE void impl(T& c, T2& src, T3& weight, T4& temp) {
        c[0][0] = vdotq_s32_h(
                src[(0 + src_idx) % 8], weight[0][weight_idx], c[0][0], temp[0]);
        c[0][1] = vdotq_s32_h(
                src[(1 + src_idx) % 8], weight[0][weight_idx], c[0][1], temp[1]);
        c[0][2] = vdotq_s32_h(
                src[(2 + src_idx) % 8], weight[0][weight_idx], c[0][2], temp[2]);
        c[0][3] = vdotq_s32_h(
                src[(3 + src_idx) % 8], weight[0][weight_idx], c[0][3], temp[3]);
        c[0][4] = vdotq_s32_h(
                src[(4 + src_idx) % 8], weight[0][weight_idx], c[0][4], temp[0]);
        c[0][5] = vdotq_s32_h(
                src[(5 + src_idx) % 8], weight[0][weight_idx], c[0][5], temp[1]);
        c[0][6] = vdotq_s32_h(
                src[(6 + src_idx) % 8], weight[0][weight_idx], c[0][6], temp[2]);
        c[0][7] = vdotq_s32_h(
                src[(7 + src_idx) % 8], weight[0][weight_idx], c[0][7], temp[3]);
    }
    static MEGDNN_ALWAYS_INLINE void impl(T&, T2&, T3&);
};

template <BiasMode bias_mode, typename Op, int remain_w, int oc_block>
struct KerNeonXXs2NchwNchw44<bias_mode, Op, remain_w, 1, oc_block, 1> {
    static void impl(
            const int8_t* src_ptr, const int8_t* weight_ptr, const int32_t* bias_ptr,
            int8_t* dst_ptr, int ic, int ih, int iw, int ld_dst_oc, const Op& op) {
        constexpr int stride = 1;
        constexpr int filter_height = 1;
        constexpr int filter_width = 4;
        constexpr int oc_step = 4;
        constexpr int loop_ic_step = 1;
        constexpr int simd_len = 16;
        constexpr int pack_iw_len = 16;
        constexpr int src_reg = 8;
        constexpr int weight_reg = 1;

        const int ic_stride = ih * iw * pack_iw_len;
        const int ld_weight_oc = oc_step * filter_height * filter_width * ic;
        constexpr int c_dim = OCHelper<oc_block>::val;
        int32x4_t c[c_dim][8];
        init_ocx_ow8<c_dim, bias_mode, remain_w>(c, bias_ptr, oc_step);

        for (int ic_idx = 0; ic_idx < ic; ic_idx += loop_ic_step) {
            const int8_t* nchw_src_ptr = src_ptr + ic_idx * ic_stride;
            int8x16_t src[src_reg];
            int8x16_t dot4_weight[c_dim][weight_reg];
            int16x8_t temp_c[4];
            load_helper<weight_reg, 0, simd_len, c_dim, Vld1q_s8>(
                    dot4_weight, weight_ptr, ld_weight_oc);
            load_helper<src_reg, 0, simd_len, 0, Vld1q_s8>(
                    src, nchw_src_ptr + 0 * iw * pack_iw_len, 0);
            cal_helper<0, 0, c_dim, stride>(c, src, dot4_weight, temp_c);

            weight_ptr += oc_step * filter_height * filter_width;
        }

        store_ocx_ow8_remain_static_dt<c_dim, remain_w, Op, dt_qint8*>(
                c, op, dst_ptr, ld_dst_oc);
    }
};

template <BiasMode bias_mode, typename Op, int remain_w, int oc_block>
struct KerNeonXXs2NchwNchw44<bias_mode, Op, remain_w, 2, oc_block, 1> {
    static void impl(
            const int8_t* src_ptr, const int8_t* weight_ptr, const int32_t* bias_ptr,
            int8_t* dst_ptr, int ic, int ih, int iw, int ld_dst_oc, const Op& op) {
        constexpr int stride = 1;
        constexpr int filter_height = 2;
        constexpr int filter_width = 4;
        constexpr int oc_step = 4;
        constexpr int loop_ic_step = 1;
        constexpr int simd_len = 16;
        constexpr int pack_iw_len = 16;
        constexpr int src_reg = 8;
        constexpr int weight_reg = 1;

        const int ic_stride = ih * iw * pack_iw_len;
        const int ld_weight_oc = oc_step * filter_height * filter_width * ic;
        constexpr int c_dim = OCHelper<oc_block>::val;
        int32x4_t c[c_dim][8];
        init_ocx_ow8<c_dim, bias_mode, remain_w>(c, bias_ptr, oc_step);

        for (int ic_idx = 0; ic_idx < ic; ic_idx += loop_ic_step) {
            const int8_t* nchw_src_ptr = src_ptr + ic_idx * ic_stride;
            int8x16_t src[src_reg];
            int8x16_t dot4_weight[c_dim][weight_reg];
            int16x8_t temp_c[4];
            load_helper<weight_reg, 0, simd_len, c_dim, Vld1q_s8>(
                    dot4_weight, weight_ptr, ld_weight_oc);
            load_helper<src_reg, 0, simd_len, 0, Vld1q_s8>(
                    src, nchw_src_ptr + 0 * iw * pack_iw_len, 0);
            cal_helper<0, 0, c_dim, stride>(c, src, dot4_weight, temp_c);

            load_helper<weight_reg, 0, simd_len, c_dim, Vld1q_s8>(
                    dot4_weight, weight_ptr + 1 * filter_width * oc_step, ld_weight_oc);
            load_helper<src_reg, 0, simd_len, 0, Vld1q_s8>(
                    src, nchw_src_ptr + 1 * iw * pack_iw_len, 0);
            cal_helper<0, 0, c_dim, stride>(c, src, dot4_weight, temp_c);

            weight_ptr += oc_step * filter_height * filter_width;
        }

        store_ocx_ow8_remain_static_dt<c_dim, remain_w, Op, dt_qint8*>(
                c, op, dst_ptr, ld_dst_oc);
    }
};

template <BiasMode bias_mode, typename Op, int remain_w, int oc_block>
struct KerNeonXXs2NchwNchw44<bias_mode, Op, remain_w, 3, oc_block, 1> {
    static void impl(
            const int8_t* src_ptr, const int8_t* weight_ptr, const int32_t* bias_ptr,
            int8_t* dst_ptr, int ic, int ih, int iw, int ld_dst_oc, const Op& op) {
        constexpr int stride = 1;
        constexpr int filter_height = 3;
        constexpr int filter_width = 4;
        constexpr int oc_step = 4;
        constexpr int loop_ic_step = 1;
        constexpr int simd_len = 16;
        constexpr int pack_iw_len = 16;
        constexpr int src_reg = 8;
        constexpr int weight_reg = 1;

        const int ic_stride = ih * iw * pack_iw_len;
        const int ld_weight_oc = oc_step * filter_height * filter_width * ic;
        constexpr int c_dim = OCHelper<oc_block>::val;
        int32x4_t c[c_dim][8];
        init_ocx_ow8<c_dim, bias_mode, remain_w>(c, bias_ptr, oc_step);

        for (int ic_idx = 0; ic_idx < ic; ic_idx += loop_ic_step) {
            const int8_t* nchw_src_ptr = src_ptr + ic_idx * ic_stride;
            int8x16_t src[src_reg];
            int8x16_t dot4_weight[c_dim][weight_reg];
            int16x8_t temp_c[4];
            load_helper<weight_reg, 0, simd_len, c_dim, Vld1q_s8>(
                    dot4_weight, weight_ptr, ld_weight_oc);

            load_helper<src_reg, 0, simd_len, 0, Vld1q_s8>(
                    src, nchw_src_ptr + 0 * iw * pack_iw_len, 0);
            cal_helper<0, 0, c_dim, stride>(c, src, dot4_weight, temp_c);
            load_helper<weight_reg, 0, simd_len, c_dim, Vld1q_s8>(
                    dot4_weight, weight_ptr + 1 * filter_width * oc_step, ld_weight_oc);

            load_helper<src_reg, 0, simd_len, 0, Vld1q_s8>(
                    src, nchw_src_ptr + 1 * iw * pack_iw_len, 0);
            cal_helper<0, 0, c_dim, stride>(c, src, dot4_weight, temp_c);

            load_helper<weight_reg, 0, simd_len, c_dim, Vld1q_s8>(
                    dot4_weight, weight_ptr + 2 * filter_width * oc_step, ld_weight_oc);
            load_helper<src_reg, 0, simd_len, 0, Vld1q_s8>(
                    src, nchw_src_ptr + 2 * iw * pack_iw_len, 0);
            cal_helper<0, 0, c_dim, stride>(c, src, dot4_weight, temp_c);

            weight_ptr += oc_step * filter_height * filter_width;
        }
        store_ocx_ow8_remain_static_dt<c_dim, remain_w, Op, dt_qint8*>(
                c, op, dst_ptr, ld_dst_oc);
    }
};

template <BiasMode bias_mode, typename Op, int remain_w, int oc_block>
struct KerNeonXXs2NchwNchw44<bias_mode, Op, remain_w, 5, oc_block, 1> {
    static void impl(
            const int8_t* src_ptr, const int8_t* weight_ptr, const int32_t* bias_ptr,
            int8_t* dst_ptr, int ic, int ih, int iw, int ld_dst_oc, const Op& op) {
        constexpr int stride = 1;
        constexpr int filter_height = 5;
        constexpr int filter_width = 8;
        constexpr int oc_step = 4;
        constexpr int loop_ic_step = 1;
        constexpr int simd_len = 16;
        constexpr int pack_iw_len = 16;
        constexpr int src_reg = 8;
        constexpr int weight_reg = 2;

        const int ic_stride = ih * iw * pack_iw_len;
        const int ld_weight_oc = oc_step * filter_height * filter_width * ic;
        constexpr int c_dim = OCHelper<oc_block>::val;
        int32x4_t c[c_dim][8];
        init_ocx_ow8<c_dim, bias_mode, remain_w>(c, bias_ptr, oc_step);

        for (int ic_idx = 0; ic_idx < ic; ic_idx += loop_ic_step) {
            const int8_t* nchw_src_ptr = src_ptr + ic_idx * ic_stride;
            int8x16_t src[src_reg];
            int8x16_t dot4_weight[c_dim][weight_reg];
            int16x8_t temp_c[4];
#define cb(step)                                                                     \
    load_helper<weight_reg, 0, simd_len, c_dim, Vld1q_s8>(                           \
            dot4_weight, weight_ptr + step * filter_width * oc_step, ld_weight_oc);  \
    load_helper<src_reg, 0, simd_len, 0, Vld1q_s8>(                                  \
            src, nchw_src_ptr + step * iw * pack_iw_len, 0);                         \
    cal_helper<0, 0, c_dim, stride>(c, src, dot4_weight, temp_c);                    \
    load_helper<4, 0, simd_len, 0, Vld1q_s8>(                                        \
            src, nchw_src_ptr + step * iw * pack_iw_len + src_reg * pack_iw_len, 0); \
    cal_helper<4, 1, c_dim, stride>(c, src, dot4_weight, temp_c);
            UNROLL_CALL_RAW(5, cb);
#undef cb
            weight_ptr += oc_step * filter_height * filter_width;
        }
        store_ocx_ow8_remain_static_dt<c_dim, remain_w, Op, dt_qint8*>(
                c, op, dst_ptr, ld_dst_oc);
    }
};

template <BiasMode bias_mode, typename Op, int remain_w, int oc_block>
struct KerNeonXXs2NchwNchw44<bias_mode, Op, remain_w, 7, oc_block, 1> {
    static void impl(
            const int8_t* src_ptr, const int8_t* weight_ptr, const int32_t* bias_ptr,
            int8_t* dst_ptr, int ic, int ih, int iw, int ld_dst_oc, const Op& op) {
        constexpr int stride = 1;
        constexpr int filter_height = 7;
        constexpr int filter_width = 8;
        constexpr int oc_step = 4;
        constexpr int loop_ic_step = 1;
        constexpr int simd_len = 16;
        constexpr int pack_iw_len = 16;
        constexpr int src_reg = 8;
        constexpr int weight_reg = 2;

        const int ic_stride = ih * iw * pack_iw_len;
        const int ld_weight_oc = oc_step * filter_height * filter_width * ic;
        constexpr int c_dim = OCHelper<oc_block>::val;
        int32x4_t c[c_dim][8];
        init_ocx_ow8<c_dim, bias_mode, remain_w>(c, bias_ptr, oc_step);

        for (int ic_idx = 0; ic_idx < ic; ic_idx += loop_ic_step) {
            const int8_t* nchw_src_ptr = src_ptr + ic_idx * ic_stride;
            int8x16_t src[src_reg];
            int8x16_t dot4_weight[c_dim][weight_reg];
            int16x8_t temp_c[4];
#define cb(step)                                                                     \
    load_helper<weight_reg, 0, simd_len, c_dim, Vld1q_s8>(                           \
            dot4_weight, weight_ptr + step * filter_width * oc_step, ld_weight_oc);  \
    load_helper<src_reg, 0, simd_len, 0, Vld1q_s8>(                                  \
            src, nchw_src_ptr + step * iw * pack_iw_len, 0);                         \
    cal_helper<0, 0, c_dim, stride>(c, src, dot4_weight, temp_c);                    \
    load_helper<4, 0, simd_len, 0, Vld1q_s8>(                                        \
            src, nchw_src_ptr + step * iw * pack_iw_len + src_reg * pack_iw_len, 0); \
    cal_helper<4, 1, c_dim, stride>(c, src, dot4_weight, temp_c);

            UNROLL_CALL_RAW(7, cb);
#undef cb
            weight_ptr += oc_step * filter_height * filter_width;
        }
        store_ocx_ow8_remain_static_dt<c_dim, remain_w, Op, dt_qint8*>(
                c, op, dst_ptr, ld_dst_oc);
    }
};
}  // namespace

namespace int8_direct_nchw_nchw44 {
/**
 * pack {oc / 4, fh, fw, ic, 4(oc)} to {oc / 4, ic, fh ,fw/4, 4(oc)*4(fw)}
 * pack interleave two adjacent row in filter to one row
 * */
template <BiasMode bias_mode, typename Op, size_t filter_size>
struct ConvDiectStrideInt8NchwNchw44<bias_mode, Op, filter_size, 1> {
    static void impl(
            const int8_t* src, const int8_t* filter, const int32_t* bias, int32_t* temp,
            int8_t* dst, const size_t oc, const size_t ic, const size_t ih,
            const size_t iw, const size_t oh, const size_t ow, const Op& op) {
        MEGDNN_MARK_USED_VAR(temp);
        constexpr int stride = 1;
        constexpr size_t fh = filter_size;
        constexpr size_t fw = (filter_size + 3) / 4 * 4;
        constexpr size_t ic_step = 1;
        constexpr size_t big_oc_step = 8;
        constexpr size_t oc_step = 4;
        constexpr size_t ih_step = 1;
        constexpr size_t oh_step = 1;
        constexpr size_t ow_step = 8;
        constexpr size_t stride_h = stride;
        constexpr size_t stride_w = stride;
        constexpr int pack_iw_len = 16;

        const size_t img_stride = oh * ow;
        const size_t ow_end = ow / ow_step * ow_step;
        const size_t ow_remain = ow - ow_end;
        const size_t oc_end = oc / big_oc_step * big_oc_step;
        const size_t oc_remain = oc - oc_end;
        const int ld_dst_oc = oc_step * img_stride;

        using remain_fun = std::function<void(
                const int8_t* src_ptr, const int8_t* weight_ptr,
                const int32_t* bias_ptr, int8_t* dst_ptr, int ic, int ih, int iw,
                int ld_dst_oc, const Op& op)>;
        remain_fun kern_big_oc_remain = nullptr;
        remain_fun kern_small_oc_remain = nullptr;
        switch (ow_remain) {
#define cb(step)                                                              \
    case step:                                                                \
        kern_big_oc_remain = KerNeonXXs2NchwNchw44<                           \
                bias_mode, Op, step, filter_size, big_oc_step, stride>::impl; \
        kern_small_oc_remain = KerNeonXXs2NchwNchw44<                         \
                bias_mode, Op, step, filter_size, oc_step, stride>::impl;     \
        break;

            UNROLL_CALL_RAW(8, cb);
            default:
                megdnn_assert(0, "no remain %zu for kern", ow_remain);
        }

        for (size_t oc_idx = 0; oc_idx < oc_end; oc_idx += big_oc_step) {
            const size_t weight_offset = oc_idx * ic * fh * fw;
            for (size_t oh_idx = 0; oh_idx < oh; oh_idx += oh_step) {
                for (size_t ow_idx = 0; ow_idx < ow_end; ow_idx += ow_step) {
                    const size_t src_offset =
                            (oh_idx * stride_h * iw + ow_idx * stride_w * ih_step) *
                            ic_step * pack_iw_len;
                    const size_t dst_offset =
                            oc_idx * img_stride + (oh_idx * ow + ow_idx) * oc_step;

                    KerNeonXXs2NchwNchw44<
                            bias_mode, Op, ow_step, filter_size, big_oc_step, stride>::
                            impl(src + src_offset, filter + weight_offset,
                                 bias + oc_idx, dst + dst_offset, ic, ih, iw, ld_dst_oc,
                                 op);
                }
                if (ow_remain > 0) {
                    const size_t src_offset =
                            (oh_idx * stride_h * iw + ow_end * stride_w * ih_step) *
                            ic_step * pack_iw_len;
                    const size_t dst_offset =
                            oc_idx * img_stride + (oh_idx * ow + ow_end) * oc_step;
                    kern_big_oc_remain(
                            src + src_offset, filter + weight_offset, bias + oc_idx,
                            dst + dst_offset, ic, ih, iw, ld_dst_oc, op);
                }
            }
        }

        if (oc_remain > 0) {
            size_t oc_idx = oc_end;
            const size_t weight_offset = oc_idx * ic * fh * fw;
            for (size_t oh_idx = 0; oh_idx < oh; oh_idx += oh_step) {
                for (size_t ow_idx = 0; ow_idx < ow_end; ow_idx += ow_step) {
                    const size_t src_offset =
                            (oh_idx * stride_h * iw + ow_idx * stride_w * ih_step) *
                            ic_step * pack_iw_len;
                    const size_t dst_offset =
                            oc_idx * img_stride + (oh_idx * ow + ow_idx) * oc_step;
                    KerNeonXXs2NchwNchw44<
                            bias_mode, Op, ow_step, filter_size, oc_step, stride>::
                            impl(src + src_offset, filter + weight_offset,
                                 bias + oc_idx, dst + dst_offset, ic, ih, iw, ld_dst_oc,
                                 op);
                }
                if (ow_remain > 0) {
                    const size_t src_offset =
                            (oh_idx * stride_h * iw + ow_end * stride_w * ih_step) *
                            ic_step * pack_iw_len;
                    const size_t dst_offset =
                            oc_idx * img_stride + (oh_idx * ow + ow_end) * oc_step;
                    kern_small_oc_remain(
                            src + src_offset, filter + weight_offset, bias + oc_idx,
                            dst + dst_offset, ic, ih, iw, ld_dst_oc, op);
                }
            }
        }
    }
};

#define INSTANCE_CONV_KERN_FUN(stride, filter_size, bias_mode, Op) \
    template struct megdnn::arm_common::int8_direct_nchw_nchw44::  \
            ConvDiectStrideInt8NchwNchw44<bias_mode, Op, filter_size, stride>;

#define INSTANCE_OP_PARAM(stride, filter, bias_mode)                               \
    INSTANCE_CONV_KERN_FUN(                                                        \
            stride, filter, bias_mode, TypeCvtOp<dt_qint32 MEGDNN_COMMA dt_qint8>) \
    INSTANCE_CONV_KERN_FUN(                                                        \
            stride, filter, bias_mode, ReluOp<dt_qint32 MEGDNN_COMMA dt_qint8>)    \
    INSTANCE_CONV_KERN_FUN(                                                        \
            stride, filter, bias_mode, HSwishOp<dt_qint32 MEGDNN_COMMA dt_qint8>)

#define INSTANCE_BIAS_MODE_PARAM(stride, filter)         \
    INSTANCE_OP_PARAM(stride, filter, BiasMode::NO_BIAS) \
    INSTANCE_OP_PARAM(stride, filter, BiasMode::BROADCAST_CHANNEL_BIAS)

#define INSTANCE_CONV_KERN(stride, filter) INSTANCE_BIAS_MODE_PARAM(stride, filter)

}  // namespace int8_direct_nchw_nchw44
}  // namespace arm_common
}  // namespace megdnn

// vim: syntax=cpp.doxygen
