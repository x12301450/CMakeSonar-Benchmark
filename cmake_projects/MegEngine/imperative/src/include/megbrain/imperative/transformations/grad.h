#pragma once

#include <variant>

#include "megbrain/imperative/backward_graph_opt.h"
#include "megbrain/imperative/dispatch.h"
#include "megbrain/imperative/interpreter.h"
#include "megbrain/imperative/opr_utility.h"
#include "megbrain/imperative/ops/autogen.h"
#include "megbrain/imperative/utils/helper.h"
#include "megbrain/imperative/utils/intrusive_list.h"
#include "megbrain/imperative/utils/to_string.h"

namespace mgb::imperative {

struct BackwardGraphWithClosure {
    std::shared_ptr<OptimizedBackwardGraphResult> backward_graph;
    SmallVector<ValueRef> closure;
    size_t output_mask_offset;
    size_t grad_mask_offset;
    std::shared_ptr<OpDef> op;

    struct OutputDesc {
        ValueRef shape;
        DTypeValue::ref_t dtype;
        CompNodeValue::ref_t device;
    };

    SmallVector<OutputDesc> output_descs;

    BackwardGraphWithClosure(
            std::shared_ptr<OptimizedBackwardGraphResult> backward_graph,
            std::shared_ptr<OpDef> op, Span<ValueRef> inputs, Span<ValueRef> outputs);

    void operator()(
            Span<ValueRef> grads, std::function<void(size_t, ValueRef)> receiver);

    bool input_has_grad(size_t i) { return backward_graph->input_has_grad[i]; }

    bool output_requires_grad(size_t i) {
        return backward_graph->save_for_backward[grad_mask_offset + i];
    }
    bool output_captured(size_t i) {
        return backward_graph->save_for_backward[output_mask_offset + i];
    }
};

struct CustomBackward;

using GradRuleFn = std::function<ValueRefList(Span<ValueRef> inputs, CustomBackward&)>;

struct CustomBackward {
    using BackwardFn = std::function<SmallVector<ValueRef>(Span<ValueRef>)>;
    using BackwardRule = std::function<std::optional<ValueRefList>(
            const OpDef&, Span<ValueRef>, Span<bool>, CustomBackward&)>;
    BackwardFn m_backward;
    SmallVector<bool, 8> m_input_has_grad;
    struct OutputAttr {
        bool requires_grad = true, captured = true;
    };
    SmallVector<OutputAttr> m_output_attrs;

public:
    void operator()(
            Span<ValueRef> grads, std::function<void(size_t, ValueRef)> receiver);

    bool input_has_grad(size_t i) { return m_input_has_grad[i]; }
    bool output_requires_grad(size_t i) { return m_output_attrs[i].requires_grad; }
    bool output_captured(size_t i) { return m_output_attrs[i].captured; }

    static bool register_grad_rule(Typeinfo* typeinfo, BackwardRule rule);
    static BackwardRule lookup_grad_rule(Typeinfo* typeinfo);
};

struct IdentityBackward {
    bool input_has_grad(size_t i) { mgb_assert(0); }
    bool output_requires_grad(size_t i) { mgb_assert(0); }

    template <typename F>
    void operator()(Span<ValueRef> grads, F&& receiver) {
        for (size_t i = 0; i < grads.size(); ++i) {
            if (grads[i]) {
                receiver(i, grads[i]);
            }
        }
    }
};

class GradSlot;
class GradSlotPtr;
class GradSlotProducerPtr;
class GradFn;
class GradKey;

struct GradProducerRecord : utils::intrusive_list::Node<GradProducerRecord> {
    using Node = utils::intrusive_list::Node<GradProducerRecord>;

    GradProducerRecord() = default;
    GradProducerRecord(head_t& head) : Node(utils::intrusive_list::after_t{}, head) {}
};

class GradSlot {
private:
    ValueRef m_grad;
    GradProducerRecord::head_t m_producer_head;
    std::function<void(ValueRef)> callback;

public:
    std::string to_string() const;
    ValueRef grad() const { return m_grad; }
    friend class GradKey;
    friend class GradSlotProducerPtr;
    friend class GradTransformation;
};

template <>
struct ToStringTrait<GradSlot> {
    std::string operator()(const GradSlot& value) const { return value.to_string(); }
};

class GradSlotPtr {
private:
    LocalPtr<GradFn> m_fn;
    size_t m_index = 0;

public:
    GradSlotPtr(LocalPtr<GradFn> fn, size_t index) : m_fn(fn), m_index(index) {}
    GradSlotPtr() = default;
    GradSlot* operator->() const;

    operator bool() const { return bool(m_fn); }

    std::string to_string() const;

