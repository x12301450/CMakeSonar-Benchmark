#pragma once

#include "megbrain/graph.h"
#include "megbrain/graph/event.h"
#include "megbrain/plugin/base.h"
#include "megbrain/utils/thin/hash_table.h"

namespace mgb {
class NumRangeCheckerError final : public MegBrainError {
public:
    using MegBrainError::MegBrainError;
};
}  // namespace mgb

namespace mgb {

/*!
 * \brief check that the absolute values of all numbers in a computing graph
 *      do not exceed some threshold
 */
class NumRangeChecker final : public PluginBase {
    class Checker {
        std::shared_ptr<DeviceTensorND> m_inp;
        std::unique_ptr<HostTensorND> m_out;
        std::unique_ptr<cg::AsyncExecutable> m_func;

    public:
        void init(VarNode* var, float range);
        bool check(VarNode* var);
    };

    const float m_range;
    CompNode::UnorderedMap<ThinHashMap<megdnn::DTypeEnum, Checker>> m_cn2dt2checker;
    std::vector<std::unique_ptr<NumRangeChecker>> m_sub_graph_checkers;

    void on_kern_end(const cg::event::OprExecKernelEnd& event);
    void on_subgraph_associated(const cg::event::SubgraphAssociated& event);

    void on_var_computed(VarNode* var);

    template <typename ctype>
    std::string format_msg(const HostTensorND& hv, float range);

public:
    using Error = NumRangeCheckerError;
    MGE_WIN_DECLSPEC_FUC NumRangeChecker(cg::ComputingGraph* graph, float range);
};
}  // namespace mgb

// vim: syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
