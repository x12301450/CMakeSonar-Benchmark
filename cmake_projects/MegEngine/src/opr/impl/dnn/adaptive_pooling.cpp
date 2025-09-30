#include "megbrain/opr/dnn/adaptive_pooling.h"
#include "../internal/megdnn_opr_wrapper.inl"
#include "megbrain/graph/grad_impl.h"
#include "megbrain/opr/utility.h"

#include "megdnn/opr_param_defs.h"
#include "megdnn/oprs/nn.h"

using namespace mgb;
using namespace opr;

MGB_DYN_TYPE_OBJ_FINAL_IMPL(AdaptivePoolingForward);
AdaptivePoolingForward::AdaptivePoolingForward(
        VarNode* src, VarNode* out_shape, const Param& param,
        const OperatorNodeConfig& config)
        : Super(OperatorNodeBaseCtorParam{
                  src->owner_graph(), config, "adaptive_pooling", {src, out_shape}}) {
    init_megdnn_opr(*this, param);
    add_input({src, out_shape});
    output(0)->add_flag(VarNode::Flag::ALLOW_EMPTY_SHAPE);
    outshape_by_symvar_enable(1, 1);
}

SymbolVar AdaptivePoolingForward::make(
        SymbolVar src, SymbolVar out_shape, const Param& param,
        const OperatorNodeConfig& config) {
    return src.insert_single_output_opr<AdaptivePoolingForward>(
            src.node(), out_shape.node(), param, config);
}

void AdaptivePoolingForward::scn_do_execute() {
    if (input(0)->dev_tensor().empty()) {
        mgb_assert(output(0)->dev_tensor().empty());
        return;
    }
    megdnn_opr()->exec(
            input(0)->dev_tensor().as_megdnn(), output(0)->dev_tensor().as_megdnn(),
            intl::get_megdnn_workspace_from_var(output().back()));
}

MAKE_NODE_PROP_WITH_ZERO_SHAPE_2(AdaptivePoolingForward, 0, 1)

void AdaptivePoolingForward::outshape_by_symvar_do_get_output_shape(
        TensorShape& dest, const ShapeInferInfo& shpinfo) {
    TensorShape oshp2d;
    cg::copy_tensor_value_to_shape(oshp2d, *shpinfo.shpval_inp_val.at(0));
    auto src = shpinfo.shape_inp_shp.at(0);
    mgb_assert(
            (src.ndim == 4 || src.ndim == 5) && (oshp2d.ndim == 2 || oshp2d.ndim == 1),
            "shape mismatch for AdaptivePooling: src=%s, out2d=%s",
            src.to_string().c_str(), oshp2d.to_string().c_str());

    auto param_format = param().format;
    bool tshp1n = oshp2d.ndim == 1;
    if (param_format == Param::Format::NCHW) {
        dest.ndim = 4;
        dest.shape[0] = src.shape[0];
        dest.shape[1] = src.shape[1];
        dest.shape[2] = oshp2d.shape[0];
        dest.shape[3] = (tshp1n) ? oshp2d.shape[0] : oshp2d.shape[1];
    } else if (param_format == Param::Format::NHWC) {
        dest.ndim = 4;
        dest.shape[0] = src.shape[0];
        dest.shape[1] = oshp2d.shape[0];
        dest.shape[2] = (tshp1n) ? oshp2d.shape[0] : oshp2d.shape[1];
        dest.shape[3] = src.shape[3];
    } else if (
            param_format == Param::Format::NCHW44 ||
            param_format == Param::Format::NCHW88) {
        dest.ndim = 5;
        dest.shape[0] = src.shape[0];
        dest.shape[1] = src.shape[1];
        dest.shape[2] = oshp2d.shape[0];
        dest.shape[3] = (tshp1n) ? oshp2d.shape[0] : oshp2d.shape[1];
        dest.shape[4] = src.shape[4];
    } else {
        mgb_throw(
                MegBrainError, "AdaptivePooling not support %d format",
                (int)param_format);
    }
}

size_t AdaptivePoolingForward::get_workspace_size_bytes(
        const TensorShapeArray& input_shapes,
        const TensorShapeArray& output_shapes) const {
    return megdnn_opr()->get_workspace_in_bytes(
            {input_shapes[0], this->input(0)->dtype(), this->input(0)->format()},
            {output_shapes[0], this->output(0)->dtype(), this->output(0)->format()});
}

void AdaptivePoolingForward::init_output_dtype() {
    output(0)->dtype(input(0)->dtype());
}

void AdaptivePoolingForward::add_input_layout_constraint() {
    mixin::megdnn_utils::add_input_layout_constraint_contig(*this);
}

void AdaptivePoolingForward::init_output_static_infer_desc() {
    Super::init_output_static_infer_desc();
    init_output_static_infer_desc_workspace(false);
}

void AdaptivePoolingForward::record_execute_deps(ExecDependencyArray& deps) {
    record_megdnn_opr(deps);
}

#if MGB_ENABLE_GRAD
MGB_IMPL_OPR_GRAD(AdaptivePoolingForward) {
    if (wrt_idx == 0) {
        // wrt src
        SymbolVar grad = AdaptivePoolingBackward::make(
                opr.input(0), opr.input(1), opr.output(0), out_grad[0], opr.param());
        return grad.node();
    } else {
        mgb_assert(wrt_idx == 1);
        return InvalidGrad::make(opr, wrt_idx);
    }
}
#endif

MGB_DYN_TYPE_OBJ_FINAL_IMPL(AdaptivePoolingBackward);
AdaptivePoolingBackward::AdaptivePoolingBackward(
        VarNode* src, VarNode* out_shape, VarNode* dst, VarNode* diff,
        const Param& param, const OperatorNodeConfig& config)
        : Super(
                  OperatorNodeBaseCtorParam{
                          src->owner_graph(), config, "adaptive_pooling_bwd", {src}},
                  0, true) {
    init_megdnn_opr(*this, param);
    add_input({src, out_shape, dst, diff});
}

SymbolVar AdaptivePoolingBackward::make(
        SymbolVar src, SymbolVar out_shape, SymbolVar dst, SymbolVar diff,
        const Param& param, const OperatorNodeConfig& config) {
    return src.insert_single_output_opr<AdaptivePoolingBackward>(
            src.node(), out_shape.node(), dst.node(), diff.node(), param, config);
}

void AdaptivePoolingBackward::scn_do_execute() {
    megdnn_opr()->exec(
            input(0)->dev_tensor().as_megdnn(), input(2)->dev_tensor().as_megdnn(),
            input(3)->dev_tensor().as_megdnn(), output(0)->dev_tensor().as_megdnn(),
            intl::get_megdnn_workspace_from_var(output().back()));
}
size_t AdaptivePoolingBackward::get_workspace_size_bytes(
        const TensorShapeArray& input_shapes,
        const TensorShapeArray& output_shapes) const {
    return megdnn_opr()->get_workspace_in_bytes(
            {input_shapes[0], input(0)->dtype(), input(0)->format()},
            {input_shapes[2], input(2)->dtype(), input(2)->format()},
            {input_shapes[3], input(3)->dtype(), input(3)->format()},
            {output_shapes[0], output(0)->dtype(), output(0)->format()});
}

// vim: syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
