#include "src/x86/warp_affine/opr_impl.h"
#include "src/common/warp_common.h"
#include "src/x86/handle.h"
#include "src/x86/utils.h"
#include "src/x86/warp_affine/warp_affine_cv.h"

using namespace megdnn;
using namespace x86;

void WarpAffineImpl::exec(
        _megdnn_tensor_in src, _megdnn_tensor_in mat, _megdnn_tensor_out dst,
        _megdnn_workspace workspace) {
    check_exec(src.layout, mat.layout, dst.layout, workspace.size);

    if (warp::is_cv_available(
                src.layout, mat.layout, dst.layout, param().imode, param().format) &&
        is_supported(SIMDType::SSE3)) {
        warp_affine_cv_exec(
                src, mat, dst, param().border_val, param().border_mode, param().imode,
                handle());
    } else {
        //! Use fallback implementation
        naive::WarpAffineImpl::exec(src, mat, dst, workspace);
    }
}

// vim: syntax=cpp.doxygen
