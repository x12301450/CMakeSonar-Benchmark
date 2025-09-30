#pragma once

#include "src/x86/conv_bias/opr_impl.h"

namespace megdnn {
namespace x86 {
namespace avx2_chanwise_stride1 {

#define KERN(stride, i)                                                   \
    template <BiasMode bias_mode, bool is_quantized, typename Op>         \
    MEGDNN_ATTRIBUTE_TARGET("avx2")                                       \
    void avx2_chanwise_direct_##stride##_##i##x##i##_int8(                \
            const int8_t* src, const int8_t* filter, const int32_t* bias, \
            int32_t* temp, int8_t* dst, const size_t IH, const size_t IW, \
            const size_t OH, const size_t OW, const Op& op);

KERN(stride1, 2)
KERN(stride1, 3)
KERN(stride1, 5)
KERN(stride1, 7)

#undef KERN

}  // namespace avx2_chanwise_stride1

namespace avx2_chanwise_stride2 {

#define KERN(stride, i)                                                   \
    template <BiasMode bias_mode, bool is_quantized, typename Op>         \
    MEGDNN_ATTRIBUTE_TARGET("avx2")                                       \
    void avx2_chanwise_direct_##stride##_##i##x##i##_int8(                \
            const int8_t* src, const int8_t* filter, const int32_t* bias, \
            int32_t* temp, int8_t* dst, const size_t IH, const size_t IW, \
            const size_t OH, const size_t OW, const Op& op);

KERN(stride2, 2)
KERN(stride2, 3)
KERN(stride2, 5)
KERN(stride2, 7)

#undef KERN

}  // namespace avx2_chanwise_stride2
}  // namespace x86
}  // namespace megdnn

// vim: syntax=cpp.doxygen
