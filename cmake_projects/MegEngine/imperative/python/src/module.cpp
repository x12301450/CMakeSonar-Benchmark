#include <pybind11/eval.h>

#define DO_IMPORT_ARRAY
#include "./helper.h"
#include "./numpy_dtypes.h"
#include "megdnn/handle.h"

#include "./common.h"
#include "./graph_rt.h"
#include "./imperative_rt.h"
#include "./ops.h"
#include "./utils.h"

#include "./tensor.h"

namespace py = pybind11;
using namespace mgb::imperative::python;

#ifndef MODULE_NAME
#define MODULE_NAME imperative_rt
#endif

namespace megdnn {
extern const std::shared_ptr<Handle>& inplace_cpu_handle(int debug_level = 0);
}

PYBIND11_MODULE(MODULE_NAME, m) {
    // initialize numpy
    if ([]() {
            import_array1(1);
            return 0;
        }()) {
        throw py::error_already_set();
    }

    megdnn::inplace_cpu_handle();

    py::module::import("sys").attr("modules")[m.attr("__name__")] = m;

    m.attr("__package__") = m.attr("__name__");
    m.attr("__builtins__") = py::module::import("builtins");

    auto atexit = py::module::import("atexit");
    atexit.attr("register")(py::cpp_function([]() {
        py::gil_scoped_release _;
        py_task_q.wait_all_task_finish();
    }));

    auto common = submodule(m, "common");
    auto utils = submodule(m, "utils");
    auto imperative = submodule(m, "imperative");
    auto graph = submodule(m, "graph");
    auto ops = submodule(m, "ops");

    init_common(common);
    init_utils(utils);
    init_imperative_rt(imperative);
    init_graph_rt(graph);
    init_ops(ops);

    py::exec(
            R"(
        from .common import *
        from .utils import *
        from .imperative import *
        from .graph import *
        from .ops import OpDef
        )",
            py::getattr(m, "__dict__"));

    init_tensor(submodule(m, "core2"));
}