    friend class GradKey;
    friend class GradTransformation;
};

template <>
struct ToStringTrait<GradSlotPtr> {
    std::string operator()(const GradSlotPtr& value) const { return value.to_string(); }
};

class GradSlotProducerPtr : public GradSlotPtr {
private:
    GradProducerRecord m_producer_record;
    bool dirty = false;

public:
    GradSlotProducerPtr(const GradSlotPtr& info)
            : GradSlotPtr(info), m_producer_record(info->m_producer_head) {}
    GradSlotProducerPtr() = default;
    GradSlotProducerPtr(GradSlotProducerPtr&&) = default;
    ~GradSlotProducerPtr() { dirty = true; }
    friend class GradKey;
    friend class GradTransformation;
};

template <>
struct ToStringTrait<GradSlotProducerPtr> {
    std::string operator()(const GradSlotProducerPtr& value) const {
        return value.to_string();
    }
};

class GradFn {
private:
    std::weak_ptr<GradKey> m_key;
    SmallVector<GradSlot> m_slots;
    SmallVector<GradSlotProducerPtr> m_dests;
    std::variant<
            std::monostate, BackwardGraphWithClosure, CustomBackward, IdentityBackward>
            m_backward;

public:
    void clear() {
        m_key.reset();
        m_slots.clear();
        m_dests.clear();
        m_backward.emplace<std::monostate>();
    }

    std::string to_string() const;

    friend class GradSlotPtr;
    friend class GradKey;
    friend class GradTransformation;
};

inline GradSlot* GradSlotPtr::operator->() const {
    return &m_fn->m_slots[m_index];
}

class GradValue final : public ObjectValue<GradValue> {
private:
    ValueRef m_value;
    std::shared_ptr<GradKey> m_key;
    GradSlotPtr m_slot;

public:
    GradValue(ValueRef value, std::shared_ptr<GradKey> key, GradSlotPtr slot = {})
            : m_value(std::move(value)), m_key(std::move(key)), m_slot(slot) {}

    std::string to_string() const override;

    const GradSlotPtr& slot() const { return m_slot; }

    // std::shared_ptr<GradKey> key() const { return m_key; }

    void clear() override {
        m_slot = {};
        m_value = {};
        m_key = nullptr;
    }

    void on_watch() override { m_value.watch(); }

    void on_unwatch() override { m_value.unwatch(); }

    friend class GradKey;
    friend class GradTransformation;
};

class GradKey : public std::enable_shared_from_this<GradKey> {
private:
    std::string m_name;
    std::vector<LocalPtr<GradFn>> m_side_effects;
    std::vector<std::pair<LocalWeakPtr<GradFn>, std::shared_ptr<OpDef>>> m_tape;
    std::vector<std::pair<LocalPtr<GradFn>, std::shared_ptr<OpDef>>> m_frozen_tape;
    bool m_frozen = false;
    const Type<GradValue>& m_value_type;

public:
    GradKey(const Type<GradValue>& value_type) : m_value_type(value_type) {
        m_tape.reserve(4 * 1024);
    }

    void backward();
    GradValue::ref_t attach(ValueRef tensor, std::function<void(ValueRef)> callback);
    const std::string& name() const { return m_name; }
    void name(std::string name) { m_name = std::move(name); }
    void freeze();

    friend class GradTransformation;
};

class GradKeyValue final
        : public PrimitiveValue<GradKeyValue, std::shared_ptr<GradKey>> {
public:
    using PrimitiveValue::PrimitiveValue;

    std::string to_string() const override {
        return ssprintf("GradKey{%s}", (*this)->name().c_str());
    }
};

class GradSlotValue final : public PrimitiveValue<GradSlotValue, GradSlotPtr> {
public:
    using PrimitiveValue::PrimitiveValue;

    std::string to_string() const override { return ssprintf("GradSlot{}"); }
};

class GradTransformation final : public Transformation {
private:
    ObjectType<GradValue> m_value_type{"GradValue"};
    std::shared_ptr<GradKey> m_key;
    std::vector<GradValue::weak_ref_t> m_weak_values;
    size_t m_suppressed = 0;

public:
    GradTransformation() { m_key = std::make_shared<GradKey>(m_value_type); }

    auto record_grad(GradValue::ref_t tensor) {
        m_weak_values.push_back(tensor);
        return tensor;
    }

    bool is_grad_value(const ValueRef& value) { return value.is(m_value_type); }

    /**
     * \brief test whether value is related to this GradTransformation
     *
     * there may be multiple grad transformations, so simply using value.is<GradValue>()
     * is unsafe
     *
     * \param value
     * \return GradValue::ref_t
     */
    const GradValue::ref_t& as_grad_value(const ValueRef& value) {
        return value.as_ref(m_value_type);
    }

    bool has_key(std::shared_ptr<GradKey> key) {
        if (key == m_key) {
            return true;
        }
        return false;
    }

    ValueRefList apply_transformation(
            const Operator& op, Span<ValueRef> inputs) override;

    ValueRef unwrap(ValueRef value) override {
        if (auto grad_val = as_grad_value(value)) {
            return grad_val->m_value;
        }
        return value;
    }

    const std::shared_ptr<GradKey>& key() const { return m_key; }

    std::string name() const override { return "GradTransformation"; }

    GenericFunction make_backward_closure(Span<ValueRef> ys);

    void on_unregister() noexcept override;

