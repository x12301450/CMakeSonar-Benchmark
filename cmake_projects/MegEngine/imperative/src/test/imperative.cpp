#include "./helper.h"
#include "megbrain/comp_node_env.h"
#include "megbrain/imperative/blob_manager.h"
#include "megbrain/imperative/ops/autogen.h"
#include "megbrain/imperative/ops/opr_attr.h"
#include "megbrain/opr/basic_arith.h"
#include "megbrain/opr/basic_arith_wrapper.h"
#include "megbrain/opr/blas.h"
#include "megbrain/opr/dnn/batch_norm.h"
#include "megbrain/opr/dnn/convolution.h"
#include "megbrain/opr/tensor_manip.h"
#include "megbrain/opr/utility.h"

using namespace mgb;
using namespace cg;
using namespace imperative;

TEST(TestImperative, APlusB) {
    auto op = OprAttr::make("Elemwise");
    auto&& attr = op->cast_final_safe<OprAttr>();
    using Param = opr::Elemwise::Param;
    Param param{Param::Mode::ADD};
    attr.param.write_pod(param);
    OprChecker(op).run({TensorShape{42}, TensorShape{42}});
}

TEST(TestImperative, Convolution) {
    auto op = OprAttr::make("ConvolutionV2");
    auto&& attr = op->cast_final_safe<OprAttr>();
    using Param = opr::Convolution::Param;
    using Policy = opr::Convolution::ExecutionPolicy;
    Param param{Param::Mode::CONVOLUTION};
    Policy policy{Policy::Strategy::HEURISTIC};
    attr.param.write_pod(param);
    attr.param.write_pod(policy);
    size_t N = 4, IC = 3, OC = 8, FH = 3, FW = 3, IH = 16, IW = 16;
    OprChecker(op).run({TensorShape{N, IC, IH, IW}, TensorShape{OC, IC, FH, FW}});
}

TEST(TestImperative, Reduce) {
    auto op = OprAttr::make("ReduceV2");
    auto&& attr = op->cast_final_safe<OprAttr>();
    using Param = opr::Reduce::Param;
    Param param{Param::Mode::SUM_SQR};
    attr.param.write_pod(param);
    HostTensorND one{CompNode::load("xpu0"), {{1}, dtype::Int32()}};
    one.ptr<int>()[0] = 1;
    OprChecker(op).run({TensorShape{2, 3, 4}, one});
}

TEST(TestImperative, BatchNorm) {
    auto op = OprAttr::make("BatchNormV1");
    auto&& attr = op->cast_final_safe<OprAttr>();
    using Param = opr::BatchNorm::Param;
    Param param;
    param.param_dim = Param::ParamDim::DIM_1C11;
    param.avg_factor = 0.999;
    attr.param.write_pod(param);
    size_t N = 2, C = 3, H = 5, W = 5;
    OprChecker(op).run(
            {TensorShape{N, C, H, W}, TensorShape{1, C, 1, 1}, TensorShape{1, C, 1, 1},
             TensorShape{1, C, 1, 1}, TensorShape{1, C, 1, 1}},
            {4});
}

TEST(TestImperative, Concat) {
    REQUIRE_XPU(2);
    OprAttr::Param param;
    param.write_pod(megdnn::param::Axis(0));
    OperatorNodeConfig config{CompNode::load("xpu1")};
    OprChecker(OprAttr::make("Concat", param, config))
            .run({TensorShape{200, 300}, TensorShape{300, 300}});
}

TEST(TestImperative, Split) {
    OprAttr::Param param;
    param.write_pod(megdnn::param::Axis(0));
    auto op = OprAttr::make("Split", param, OperatorNodeConfig{});
    auto cn = CompNode::load("xpu0");
    HostTensorND s1{cn, {{1}, dtype::Int32()}};
    s1.ptr<int>()[0] = 20;
    HostTensorND s2{cn, {{1}, dtype::Int32()}};
    s2.ptr<int>()[0] = 80;
    OprChecker(op).run({TensorShape{100}, s1, s2});
}

