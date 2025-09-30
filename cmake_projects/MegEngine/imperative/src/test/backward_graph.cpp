#include "./helper.h"
#include "megbrain/imperative/backward_graph_opt.h"
#include "megbrain/imperative/ops/autogen.h"
#include "megbrain/imperative/ops/opr_attr.h"
#include "megbrain/opr/basic_arith.h"
#include "megbrain/opr/dnn/batch_norm.h"

using namespace mgb;
using namespace cg;
using namespace imperative;

template <typename T>
T prepare_backward_graph_inputs(
        const EncodedSubgraph& bg, const T& inputs, const T& outputs, const T& grads) {
    T ret;
    size_t i = 0;
    for (auto&& t : inputs) {
        if (bg.input_mask[i++]) {
            ret.push_back(t);
        }
    }
    for (auto&& t : outputs) {
        if (bg.input_mask[i++]) {
            ret.push_back(t);
        }
    }
    for (auto&& t : grads) {
        if (bg.input_mask[i++]) {
            ret.push_back(t);
        }
    }
    return ret;
}

template <typename T, typename U>
T expand_grads(const U& mask, const T& outputs) {
    T ret(mask.size());
    for (size_t i = 0, j = 0; i < mask.size(); ++i) {
        if (mask[i]) {
            ret[i] = outputs[j++];
        }
    }
    return ret;
}

template <typename T>
T prepare_optimized_backward_inputs(
        const OptimizedBackwardGraphResult& bg, const T& precomp, const T& inputs,
        const T& outputs, const T& grads) {
    T ret = precomp;
    size_t i = 0;
    for (auto&& t : inputs) {
        if (bg.save_for_backward[i++]) {
            ret.push_back(t);
        }
    }
    for (auto&& t : outputs) {
        if (bg.save_for_backward[i++]) {
            ret.push_back(t);
        }
    }
    for (auto&& t : grads) {
        if (bg.save_for_backward[i++]) {
            ret.push_back(t);
        }
    }
    return ret;
}

SmallVector<TensorPtr> apply_shared_on_physical_tensor(
        std::shared_ptr<OpDef> def, SmallVector<TensorPtr> inputs, size_t nr_outputs) {
    SmallVector<LogicalTensorDesc> input_descs;
    for (auto&& i : inputs) {
        input_descs.push_back({i->layout(), i->comp_node()});
    }
    auto [output_descs, validated] =
            OpDef::infer_output_attrs_fallible(*def, input_descs);
    return OpDef::apply_on_physical_tensor(*def, inputs, output_descs, validated);
}

TEST(TestImperative, BackwardGraphBasic) {
    HostTensorGenerator<> gen;
    SmallVector<HostTensorND> hvs;
    SmallVector<TensorPtr> inputs;
    for (size_t i = 0; i < 2; ++i) {
        hvs.push_back(*gen({42}));
        inputs.push_back(Tensor::make(hvs.back()));
    }

    using Param = opr::Elemwise::Param;
    Param param{Param::Mode::MUL};
    auto attr = OprAttr::make("Elemwise");
    attr->cast_final_safe<OprAttr>().param.write_pod(param);

    SmallVector<LogicalTensorDesc> input_descs;
    for (auto&& i : inputs) {
        input_descs.push_back({i->layout(), i->comp_node()});
    }
    auto result = OpDef::make_backward_graph(*attr, input_descs, {true, true}, {true});
    auto&& save_for_backward = result.input_mask;
    auto&& input_has_grad = result.output_mask;

    for (size_t i = 0; i < inputs.size(); i++) {
        input_descs[i].value = inputs[i]->dev_tensor();
    }
    auto [output_descs, validated] =
            OpDef::infer_output_attrs_fallible(*attr, input_descs);
    auto outputs =
            OpDef::apply_on_physical_tensor(*attr, inputs, output_descs, validated);
    inputs.push_back(outputs[0]);
    hvs.push_back(*gen({42}));
    inputs.push_back(Tensor::make(hvs.back()));
    mgb_assert(save_for_backward.size() == inputs.size());
    for (size_t i = 0; i < inputs.size(); ++i) {
        if (!save_for_backward[i]) {
            inputs[i].reset();  // drop unused tensor
        }
    }
    SmallVector<TensorPtr> backward_graph_inputs;
    for (auto&& i : inputs) {
        if (i) {
            backward_graph_inputs.push_back(i);
        }
    }
    inputs.clear();
    auto input_grads = result.graph.apply<TensorPtr>(
            backward_graph_inputs, apply_shared_on_physical_tensor,
            [&](auto&& x) { return x; });
    mgb_assert(input_grads.size() == input_has_grad.size());
    for (size_t i = 0; i < input_has_grad.size(); ++i) {
        mgb_assert(input_has_grad[i] == static_cast<bool>(input_grads[i]));
    }

    SmallVector<HostTensorND> res;
    for (auto&& i : input_grads) {
        res.emplace_back();
        res.back().copy_from(i->dev_tensor()).sync();
    }
    for (size_t i = 0; i < 42; ++i) {
        for (size_t j = 0; j < 1; ++j) {
            ASSERT_EQ(
                    hvs[2].ptr<float>()[i] * hvs[j].ptr<float>()[i],
                    res[j ^ 1].ptr<float>()[i]);
        }
    }
}

