#include "./opr_impl.h"

#include "megdnn/tensor_iter.h"
#include "src/common/utils.h"
#include "src/naive/handle.h"

namespace {

using namespace megdnn;

template <typename T>
void forward(const ElemwiseOpParamN<2>& src, const AddUpdate::Param& param) {
    T alpha(param.alpha), beta(param.beta), bias(param.bias);

    auto iter0 = tensor_iter_valonly<T>(src[0]).begin();
    auto iter1 = tensor_iter_valonly<T>(src[1]).begin();
    for (size_t i = 0, it = src[0].layout.total_nr_elems(); i < it; ++i) {
        *iter0 = alpha * *iter0 + beta * *iter1 + bias;
        ++iter0;
        ++iter1;
    }
}

}  // anonymous namespace

namespace megdnn {
namespace naive {

void AddUpdateForwardImpl::exec(_megdnn_tensor_inout dest, _megdnn_tensor_in delta) {
#if !MGE_BUILD_WITHOUT_NAIVE_EXEC
    check_exec(dest.layout, delta.layout);
    ElemwiseOpParamN<2> src = make_param(dest, delta);
    auto param = m_param;
#define cb(DType)                                                 \
    if (dest.layout.dtype == DType()) {                           \
        using ctype = typename DTypeTrait<DType>::ctype;          \
        MEGDNN_DISPATCH_CPU_KERN_OPR(forward<ctype>(src, param)); \
        return;                                                   \
    }
    MEGDNN_FOREACH_COMPUTING_DTYPE(cb)
#undef cb
    megdnn_assert_internal(0);
#else
    __builtin_trap();
#endif
}

}  // namespace naive
}  // namespace megdnn
// vim: syntax=cpp.doxygen
