#pragma once

#include "megbrain/opr/internal/megdnn_opr_wrapper.h"
#include "megdnn/oprs.h"

namespace mgb {
namespace opr {

MGB_DEFINE_OPR_CLASS(
        ROIAlignForward, intl::MegDNNOprWrapperFwd<megdnn::ROIAlignForward>) // {
public:
    MGE_WIN_DECLSPEC_FUC ROIAlignForward(
            VarNode* src, VarNode* rois, const Param& param,
            const OperatorNodeConfig& config);

    MGE_WIN_DECLSPEC_FUC static SymbolVar make(
            SymbolVar src, SymbolVar rois, const Param& param = {},
            const OperatorNodeConfig& config = {});

private:
    void scn_do_execute() override;
    NodeProp* do_make_node_prop() const override;
    size_t get_workspace_size_bytes(
            const TensorShapeArray& input_shapes,
            const TensorShapeArray& output_shapes) const override;
};
using ROIAlign = ROIAlignForward;

MGB_DEFINE_OPR_CLASS(
        ROIAlignBackward, intl::MegDNNOprWrapperBwd<megdnn::ROIAlignBackward>) // {
public:
    MGE_WIN_DECLSPEC_FUC ROIAlignBackward(
            VarNode* diff, VarNode* src, VarNode* rois, VarNode* index,
            const Param& param, const OperatorNodeConfig& config);

    MGE_WIN_DECLSPEC_FUC static SymbolVar make(
            SymbolVar diff, SymbolVar src, SymbolVar rois, SymbolVar index,
            const Param& param = {}, const OperatorNodeConfig& config = {});

private:
    void scn_do_execute() override;
    size_t get_workspace_size_bytes(
            const TensorShapeArray& input_shapes,
            const TensorShapeArray& output_shapes) const override;
};

}  // namespace opr
}  // namespace mgb

// vim: syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