TEST(TestImperative, ProfileBackward) {
    auto cn = CompNode::load("xpux");
    using Policy = megdnn::param::ExecutionPolicy;
    using S = Policy::Strategy;
    Policy policy;
    policy.strategy = S::PROFILE;
    {
        megdnn::param::Convolution param;
        auto op = std::shared_ptr<OpDef>(Convolution::make(param, policy));
        LogicalTensorDesc inp_desc = {
                TensorLayout({16, 3, 16, 16}, dtype::Float32()), cn};
        LogicalTensorDesc weight_desc = {
                TensorLayout({16, 3, 5, 5}, dtype::Float32()), cn};
        auto bg = OpDef::make_backward_graph(
                *op, {inp_desc, weight_desc}, {true, false}, {true});
        auto&& bop = (bg.graph.exprs.at(0)).op;
        auto&& attr = bop->cast_final_safe<OprAttr>();
        // attr.type = ConvolutionBackwardDataV2
        mgb_assert(attr.policy.strategy == S::PROFILE);
    }
    {
        megdnn::param::Pooling param;
        auto op = std::shared_ptr<OpDef>(Pooling::make(param, policy));
        LogicalTensorDesc inp_desc = {
                TensorLayout({16, 3, 16, 16}, dtype::Float32()), cn};
        auto bg = OpDef::make_backward_graph(*op, {inp_desc}, {true}, {true});
        auto&& bop = (bg.graph.exprs.at(0)).op;
        auto&& attr = bop->cast_final_safe<OprAttr>();
        // attr.type = PoolingBackwardV1
        mgb_assert(attr.policy.strategy == S::PROFILE);
    }
    {
        megdnn::param::MatrixMul param;
        auto op = std::shared_ptr<OpDef>(MatrixMul::make(param, policy, 2, 2));
        LogicalTensorDesc inp1_desc = {TensorLayout({12, 16}, dtype::Float32()), cn};
        LogicalTensorDesc inp2_desc = {TensorLayout({16, 20}, dtype::Float32()), cn};
        auto bg = OpDef::make_backward_graph(
                *op, {inp1_desc, inp2_desc}, {true, false}, {true});
        auto&& bop = (bg.graph.exprs.at(0)).op;
        auto&& attr = bop->cast_final_safe<OprAttr>();
        // attr.type = MatrixMulV2
        mgb_assert(attr.policy.strategy == S::PROFILE);
    }
}

TEST(TestImperative, BackwardGraphIdentity) {
    HostTensorGenerator<> gen;
    auto host_a = gen({42}), host_dc = gen({42});
    auto a = Tensor::make(*host_a), dc = Tensor::make(*host_dc);
    SmallVector<TensorPtr> inputs;
    inputs.push_back(a);

    auto attr = OprAttr::make("Identity");
    attr->cast_final_safe<OprAttr>().param.write_pod<megdnn::param::Empty>({});

    SmallVector<LogicalTensorDesc> input_descs;
    input_descs.push_back({a->layout(), a->comp_node()});
    auto result = OpDef::make_backward_graph(*attr, input_descs, {true}, {true});
    auto&& save_for_backward = result.input_mask;
    auto&& input_has_grad = result.output_mask;

    auto [output_descs, validated] =
            OpDef::infer_output_attrs_fallible(*attr, input_descs);
    auto outputs =
            OpDef::apply_on_physical_tensor(*attr, inputs, output_descs, validated);
    inputs.push_back(outputs[0]);
    inputs.push_back(dc);
    mgb_assert(save_for_backward.size() == inputs.size());
    for (size_t i = 0; i < inputs.size(); ++i) {
        if (!save_for_backward[i]) {
            inputs[i].reset();  // drop unused tensor
        }
    }
    SmallVector<TensorPtr> backward_graph_inputs;
    for (auto&& i : inputs) {
        if (i) {
            backward_graph_inputs.push_back(i);
        }
    }
    inputs.clear();
    auto input_grads = result.graph.apply<TensorPtr>(
            backward_graph_inputs, apply_shared_on_physical_tensor,
            [&](auto&& x) { return x; });
    mgb_assert(input_grads.size() == input_has_grad.size());
    for (size_t i = 0; i < input_has_grad.size(); ++i) {
        mgb_assert(input_has_grad[i] == static_cast<bool>(input_grads[i]));
    }

    HostTensorND hv;
    hv.copy_from(input_grads[0]->dev_tensor()).sync();
    for (size_t i = 0; i < 42; ++i) {
        ASSERT_EQ(host_dc->ptr<float>()[i], hv.ptr<float>()[i]);
    }
}

