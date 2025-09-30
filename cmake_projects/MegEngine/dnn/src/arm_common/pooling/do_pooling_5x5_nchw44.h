#pragma once
#include "src/common/utils.h"

namespace megdnn {
namespace arm_common {

#define KERN(mode, stride, ctype)                                            \
    void do_##mode##_pooling_5x5_##stride##_##ctype##_nchw44_NEON(           \
            const int8_t* src, int8_t* dst, size_t IH, size_t IW, size_t OH, \
            size_t OW, size_t PH, size_t PW, const WorkspaceBundle& ws);
KERN(max, stride1, int8)
KERN(max, stride2, int8)
KERN(avg, stride1, int8)
KERN(avg, stride2, int8)
#undef KERN

}  // namespace arm_common
}  // namespace megdnn

// vim: syntax=cpp.doxygen
