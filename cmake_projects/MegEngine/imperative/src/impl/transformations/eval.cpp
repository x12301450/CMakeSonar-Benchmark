#include "megbrain/imperative/transformations/eval.h"

namespace mgb {
namespace imperative {

DTypeValue::ref_t InterpreterValue::dtype() const {
    if (!m_dtype) {
        m_dtype = DTypeValue::make(handle()->channel()->get_dtype(handle()->handle()));
    }
    return m_dtype;
}

CompNodeValue::ref_t InterpreterValue::comp_node() const {
    if (!m_comp_node) {
        m_comp_node = CompNodeValue::make(
                handle()->channel()->get_device(handle()->handle()));
    }
    return m_comp_node;
}

ShapeValue::ref_t InterpreterValue::shape() const {
    if (!m_shape) {
        m_shape = ShapeValue::make(
                ValueShape::from(handle()->channel()->get_shape(handle()->handle())));
    }
    return m_shape;
}

ValueRefList InterpreterTransformation::apply_op(
        const ApplyOp& apply_op, Span<ValueRef> inputs) {
    SmallVector<Handle> input_handles;
    SmallVector<Handle> output_handles;
    CleanupGuard _{[&] {
        for (auto handle : output_handles) {
            if (handle) {
                m_channel->del(handle);
            }
        }
    }};
    for (auto input : inputs) {
        input_handles.push_back(input.cast(m_value_type).handle()->handle());
    }
    m_channel->set_backtrace(Transformation::get_context().bt);
    output_handles =
            m_channel->apply_op(apply_op.op().shared_from_this(), input_handles);
    ValueRefList outputs(output_handles.size());
    for (size_t i = 0; i < output_handles.size(); ++i) {
        outputs[i] = m_value_type.make(share_handle(output_handles[i]));
        output_handles[i] = nullptr;
    }
    output_handles.clear();
    m_channel->clear_backtrace();
    return outputs;
}

ValueRefList InterpreterTransformation::apply_get_attr(
        const GetAttr& get_attr, Span<ValueRef> inputs) {
    auto& input = inputs.item().cast(m_value_type);
    ValueRef output;
    m_channel->set_backtrace(Transformation::get_context().bt);
    switch (get_attr.attr()) {
        case GetAttr::DType:
            output = input.dtype();
            break;
        case GetAttr::Shape:
            output = input.shape();
            break;
        case GetAttr::Device:
            output = input.comp_node();
            break;
        case GetAttr::Value:
            output = HostValue::make(m_channel->get_value(input.handle()->handle()));
            break;
        case GetAttr::Data:
            output = DeviceValue::make(
                    m_channel->get_dev_tensor(input.handle()->handle()));
            break;
        default:
            mgb_throw(
                    MegBrainError, "Interpreter: malformed GetAttr: %s",
                    get_attr.to_string().c_str());
    }
    m_channel->clear_backtrace();
    return {output};
}

ValueRefList InterpreterTransformation::apply_create_tensor(
        const CreateTensor& create_tensor, Span<ValueRef> inputs) {
    auto args = create_tensor.parse(inputs);
    if (!args.device) {
        // implies H2D
        mgb_assert(args.host, "neither host and device value is valid");
        return {m_value_type.make(share_handle(
                m_channel->put(*args.host, args.kind == CreateTensor::Unique)))};
    } else {
        return {m_value_type.make(share_handle(m_channel->put(
                *args.device, args.host ? *args.host : HostTensorND())))};
    }
}

ValueRefList InterpreterTransformation::apply_transformation(
        const Operator& op, Span<ValueRef> inputs) {
    if (auto* op_val = op.as<ApplyOp>()) {
        if (op_val->op().same_type<FastpathCopy>()) {
            return inputs[0];
        } else {
            return apply_op(*op_val, inputs);
        }
    } else if (auto* get_attr = op.as<GetAttr>()) {
        return apply_get_attr(*get_attr, inputs);
    } else if (auto* create_tensor = op.as<CreateTensor>()) {
        return apply_create_tensor(*create_tensor, inputs);
    } else if (auto* dtr_command = op.as<DTRCommand>()) {
        auto handle = inputs[0].cast(m_value_type).handle()->handle();
        switch (dtr_command->kind()) {
            case DTRCommand::Drop:
                m_channel->drop(handle);
                break;
            default:
                mgb_throw(AssertionError, "unknown DTRCommand %d", dtr_command->kind());
        }
        return {};
    } else if (auto* rename_value = op.as<RenameValue>()) {
        auto& input = inputs[0].cast(m_value_type);
        return {m_value_type.make(input.handle(), rename_value->name())};
    } else if (op.is<GetName>()) {
        auto name = inputs[0].cast(m_value_type).name();
        if (!name.empty()) {
            return {StringValue::make(name)};
        } else {
            return {ValueRef()};
        }
    } else if (op.is<GetId>()) {
        auto& val = inputs[0].cast(m_value_type);
        int64_t id = val.id();
        return {IntegerValue::make(id)};

    } else if (op.is<DupTensor>()) {
        auto& input = inputs[0].cast(m_value_type);
        DeviceTensorND dev_tensor;
        dev_tensor.copy_from(m_channel->get_dev_tensor(input.handle()->handle()));
        return m_value_type.make(share_handle(m_channel->put(dev_tensor, {})));
    } else if (auto push_scope = op.as<PushScope>()) {
        m_channel->push_scope(push_scope->name, push_scope->type);
        return {};
    } else if (auto pop_scope = op.as<PopScope>()) {
        m_channel->pop_scope(pop_scope->name, pop_scope->type);
        return {};
    } else {
        return op.fallback(inputs);
    }
}

}  // namespace imperative
}  // namespace mgb
