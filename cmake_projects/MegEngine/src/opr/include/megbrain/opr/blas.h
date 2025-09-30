#pragma once

#include "megbrain/exception.h"
#include "megbrain/graph.h"
#include "megbrain/opr/internal/megdnn_opr_wrapper.h"
#include "megbrain/opr/search_policy/algo_chooser_helper.h"
#include "megbrain/tensor.h"

#include "megdnn/oprs/general.h"
#include "megdnn/oprs/linalg.h"

namespace mgb {
namespace opr {

/*!
 * \brief matrix_mul(trans0(opr0), trans1(opr1))
 */
MGB_DEFINE_OPR_CLASS_WITH_EXPORT(
        MatrixMul, intl::MegDNNOprWrapperFwd<megdnn::MatrixMul>,
        public mixin::AlgoChooserHelper) // {
public:
    using AlgorithmInfo = megdnn::detail::Algorithm::Info;
    MGE_WIN_DECLSPEC_FUC MatrixMul(
            VarNode* opr0, VarNode* opr1, const Param& param,
            const ExecutionPolicy& policy, const OperatorNodeConfig& config);

    MGE_WIN_DECLSPEC_FUC static SymbolVar make(
            SymbolVar opr0, SymbolVar opr1, const Param& param = {},
            const ExecutionPolicy& policy = {}, const OperatorNodeConfig& config = {});

private:
    void add_input_layout_constraint() override;
    void scn_do_execute() override;
    void init_output_dtype() override;
    NodeProp* do_make_node_prop() const override;
    size_t get_workspace_size_bytes(
            const TensorShapeArray& input_shapes,
            const TensorShapeArray& output_shapes) const override;
    static bool check_layout(const TensorLayout& layout, int transpose);

    //! store the policy of all transpose situations
    megdnn::ExecutionPolicy m_cadidate_execution_policies[4];
    std::unique_ptr<megdnn::Fill> m_fill_opr;
};

/*!
 * \brief batched matrix multiplication on 3D inputs
 */
MGB_DEFINE_OPR_CLASS_WITH_EXPORT(
        BatchedMatrixMul, intl::MegDNNOprWrapperFwd<megdnn::BatchedMatrixMul>,
        public mixin::AlgoChooserHelper) // {
public:
    using AlgorithmInfo = megdnn::detail::Algorithm::Info;
    MGE_WIN_DECLSPEC_FUC BatchedMatrixMul(
            VarNode* opr0, VarNode* opr1, const Param& param,
            const ExecutionPolicy& policy, const OperatorNodeConfig& config);

    MGE_WIN_DECLSPEC_FUC static SymbolVar make(
            SymbolVar opr0, SymbolVar opr1, const Param& param = {},
            const ExecutionPolicy& policy = {}, const OperatorNodeConfig& config = {});

private:
    void add_input_layout_constraint() override;
    void init_output_dtype() override;
    void scn_do_execute() override;
    NodeProp* do_make_node_prop() const override;
    size_t get_workspace_size_bytes(
            const TensorShapeArray& input_shapes,
            const TensorShapeArray& output_shapes) const override;

    static bool check_layout(const TensorLayout& layout, bool transpose);
    //! store the policy of all transpose situations
    megdnn::ExecutionPolicy m_cadidate_execution_policies[4];
    std::unique_ptr<megdnn::Fill> m_fill_opr;
};

/*!
 * \brief dot product of two tensors
 */
MGB_DEFINE_OPR_CLASS_WITH_EXPORT(
        Dot, cg::SingleCNOperatorNodeBaseT<mixin::MegDNNOprHolderImpl<megdnn::Dot>>) // {
public:
    MGE_WIN_DECLSPEC_FUC Dot(
            VarNode* opr0, VarNode* opr1, const OperatorNodeConfig& config);

    MGE_WIN_DECLSPEC_FUC static SymbolVar make(
            SymbolVar opr0, SymbolVar opr1, const OperatorNodeConfig& config = {});

    // for serialization
    static SymbolVar make(
            SymbolVar opr0, SymbolVar opr1, Param, const OperatorNodeConfig& config) {
        return make(opr0, opr1, config);
    }

private:
    void add_input_layout_constraint() override;
    void scn_do_execute() override;
    void init_output_static_infer_desc() override;
    NodeProp* do_make_node_prop() const override;
    void record_execute_deps(ExecDependencyArray& deps) override;
    std::unique_ptr<megdnn::Fill> m_fill_opr;
};

MGB_DEFINE_OPR_CLASS(
        MatrixInverse, intl::MegDNNOprWrapperFwd<megdnn::MatrixInverse>) // {
public:
    MGE_WIN_DECLSPEC_FUC MatrixInverse(
            VarNode* p0, const Param& param, const OperatorNodeConfig& config);
    MGE_WIN_DECLSPEC_FUC static SymbolVar make(
            SymbolVar p0, const Param& param = {},
            const OperatorNodeConfig& config = {});
    void scn_do_execute() override;
    NodeProp* do_make_node_prop() const override;
};

MGB_DEFINE_OPR_CLASS(SVD, intl::MegDNNOprWrapperFwd<megdnn::SVD>) // {
public:
    MGE_WIN_DECLSPEC_FUC SVD(
            VarNode* src, const Param& param, const OperatorNodeConfig& config);
    MGE_WIN_DECLSPEC_FUC static SymbolVarArray make(
            const SymbolVar& src, const Param& param = {},
            const OperatorNodeConfig& config = {});
};

MGB_DEFINE_OPR_CLASS(Cross, intl::MegDNNOprWrapperFwd<megdnn::Cross>) // {
public:
    MGE_WIN_DECLSPEC_FUC Cross(
            VarNode* A, VarNode* B, const Param& param,
            const OperatorNodeConfig& config);
    MGE_WIN_DECLSPEC_FUC static SymbolVar make(
            SymbolVar A, SymbolVar B, const Param& param,
            const OperatorNodeConfig& config = {});

private:
    void add_input_layout_constraint() override;
};

}  // namespace opr
}  // namespace mgb

// vim: ft=cpp syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
