#pragma once

#include "megbrain/opr/internal/megdnn_opr_wrapper.h"
#include "megbrain/opr/internal/out_shape_by_sym_var.h"

#include "megdnn/oprs.h"

namespace mgb {
namespace opr {

namespace intl {
using DeformablePSROIPoolingBackwardBase = cg::SingleCNOperatorNode<
        cg::OutshapePureByInshapeOpr<>,
        mixin::MegDNNOprHolderImpl<megdnn::DeformablePSROIPoolingBackward>>;

using DeformablePSROIPoolingBackwardT =
        WorkspaceSizeInfer<DeformablePSROIPoolingBackwardBase>;

}  // namespace intl

/*!
 * \brief ROI Pooling.
 *
 * The internal implementation is akin to
 * https://github.com/rbgirshick/caffe-fast-rcnn .
 *
 * Note that the operator actual has two outputs; the second output is an index
 * used for backward
 */
MGB_DEFINE_OPR_CLASS(
        ROIPoolingForward,
        intl::WorkspaceSizeInfer<intl::OutshapeBySymvarSCNOpr<
                mixin::MegDNNOprHolderImpl<megdnn::ROIPoolingForward>>>) // {
public:
    MGE_WIN_DECLSPEC_FUC ROIPoolingForward(
            VarNode* src, VarNode* rois, VarNode* dst_shape, const Param& param,
            const OperatorNodeConfig& config);

    MGE_WIN_DECLSPEC_FUC static SymbolVar make(
            SymbolVar src, SymbolVar rois, SymbolVar dst_shape, const Param& param = {},
            const OperatorNodeConfig& config = {});

    static SymbolVar make(
            SymbolVar src, SymbolVar rois, const TensorShape& dst_shape,
            const Param& param = {}, const OperatorNodeConfig& config = {}) {
        return make(
                src, rois, cg::var_from_tensor_shape(src, dst_shape), param, config);
    }

private:
    void scn_do_execute() override;

    void outshape_by_symvar_do_get_output_shape(
            TensorShape& dest, const ShapeInferInfo& shpinfo) override;

    void init_output_static_infer_desc() override;
    void add_input_layout_constraint() override;
    size_t get_workspace_size_bytes(
            const TensorShapeArray& input_shapes,
            const TensorShapeArray& output_shapes) const override;
    void record_execute_deps(ExecDependencyArray& deps) override;
};
using ROIPooling = ROIPoolingForward;

MGB_DEFINE_OPR_CLASS(
        ROIPoolingBackward, intl::MegDNNOprWrapperBwd<megdnn::ROIPoolingBackward>) // {
public:
    MGE_WIN_DECLSPEC_FUC ROIPoolingBackward(
            VarNode* diff, VarNode* src, VarNode* rois, VarNode* index,
            const Param& param, const OperatorNodeConfig& config);

    MGE_WIN_DECLSPEC_FUC static SymbolVar make(
            SymbolVar diff, SymbolVar src, SymbolVar rois, SymbolVar index,
            const Param& param = {}, const OperatorNodeConfig& config = {});
};

/*!
 * \brief Deformable PS ROI Pooling.
 *
 * PS ROI Pooling with a bbox deformation.
 */
MGB_DEFINE_OPR_CLASS(
        DeformablePSROIPoolingForward,
        intl::MegDNNOprWrapperFwd<megdnn::DeformablePSROIPoolingForward>) // {
public:
    MGE_WIN_DECLSPEC_FUC DeformablePSROIPoolingForward(
            VarNode* src, VarNode* rois, VarNode* trans, const Param& param,
            const OperatorNodeConfig& config);

    MGE_WIN_DECLSPEC_FUC static SymbolVarArray make_all(
            SymbolVar src, SymbolVar rois, SymbolVar trans, const Param& param = {},
            const OperatorNodeConfig& config = {});
    MGE_WIN_DECLSPEC_FUC static SymbolVar make(
            SymbolVar src, SymbolVar rois, SymbolVar trans, const Param& param = {},
            const OperatorNodeConfig& config = {});
};
using DeformablePSROIPooling = DeformablePSROIPoolingForward;

MGB_DEFINE_OPR_CLASS(
        DeformablePSROIPoolingBackward, intl::DeformablePSROIPoolingBackwardT) // {
public:
    MGE_WIN_DECLSPEC_FUC DeformablePSROIPoolingBackward(
            VarNode* src, VarNode* rois, VarNode* trans, VarNode* grad, VarNode* count,
            const Param& param, const OperatorNodeConfig& config);
    MGE_WIN_DECLSPEC_FUC static SymbolVarArray make_all(
            SymbolVar src, SymbolVar rois, SymbolVar trans, SymbolVar grad,
            SymbolVar count, const Param& param = {},
            const OperatorNodeConfig& config = {});
    MGE_WIN_DECLSPEC_FUC static SymbolVar make(
            SymbolVar src, SymbolVar rois, SymbolVar trans, SymbolVar grad,
            SymbolVar count, const Param& param = {},
            const OperatorNodeConfig& config = {});
    MGE_WIN_DECLSPEC_FUC void scn_do_execute() override;

private:
    void get_output_var_shape(
            const TensorShapeArray& inp_shape,
            TensorShapeArray& out_shape) const override;
    size_t get_workspace_size_bytes(
            const TensorShapeArray&, const TensorShapeArray&) const override;
    void init_output_static_infer_desc() override;
    void init_output_dtype() override;
    void init_output_format() override;

    NodeProp* do_make_node_prop() const override;

    void add_input_layout_constraint() override {
        mixin::megdnn_utils::add_input_layout_constraint_contig(*this);
    }
};

}  // namespace opr
}  // namespace mgb

// vim: syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