#if MGB_CUDA && MGB_ENABLE_EXCEPTION
void run_graph(size_t mem_reserved) {
    CompNode::try_coalesce_all_free_memory();
    CompNode::finalize();

    auto cn = CompNode::load("gpux");
    cn.sync();  // wait for async init to finish

    HostTensorGenerator<> gen;
    using TensorPtr = std::shared_ptr<Tensor>;
    TensorPtr ptr_a[100];

    size_t unit_size = mem_reserved / (100.5 * 4);
    auto host_a = gen({unit_size});
    for (int i = 0; i < 100; ++i) {
        ptr_a[i] = Tensor::make(*host_a);
    }

    // free half
    for (int i = 0; i < 100; i += 2) {
        ptr_a[i].reset();
    }

    auto op = OprAttr::make("Elemwise");
    auto&& attr = op->cast_final_safe<OprAttr>();
    using Param = opr::Elemwise::Param;
    Param param{Param::Mode::MUL};
    attr.param.write_pod(param);

    SmallVector<LogicalTensorDesc> output_descs;
    auto out = OpDef::apply_on_physical_tensor(
                       *op, {ptr_a[1], ptr_a[99]}, output_descs, false)
                       .at(0);

    // value before defrag
    HostTensorND host_out_before;
    host_out_before.copy_from(out->dev_tensor()).sync();

    // make defrag work
    auto e = Tensor::make(*gen({unit_size * 10}));

    // value after defrag
    HostTensorND host_out_after;
    host_out_after.copy_from(out->dev_tensor()).sync();

    // make sure defragment do not change the value
    for (size_t i = 0; i < unit_size; ++i) {
        ASSERT_EQ(host_out_before.ptr<float>()[i], host_out_after.ptr<float>()[i]);
    }
}

TEST(TestImperative, Defragment) {
#if WIN32
    //! FIXME, finalize on CUDA windows will be strip as windows CUDA101 DLL
    //! issue
    return;
#endif
    REQUIRE_GPU(1);
    CompNode::load("gpux").activate();
    size_t reserve;
    {
        size_t free, tot;
        MGB_CUDA_CHECK(cudaMemGetInfo(&free, &tot));
        reserve = free * 0.92;
    }
    auto reserve_setting = ssprintf("b:%zu", reserve);

    auto do_run = [reserve]() { run_graph(reserve); };

    // reserve memory explicitly to avoid uncontrollable factors
    constexpr const char* KEY = "MGB_CUDA_RESERVE_MEMORY";
    auto old_value = getenv(KEY);
    setenv(KEY, reserve_setting.c_str(), 1);
    MGB_TRY { do_run(); }
    MGB_FINALLY(
            if (old_value) { setenv(KEY, old_value, 1); } else {
                unsetenv(KEY);
            } CompNode::try_coalesce_all_free_memory();
            CompNode::finalize(););
}
#endif  // MGB_CUDA && MGB_ENABLE_EXCEPTION

TEST(TestImperative, MatrixMulApplyOnVarNode) {
    using Param = opr::MatrixMul::Param;
    Param param;
    std::vector<std::pair<TensorShape, TensorShape>> shapes;
    std::vector<TensorShape> target_shapes;
    std::vector<Param> params;
    //! testcase 0
    params.push_back(param);
    shapes.push_back({TensorShape{10, 5}, TensorShape{5, 10}});
    target_shapes.push_back(TensorShape{10, 10});

    params.push_back(param);
    shapes.push_back({TensorShape{3, 10, 5}, TensorShape{5, 10}});
    target_shapes.push_back(TensorShape{3, 10, 10});
    //! testcase 2
    param.transposeA = true;
    param.transposeB = false;
    params.push_back(param);
    shapes.push_back({TensorShape{3, 7, 6}, TensorShape{3, 7, 10}});
    target_shapes.push_back(TensorShape{6, 10});
    //! testcase 3
    param.transposeA = false;
    param.transposeB = true;
    params.push_back(param);
    shapes.push_back({TensorShape{2, 3, 7, 6}, TensorShape{10, 6}});
    target_shapes.push_back(TensorShape{2, 3, 7, 10});

    for (size_t i = 0; i < params.size(); i++) {
        auto& shape = shapes[i];
        auto op = MatrixMul::make(
                params[i], ::megdnn::param::ExecutionPolicy{}, shape.first.ndim,
                shape.second.ndim);
        auto result = OprChecker(op).run_apply_on_var_node({shape.first, shape.second});
        ASSERT_GT(result.size(), 0);
        ASSERT_EQ(target_shapes[i].ndim, result[0]->shape().ndim);
        for (size_t id = 0; id < target_shapes[i].ndim; id++) {
            ASSERT_EQ(target_shapes[i][id], result[0]->shape()[id]);
        }
    }
}

// vim: syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
