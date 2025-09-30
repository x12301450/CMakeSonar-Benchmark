#include "./helper.h"

using namespace megdnn;
using namespace cuda;
using namespace convolution3d;

bool convolution3d::is_cudnn_supported(const ForwardSizeArgs& args) {
    if (args.handle->is_tegra_k1())
        return false;

    if (args.src_layout->dtype.category() != DTypeCategory::FLOAT)
        return false;

    if (args.filter_meta.format != param::Convolution3D::Format::NCDHW)
        return false;
    auto& fm = args.filter_meta;
    return
#if CUDNN_MAJOR >= 7
            true
#else
            fm.group == 1
#endif
            && fm.spatial_ndim == 3;
}

void convolution3d::flip_filter(
        const ForwardSizeArgs& args, const Workspace& workspace, RefPtr& ref_ptr) {
    auto&& fm = args.filter_meta;
    megdnn_assert(fm.group == 1 && fm.spatial_ndim == 3);
    auto OC = fm.ocpg, IC = fm.icpg, FD = fm.spatial[0], FH = fm.spatial[1],
         FW = fm.spatial[2];
    auto dtype = DType::from_enum(fm.dtype_enum);
    megdnn_assert(workspace.size >= dtype.size() * OC * IC * FD * FH * FW);
    TensorND src{{{OC, IC, FD, FH, FW}, dtype}, ref_ptr},
            dst{workspace.raw_ptr + (FD * FH * FW - 1) * dtype.size(), src.layout};
    dst.layout.stride[2] = -dst.layout.stride[2];
    dst.layout.stride[3] = -dst.layout.stride[3];
    dst.layout.stride[4] = -dst.layout.stride[4];
    args.handle->relayout_opr()->exec(src, dst);
    ref_ptr.reset(workspace.raw_ptr);
}

// vim: syntax=cpp.doxygen
