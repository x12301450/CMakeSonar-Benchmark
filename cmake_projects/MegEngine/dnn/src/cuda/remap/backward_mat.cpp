#include "src/cuda/remap/common.h"
#include "src/cuda/remap/opr_impl.h"
#include "src/cuda/utils.h"

using namespace megdnn;
using namespace cuda;

void RemapBackwardMatImpl::exec(
        _megdnn_tensor_in src, _megdnn_tensor_in map_xy, _megdnn_tensor_in diff,
        _megdnn_tensor_out grad, _megdnn_workspace workspace) {
    check_exec(src.layout, map_xy.layout, diff.layout, grad.layout, workspace.size);
    megdnn_assert(
            (param().imode == param::Remap::InterpolationMode::NEAREST) ||
                    (param().imode == param::Remap::InterpolationMode::LINEAR),
            "only support NEAREST and LINEAR interpolationMode");
    megdnn_assert(
            param().format == param::Remap::Format::NCHW,
            "only support NCHW format for remap backward");
    auto stream = cuda_stream(this->handle());
    int N, C, IH, IW, OH, OW;
    N = src.layout.shape[0];
    C = src.layout.shape[1];
    IH = src.layout.shape[2];
    IW = src.layout.shape[3];
    OH = map_xy.layout.shape[1];
    OW = map_xy.layout.shape[2];

#define cb(dt, _format, bmode, inter_mode)                                             \
    if (param().format == param::Remap::Format::_format &&                             \
        param().border_type == param::Remap::BorderMode::bmode &&                      \
        param().imode == param::Remap::InterpolationMode::inter_mode) {                \
        using ctype = DTypeTrait<dt>::ctype;                                           \
        remap::backwardmat_proxy<                                                      \
                ctype, param_enumv::Remap::Format::_format,                            \
                ::BorderMode::BORDER_##bmode,                                          \
                ::InterpolationMode::INTER_##inter_mode>(                              \
                src.compatible_ptr<ctype>(), map_xy.compatible_ptr<dt_float32>(),      \
                diff.compatible_ptr<ctype>(), grad.compatible_ptr<dt_float32>(), N, C, \
                IH, IW, OH, OW, param().scalar, stream);                               \
        break;                                                                         \
    }

#define support_dtype(dt)                                      \
    case DTypeTrait<dt>::enumv: {                              \
        cb(dt, NCHW, CONSTANT, NEAREST);                       \
        cb(dt, NCHW, REPLICATE, NEAREST);                      \
        cb(dt, NCHW, REFLECT, NEAREST);                        \
        cb(dt, NCHW, REFLECT_101, NEAREST);                    \
        cb(dt, NCHW, WRAP, NEAREST);                           \
        cb(dt, NCHW, CONSTANT, LINEAR);                        \
        cb(dt, NCHW, REPLICATE, LINEAR);                       \
        cb(dt, NCHW, REFLECT, LINEAR);                         \
        cb(dt, NCHW, REFLECT_101, LINEAR);                     \
        cb(dt, NCHW, WRAP, LINEAR);                            \
        megdnn_throw("unsupported border type in remap cuda"); \
    }

    switch (src.layout.dtype.enumv()) {
        support_dtype(dtype::Float32);
        support_dtype(dtype::BFloat16);
        support_dtype(dtype::Float16);
        default:
            megdnn_throw("unsupported dtype in remap backward cuda\n");
    }

#undef support_dtype
#undef cb
}

// vim: syntax=cpp.doxygen