TEST(TestImperative, BatchNormGrad) {
    auto cn = CompNode::load("xpux");
    using Param = opr::BatchNorm::Param;
    size_t N = 2, C = 3, H = 5, W = 5;
    LogicalTensorDesc inp{TensorLayout{{N, C, H, W}, dtype::Float32()}, cn};
    LogicalTensorDesc stat{TensorLayout{{C}, dtype::Float32()}, cn};
    {
        auto op = OprAttr::make("BatchNormV1");
        auto&& attr = op->cast_final_safe<OprAttr>();
        Param param;
        param.fwd_mode = Param::FwdMode::TRAINING;
        attr.param.write_pod(param);
        OpDef::make_backward_graph(
                attr, {inp, stat, stat, stat, stat}, {true, true, true, false, false},
                {false, false, false, false, false, true});
    }
    {
        auto op = OprAttr::make("BatchNormV1");
        auto&& attr = op->cast_final_safe<OprAttr>();
        Param param;
        param.fwd_mode = Param::FwdMode::TRAINING;
        attr.param.write_pod(param);
        OpDef::make_backward_graph(
                attr, {inp, stat, stat}, {true, true, true},
                {false, false, false, true});
    }
}

TEST(TestImperative, OptimizedBackwardGraphBasic) {
    auto cn = CompNode::load("xpux");
    LogicalTensorDesc desc = {TensorLayout(dtype::Float32()), cn};
    HostTensorGenerator<> gen;
    auto op = std::shared_ptr<OpDef>(Elemwise::make(Elemwise::Mode::ADD));
    auto bg = OpDef::make_backward_graph(*op, {desc, desc}, {true, true}, {true});
    auto obg = OptimizedBackwardGraphResult(bg);
    ASSERT_EQ(obg.save_for_backward.size(), 4);
    ASSERT_FALSE(obg.save_for_backward[0]);
    ASSERT_FALSE(obg.save_for_backward[1]);
    ASSERT_FALSE(obg.save_for_backward[2]);

    auto a_hv = gen({42});
    auto b_hv = gen({5, 42});
    auto dc_hv = gen({5, 42});
    auto a_tn = Tensor::make(*a_hv);
    auto b_tn = Tensor::make(*b_hv);
    auto dc_tn = Tensor::make(*dc_hv);
    SmallVector<LogicalTensorDesc> input_descs;
    input_descs.push_back({a_tn->layout(), a_tn->comp_node(), a_tn->dev_tensor()});
    input_descs.push_back({b_tn->layout(), b_tn->comp_node(), b_tn->dev_tensor()});
    auto [output_descs, validated] =
            OpDef::infer_output_attrs_fallible(*op, input_descs);
    auto c_tn = OpDef::apply_on_physical_tensor(
            *op, {a_tn, b_tn}, output_descs, validated)[0];

    auto backward_graph_inputs = prepare_backward_graph_inputs<SmallVector<TensorPtr>>(
            bg, {a_tn, b_tn}, {c_tn}, {dc_tn});
    auto grads = expand_grads(
            bg.output_mask,
            bg.graph.apply<TensorPtr>(
                    backward_graph_inputs, apply_shared_on_physical_tensor,
                    [&](auto&& x) { return x; }));

    auto precomp = obg.precomp.apply<TensorPtr>(
            SmallVector<TensorPtr>{a_tn, b_tn, c_tn}, apply_shared_on_physical_tensor,
            [&](auto&& x) { return x; });
    ASSERT_EQ(precomp.size(), 2);
    ASSERT_EQ(precomp[0]->shape().ndim, 1);
    ASSERT_LE(precomp[0]->shape()[0], 2);
    ASSERT_EQ(precomp[1]->shape().ndim, 1);
    ASSERT_LE(precomp[1]->shape()[0], 2);

    auto backward_inputs = prepare_optimized_backward_inputs<SmallVector<TensorPtr>>(
            obg, precomp, {a_tn, b_tn}, {c_tn}, {dc_tn});
    auto grads2 = expand_grads(
            obg.input_has_grad,
            obg.backward.apply<TensorPtr>(
                    backward_inputs, apply_shared_on_physical_tensor,
                    [&](auto&& x) { return x; }));

    ASSERT_EQ(grads2.size(), 2);
    MGB_ASSERT_TENSOR_EQ(grads[0]->get_value(), grads2[0]->get_value());
    MGB_ASSERT_TENSOR_EQ(grads[1]->get_value(), grads2[1]->get_value());
}
