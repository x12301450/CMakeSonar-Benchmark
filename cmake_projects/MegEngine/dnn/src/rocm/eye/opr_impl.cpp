#include "src/rocm/eye/opr_impl.h"
#include "hcc_detail/hcc_defs_prologue.h"

#include "src/rocm/eye/eye.h.hip"
#include "src/rocm/utils.h"

namespace megdnn {
namespace rocm {

void EyeImpl::exec(_megdnn_tensor_out dst, _megdnn_workspace workspace) {
    check_exec(dst.layout, workspace.size);
#define cb(DType)                                                                      \
    if (dst.layout.dtype.enumv() == DTypeTrait<DType>::enumv) {                        \
        using ctype = typename DTypeTrait<DType>::ctype;                               \
        eye::exec_internal<ctype>(                                                     \
                dst.ptr<ctype>(), dst.layout.shape[0], dst.layout.shape[1], param().k, \
                hip_stream(handle()));                                                 \
    }
    MEGDNN_FOREACH_COMPUTING_DTYPE(cb)
    cb(::megdnn::dtype::Bool)
#undef cb
}

}  // namespace rocm
}  // namespace megdnn
// vim: syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