    void cleanup();
    void suppress();
    void resume();
};

class DetachGrad : public OperatorImpl<DetachGrad, Operator::IdentityLike> {
private:
    // TODO: identified by GradKey
public:
    std::string to_string() const override { return "DetachValue"; }
    std::string raw_type() const override { return "DetachGrad"; }
    ValueRefList fallback(Span<ValueRef> inputs) const override {
        return {inputs.as_array<1>()[0]};
    }
};

class AttachGrad : public OperatorImpl<AttachGrad> {
private:
    std::shared_ptr<GradKey> m_key;

public:
    AttachGrad(std::shared_ptr<GradKey> key) : m_key(key) {}
    std::shared_ptr<GradKey> key() const { return m_key; }

    std::string to_string() const override {
        return ssprintf("AttachGradValue{key=%s}", m_key->name().c_str());
    }

    std::string raw_type() const { return "AttachGrad"; }
};

class GradBackward : public OperatorImpl<GradBackward, Operator::GetAttrLike> {
private:
    std::shared_ptr<GradKey> m_key;

public:
    GradBackward(std::shared_ptr<GradKey> key) : m_key(key) {}

    std::shared_ptr<GradKey> key() const { return m_key; }

    std::string to_string() const override {
        return ssprintf("GradBackwardValue{key=%s}", m_key->name().c_str());
    }

    std::string raw_type() const { return "GradBackward"; }
};

class IsAttachedTo : public OperatorImpl<IsAttachedTo, Operator::GetAttrLike> {
private:
    std::shared_ptr<GradKey> m_key;

public:
    IsAttachedTo(std::shared_ptr<GradKey> key) : m_key(key) {}
    std::shared_ptr<GradKey> key() const { return m_key; }

    std::string to_string() const override {
        return ssprintf("IsAttachedToValue{key=%s}", m_key->name().c_str());
    }

    std::string raw_type() const { return "IsAttachedTo"; }

    ValueRefList fallback(Span<ValueRef> inputs) const override {
        return {BoolValue::make(false)};
    }
};

class SetGrad : public OperatorImpl<SetGrad> {
private:
    GenericFunction m_grad_fn;
    size_t m_nr_inputs;

public:
    SetGrad(GenericFunction grad_fn, size_t nr_inputs)
            : m_grad_fn(grad_fn), m_nr_inputs(nr_inputs) {}

    GenericFunction grad_fn() const { return m_grad_fn; }

    size_t nr_inputs() const { return m_nr_inputs; }

    std::string to_string() const override { return ssprintf("SetGradValue{}"); }
    std::string raw_type() const { return "SetGrad"; }
    ValueRefList fallback(Span<ValueRef> inputs) const override {
        auto outputs = inputs.sub(m_nr_inputs, inputs.size() - m_nr_inputs);
        return {outputs.begin(), outputs.end()};
    }
};

class GetGradKey : public OperatorImpl<GetGradKey, Operator::GetAttrLike> {
public:
    GetGradKey() = default;

    std::string to_string() const override { return ssprintf("GetGradKeyValue{}"); }
    std::string raw_type() const { return "GetGradKey"; };
    ValueRefList fallback(Span<ValueRef> inputs) const override { return {ValueRef()}; }
};

class GetGradSlot : public OperatorImpl<GetGradSlot, Operator::GetAttrLike> {
public:
    GetGradSlot() = default;

    std::string to_string() const override { return ssprintf("GetGradSlot{}"); }
    std::string raw_type() const { return "GetGradSlot"; };
    ValueRefList fallback(Span<ValueRef> inputs) const override { return {}; }
};

class InsertGradCallback : public OperatorImpl<InsertGradCallback, Operator::Other> {
public:
    GenericFunction m_callback;

public:
    InsertGradCallback(GenericFunction callback) : m_callback(callback) {}

    GenericFunction callback() const { return m_callback; }

    std::string to_string() const override { return ssprintf("InsertGradCallback{}"); }
    std::string raw_type() const { return "InsertGradCallback"; }
};

class GetBackwardColsure
        : public OperatorImpl<GetBackwardColsure, Operator::GetAttrLike> {
private:
    std::shared_ptr<GradKey> m_key;

public:
    GetBackwardColsure(std::shared_ptr<GradKey> key) : m_key(key) {}

    std::shared_ptr<GradKey> key() const { return m_key; }

    std::string to_string() const override {
        return ssprintf("GetBackwardClosure{key=%s}", m_key->name().c_str());
    }
    std::string raw_type() const { return "GetBackwardClosure"; }
};

class GradTransformationGuard final : public Transformation {
    ValueRefList apply_transformation(
            const Operator& op, Span<ValueRef> inputs) override {
        if (auto* igc = op.as<InsertGradCallback>()) {
            auto count = IntegerValue::make(0);
            return {count};
        }
        return imperative::apply(op, inputs);
    }

    ValueRef unwrap(ValueRef value) override { return value; };

    std::string name() const override { return "GradTransformationGuard"; };
};

}  // namespace mgb::imperative
