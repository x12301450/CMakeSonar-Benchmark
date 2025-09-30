#pragma once

#include "megbrain/graph.h"
#include "megbrain/imperative/physical_tensor.h"
#include "megbrain/imperative/subgraph.h"
#include "megbrain/imperative/utils/to_string.h"

namespace mgb {
namespace imperative {

class OpDef;
struct OpTrait;

enum DispatchMode { DEFAULT_CPU = 0, KERNEL = 1 };

using SharedOp = std::shared_ptr<OpDef>;

class OpDef : public Hashable,
              public NonCopyableObj,
              public std::enable_shared_from_this<OpDef> {
    mutable const OpTrait* m_trait = nullptr;
    std::string m_scope;
    std::string m_py_traceback;

public:
    using allocator_t =
            std::function<DeviceTensorStorage::RawStorage(CompNode, size_t)>;
    virtual ~OpDef() = default;

    static std::shared_ptr<OpDef> make_from_op_node(cg::OperatorNodeBase* node);

    /*!
     * \brief Decide which dispatch method to be used according to the inputs'
     * host value and size.
     *
     * \param def Specific :c:expr:`OpDef` to be executed.
     * \param inputs Input tensor descriptions.
     * \return Which DispatchMode to be used, such as `CUDA` or `DEFAULT_CPU`.
     */
    static DispatchMode decide_dispatch_mode(
            const OpDef& def, const SmallVector<LogicalTensorDesc>& inputs);

    static SmallVector<TensorPtr> apply_on_physical_tensor(
            const OpDef& def, const SmallVector<TensorPtr>& inputs,
            SmallVector<LogicalTensorDesc>& output_descs, const bool& validated);

    /*!
     * \brief Call the corresponding dnn op to calculate results. Output
     * tensors' device memory should be allocated outside.
     */
    static void apply_on_device_tensornd(
            const OpDef& def, const SmallVector<DeviceTensorND>& inputs,
            SmallVector<DeviceTensorND>* outputs);

    static cg::VarNodeArray apply_on_var_node(
            const OpDef& def, const VarNodeArray& inputs);

    static std::tuple<SmallVector<LogicalTensorDesc>, bool> infer_output_attrs_fallible(
            const OpDef& def, const SmallVector<LogicalTensorDesc>& inputs);

    static EncodedSubgraph make_backward_graph(
            const OpDef& def, const SmallVector<LogicalTensorDesc>& inputs,
            const SmallVector<bool>& input_requires_grad,
            const SmallVector<bool>& output_has_grad);

    static std::vector<std::pair<const char*, std::string>> props(const OpDef& def);

    static EncodedSubgraph make_forward_graph(
            const OpDef& def, const SmallVector<LogicalTensorDesc>& inputs);

    static SmallVector<VarNode::LayoutConstraintCallback> get_input_layout_constraint(
            const OpDef& def, const SmallVector<TensorPtr>& inputs);

    const OpTrait* trait() const;

    std::string to_string() const;

    std::string name() const;

    const std::string scope() const;

    const std::string make_name() const;

    virtual const std::string type_name() const;

    const std::string py_traceback() const;

    void set_scope(const std::string& scope);
    void set_py_traceback(const std::string& traceback);

    virtual size_t hash() const;

    virtual bool is_same_st(const Hashable&) const;

    static void set_allocator(allocator_t allocator);
    DeviceTensorStorage::RawStorage allocate(CompNode, size_t) const;

    std::shared_ptr<OpDef> shared_from_this() const {
        return const_cast<OpDef&>(*this)
                .std::enable_shared_from_this<OpDef>::shared_from_this();
    }
};

template <typename T>
class OpDefImplBase : public OpDef {
public:
    template <typename... Args>
    static std::shared_ptr<T> make(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

template <>
struct ToStringTrait<OpDef*> {
    std::string operator()(OpDef* op) const {
        if (op == nullptr) {
            return "nullptr";
        }
        return op->to_string();
    }
};

}  // namespace imperative
}  // namespace mgb

// vim: syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
