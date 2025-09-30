#include "src/naive/cumsum/opr_impl.h"
#include "src/naive/handle.h"

#include "src/common/reduce_helper.h"
#include "src/common/utils.h"

namespace {

template <typename T>
void exec_internal(
        const T* __restrict src, T* __restrict dst, size_t A, size_t B, size_t C,
        bool exclusive, bool reverse) {
    for (size_t a = 0; a < A; ++a)
        for (size_t c = 0; c < C; ++c) {
            if (exclusive && reverse) {
                T sum = T(0);
                for (size_t b = B; b > 0; --b) {
                    dst[a * B * C + (b - 1) * C + c] = sum;
                    sum += src[a * B * C + (b - 1) * C + c];
                }
            } else if (exclusive && !reverse) {
                T sum = T(0);
                for (size_t b = 0; b < B; ++b) {
                    dst[a * B * C + b * C + c] = sum;
                    sum += src[a * B * C + b * C + c];
                }
            } else if (!exclusive && reverse) {
                T sum = T(0);
                for (size_t b = B; b > 0; --b) {
                    sum += src[a * B * C + (b - 1) * C + c];
                    dst[a * B * C + (b - 1) * C + c] = sum;
                }
            } else if (!exclusive && !reverse) {
                T sum = T(0);
                for (size_t b = 0; b < B; ++b) {
                    sum += src[a * B * C + b * C + c];
                    dst[a * B * C + b * C + c] = sum;
                }
            } else {
                megdnn_assert_internal(false);
            }
        }
}

}  // anonymous namespace

namespace megdnn {
namespace naive {

void CumsumForwardImpl::exec(
        _megdnn_tensor_in src, _megdnn_tensor_out dst, _megdnn_workspace workspace) {
#if !MGE_BUILD_WITHOUT_NAIVE_EXEC
    check_exec(src.layout, dst.layout, workspace.size);

    size_t A, B, C;
    reduce::get_ABC(src.layout, A, B, C, param().axis);
#define cb(DType)                                                               \
    if (src.layout.dtype == DType()) {                                          \
        using ctype = DTypeTrait<DType>::ctype;                                 \
        MEGDNN_DISPATCH_CPU_KERN_OPR(exec_internal<ctype>(                      \
                src.ptr<ctype>(), dst.ptr<ctype>(), A, B, C, param().exclusive, \
                param().reverse));                                              \
        return;                                                                 \
    }
    MEGDNN_FOREACH_COMPUTING_DTYPE(cb)
    megdnn_assert_internal(0);
#undef cb
#else
    __builtin_trap();
#endif
}

}  // namespace naive
}  // namespace megdnn

// vim: syntax=cpp.doxygen
