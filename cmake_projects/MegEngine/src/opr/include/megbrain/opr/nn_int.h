#pragma once

#include "megbrain/opr/internal/megdnn_opr_wrapper.h"
#include "megbrain/opr/internal/out_shape_by_sym_var.h"

#include "megdnn/oprs/nn_int.h"

namespace mgb {
namespace opr {

namespace intl {
using ElemwiseMultiTypeBase = cg::SingleCNOperatorNode<
        cg::OutshapePureByInshapeOpr<>,
        mixin::MegDNNOprHolderImpl<megdnn::ElemwiseMultiType, false>>;
}

MGB_DEFINE_OPR_CLASS(ElemwiseMultiType, intl::ElemwiseMultiTypeBase) // {
public:
    using Mode = Param::Mode;

    MGE_WIN_DECLSPEC_FUC ElemwiseMultiType(
            const VarNodeArrayView& inputs, Param param,
            const OperatorNodeConfig& config);

    MGE_WIN_DECLSPEC_FUC static SymbolVar make(
            const VarNodeArrayView& inputs, Param param,
            const OperatorNodeConfig& config = {});

    MGE_WIN_DECLSPEC_FUC static TensorShape get_output_var_shape(
            Mode mode, const TensorShapeArray& input_shapes);

    MGE_WIN_DECLSPEC_FUC static void perform(
            Mode mode, DType out_dt, DeviceTensorND& dest,
            const SmallVector<DeviceTensorND>& inputs,
            intl::UniqPtrWithCN<megdnn::ElemwiseMultiType>& opr);

private:
    using ModeTrait = megdnn::ElemwiseMultiType::ModeTrait;

    void scn_do_execute() override;

    void get_output_var_shape(
            const TensorShapeArray& inp_shape,
            TensorShapeArray& out_shape) const override;

    void init_output_dtype() override;

    void record_execute_deps(ExecDependencyArray& deps) override;

    void add_input_layout_constraint() override;

    NodeProp* do_make_node_prop() const override;

    void init_output_static_infer_desc() override;

    static void call_megdnn_opr_exec(
            CompNode comp_node, megdnn::TensorNDArray& inp, const megdnn::TensorND& out,
            megdnn::ElemwiseMultiType* opr, ElemwiseMultiType* caller);
};

//! deprecated; TODO: remove in megbrain 8
class AffineInt final : public DynTypeObj {
    MGB_DYN_TYPE_OBJ_FINAL_DECL;

public:
    using Param = megdnn::param::Empty;
    MGE_WIN_DECLSPEC_FUC static SymbolVar make(
            SymbolVar x, SymbolVar k, SymbolVar b, const Param& param = {},
            const OperatorNodeConfig& config = {}) {
        return ElemwiseMultiType::make(
                {x, k, b}, {ElemwiseMultiType::Mode::FUSE_MUL_ADD3_IXxF32xF32xI8},
                config);
    }

    static Param param() {
        mgb_trap();
        return {};
    }
};

}  // namespace opr
}  // namespace mgb

// vim: syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
