#include "megbrain/opr/dnn/adaptive_pooling.h"
#include "../algo_chooser.h"
#include "../blob_manager_impl.h"
#include "../dnn_op_helper.h"
#include "../op_trait.h"
#include "megbrain/imperative/ops/autogen.h"
#include "megbrain/opr/io.h"

namespace mgb::imperative {

namespace {
namespace adaptive_pooling {
auto apply_on_var_node(const OpDef& def, const VarNodeArray& inputs) {
    auto&& pool = static_cast<const AdaptivePooling&>(def);
    OperatorNodeConfig config{pool.make_name()};
    size_t nr_inp = inputs.size();
    if (nr_inp > 1) {
        return opr::AdaptivePooling::make(inputs[0], inputs[1], pool.param(), config);
    }

    HostTensorND hv = HostTensorND(inputs[0]->comp_node(), {2}, dtype::Int32());
    auto* ptr = hv.ptr<dt_int32>();
    ptr[0] = pool.shape[0];
    ptr[1] = pool.shape[1];
    auto graph = inputs[0]->owner_graph();
    auto target_shape = opr::ImmutableTensor::make(*graph, hv, config);
    return opr::AdaptivePooling::make(inputs[0], target_shape, pool.param(), config);
}

std::tuple<SmallVector<LogicalTensorDesc>, bool> infer_output_attrs_fallible(
        const OpDef& def, const SmallVector<LogicalTensorDesc>& inputs) {
    auto&& pool = static_cast<const AdaptivePooling&>(def);
    size_t nr_inp = inputs.size();
    auto&& src = inputs[0];
    TensorLayout dst_layout(src.layout.dtype);
    if (src.layout.is_empty()) {
        return {{{TensorLayout(src.layout.dtype), src.comp_node}}, false};
    }

    const dt_int32* oshp2d = nullptr;
    dst_layout.ndim = 4u;
    bool tshp1n = false;
    if (nr_inp == 1) {
        oshp2d = pool.shape.data();
    } else {
        auto&& tshp = inputs[1];
        if (tshp.value.empty()) {
            return {{{TensorLayout(src.layout.dtype), src.comp_node}}, false};
        }
        mgb_assert(
                tshp.layout.ndim == 1,
                "target shape of AdaptivePooling expects ndim=1; got ndim=%lu actually",
                tshp.layout.ndim);
        oshp2d = tshp.value.ptr<dt_int32>();
        tshp1n = tshp.layout.total_nr_elems() == 1;
    }
    auto param_format = pool.param().format;
    if (param_format == opr::AdaptivePooling::Param::Format::NCHW) {
        dst_layout[0] = src.layout[0];
        dst_layout[1] = src.layout[1];
        dst_layout[2] = oshp2d[0];
        dst_layout[3] = tshp1n ? oshp2d[0] : oshp2d[1];
    } else if (param_format == opr::AdaptivePooling::Param::Format::NHWC) {
        dst_layout[0] = src.layout[0];
        dst_layout[1] = oshp2d[0];
        dst_layout[2] = tshp1n ? oshp2d[0] : oshp2d[1];
        dst_layout[3] = src.layout[3];
    } else {
        mgb_throw(MegBrainError, "AdaptivePooling only support NCHW or NHWC format");
    }
    dst_layout.init_contiguous_stride();
    return {{{dst_layout, src.comp_node}}, true};
}

SmallVector<TensorPtr> apply_on_physical_tensor(
        const OpDef& def, const SmallVector<TensorPtr>& inputs,
        SmallVector<LogicalTensorDesc>& output_descs, const bool& validated) {
    auto&& pooling = def.cast_final_safe<AdaptivePooling>();
    auto&& cn = inputs[0]->comp_node();

    auto&& src_layout = inputs[0]->layout();
    TensorLayout dst_layout{inputs[0]->dtype()};
    auto param_format = pooling.format;
    using PFormat = megdnn::param::AdaptivePooling::Format;
    if (!validated) {
        dst_layout.ndim = src_layout.ndim;
        const dt_int32* oshp2d = nullptr;
        bool tshp1n = false;
        if (inputs.size() == 2) {
            auto&& tshp_nd = inputs[1];
            tshp1n = inputs[1]->layout().total_nr_elems() == 1;
            oshp2d = tshp_nd->get_value().proxy_to_default_cpu().ptr<dt_int32>();
        } else {
            oshp2d = pooling.shape.data();
        }
        if (param_format == PFormat::NCHW) {
            dst_layout[0] = src_layout[0];
            dst_layout[1] = src_layout[1];
            dst_layout[2] = oshp2d[0];
            dst_layout[3] = tshp1n ? oshp2d[0] : oshp2d[1];
        } else if (param_format == PFormat::NHWC) {
            dst_layout[0] = src_layout[0];
            dst_layout[1] = oshp2d[0];
            dst_layout[2] = tshp1n ? oshp2d[0] : oshp2d[1];
            dst_layout[3] = src_layout[3];
        } else {
            mgb_throw(
                    MegBrainError, "AdaptivePooling only support NCHW or NHWC format");
        }
        dst_layout.init_contiguous_stride();
    } else {
        dst_layout = output_descs[0].layout;
    }

    size_t IH, IW, OH, OW;
    if (param_format == PFormat::NCHW) {
        IH = src_layout[2];
        IW = src_layout[3];
        OH = dst_layout[2];
        OW = dst_layout[3];
    } else if (param_format == PFormat::NHWC) {
        IH = src_layout[1];
        IW = src_layout[2];
        OH = dst_layout[1];
        OW = dst_layout[2];
    } else {
        mgb_throw(MegBrainError, "AdaptivePooling only support NCHW or NHWC format");
    }

    if (src_layout.ndim == 4 && src_layout.is_empty()) {
        mgb_assert(
                IH != 0 && IW != 0,
                "Pooling expect input to have non-zero size for non-batch "
                "dimensions, but the input has layout %s",
                src_layout.to_string().c_str());
        return {Tensor::make(dst_layout, cn)};
    }

    // adaptive_pooling param to pooling
    auto&& param = megdnn::Pooling::Param();
    param.mode = pooling.mode;
    param.format = pooling.format;
    param.pad_h = param.pad_w = 0;
    param.stride_h = IH / OH;
    param.stride_w = IW / OW;
    param.window_h = IH - (OH - 1) * param.stride_h;
    param.window_w = IW - (OW - 1) * param.stride_w;

    DnnOprCaller<megdnn::Pooling> dnn_opr(cn, param, megdnn::param::ExecutionPolicy{});
    auto src = inputs[0];
    auto dst = Tensor::make(dst_layout, cn);
    dnn_opr.exec_fastrun(inputs[0], dst);
    return {dst};
}

OP_TRAIT_REG(AdaptivePooling, AdaptivePooling)
        .apply_on_var_node(apply_on_var_node)
        .infer_output_attrs_fallible(infer_output_attrs_fallible)
        .apply_on_physical_tensor(apply_on_physical_tensor)
        .fallback();
}  // namespace adaptive_pooling
}  // namespace

}  // namespace mgb::imperative
