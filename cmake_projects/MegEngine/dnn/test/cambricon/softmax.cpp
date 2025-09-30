#include "test/cambricon/fixture.h"

#include "megdnn/tensor_iter.h"
#include "test/common/checker.h"
#include "test/common/softmax.h"

#include "src/common/utils.h"

namespace megdnn {
namespace test {

TEST_F(CAMBRICON, SOFTMAX_FORWARD) {
    auto args = softmax::get_args();
    std::vector<DType> dtypes{dtype::Float16(), dtype::Float32()};

    for (auto dtype : dtypes)
        for (auto&& arg : args) {
            auto param = arg.param;
            auto src = arg.ishape;
            Checker<Softmax> checker(handle_cambricon());
            if (dtype == dtype::BFloat16()) {
                checker.set_epsilon(2e-2);
            } else {
                checker.set_epsilon(1e-2);
            }
            checker.set_param(param).set_dtype(0, dtype).set_dtype(1, dtype).exec(
                    TensorShapeArray{src, {}});
        }
}

TEST_F(CAMBRICON, SOFTMAX_BACKWARD) {
    auto args = softmax::get_args();
    for (auto&& arg : args) {
        Checker<SoftmaxBackward> checker(handle_cambricon());
        TensorLayout ilayout = TensorLayout(arg.ishape, dtype::Float32());
        TensorLayout olayout;

        {
            auto opr = handle_cambricon()->create_operator<SoftmaxForward>();
            opr->param() = arg.param;
            opr->deduce_layout(ilayout, olayout);
        }
        auto set_dtype = [&checker](DType dtype) {
            checker.set_dtype(0, dtype).set_dtype(1, dtype).set_dtype(2, dtype);
        };

        set_dtype(dtype::Float32());
        checker.set_epsilon(1e-3).set_param(arg.param).exec(
                TensorShapeArray{ilayout, olayout, ilayout});
    }
}

}  // namespace test
}  // namespace megdnn

// vim: syntax=cpp.doxygen
