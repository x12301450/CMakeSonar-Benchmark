#pragma once
#include "megbrain/opr/internal/megdnn_opr_wrapper.h"
#include "megdnn/oprs.h"
namespace mgb {
namespace opr {

MGB_DEFINE_OPR_CLASS(TQTForward, intl::MegDNNOprWrapperFwd<megdnn::TQTForward>) // {
public:
    MGE_WIN_DECLSPEC_FUC TQTForward(
            VarNode* src, VarNode* scale, const Param& param,
            const OperatorNodeConfig& config);

    MGE_WIN_DECLSPEC_FUC static SymbolVar make(
            SymbolVar src, SymbolVar scale, const Param& param = {},
            const OperatorNodeConfig& config = {});
};
using TQT = TQTForward;

MGB_DEFINE_OPR_CLASS(
        TQTBackward, intl::MegDNNOprWrapperBwd<megdnn::TQTBackward>) // {
public:
    MGE_WIN_DECLSPEC_FUC TQTBackward(
            VarNode* y_grad, VarNode* x, VarNode* scale, const Param& param,
            const OperatorNodeConfig& config);

    MGE_WIN_DECLSPEC_FUC static SymbolVarArray make(
            SymbolVar y_grad, SymbolVar x, SymbolVar scale, const Param& param = {},
            const OperatorNodeConfig& config = {});

private:
    void init_output_static_infer_desc() override;
    void init_output_dtype() override;
};

}  // namespace opr
}  // namespace mgb
