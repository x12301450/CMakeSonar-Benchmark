#include "plugin_options.h"
#include <map>
#include "misc.h"
#include "models/model_lite.h"
#include "models/model_mdl.h"
///////////////////// Plugin options///////////////////////////
namespace lar {

template <>
void PluginOption::config_model_internel<ModelLite>(
        RuntimeParam& runtime_param, std::shared_ptr<ModelLite> model) {
    if (runtime_param.stage == RunStage::BEFORE_MODEL_LOAD) {
        LITE_ASSERT(range == 0, "lite model don't support NumRangeChecker plugin");
        LITE_ASSERT(
                !enable_check_dispatch,
                "lite model don't support CPUDispatchChecker plugin");
        LITE_ASSERT(
                var_value_check_str.empty(),
                "lite model don't support VarValueChecker plugin");
    }
#if MGB_ENABLE_JSON
    else if (runtime_param.stage == RunStage::AFTER_MODEL_LOAD) {
        if (!profile_path.empty()) {
            if (!enable_profile_host) {
                LITE_LOG("enable profiling");
                model->get_lite_network()->enable_profile_performance(profile_path);
            } else {
                LITE_LOG("enable profiling for host");
                model->get_lite_network()->enable_profile_performance(profile_path);
            }
        }
    }
#endif
}

template <>
void PluginOption::config_model_internel<ModelMdl>(
        RuntimeParam& runtime_param, std::shared_ptr<ModelMdl> model) {
    if (runtime_param.stage == RunStage::BEFORE_MODEL_LOAD) {
        auto&& config = model->get_mdl_config();
        if (range > 0) {
            mgb_log("enable number range check");
            model->set_num_range_checker(float(range));
        }

        if (enable_check_dispatch) {
            mgb_log("enable cpu dispatch check");
            cpu_dispatch_checker =
                    std::make_unique<mgb::CPUDispatchChecker>(config.comp_graph.get());
        }

        if (!var_value_check_str.empty()) {
            mgb_log("enable variable value check");
            size_t init_idx = 0, switch_interval;
            auto sep = var_value_check_str.find(':');
            if (sep != std::string::npos) {
                switch_interval = std::stoul(var_value_check_str.substr(0, sep));
                init_idx = std::stoul(var_value_check_str.substr(sep + 1));
            } else {
                switch_interval = std::stoul(var_value_check_str);
            }
            var_value_checker = std::make_unique<mgb::VarValueChecker>(
                    config.comp_graph.get(), switch_interval, init_idx);
        }

#if MGB_ENABLE_JSON

        if (!profile_path.empty()) {
            if (!enable_profile_host) {
                mgb_log("enable profiling");
            } else {
                mgb_log("enable profiling for host");
            }
            model->set_profiler();
        }
#endif
    }

    else if (runtime_param.stage == RunStage::AFTER_MODEL_RUNNING) {
#if MGB_ENABLE_JSON
        if (!profile_path.empty()) {
            if (model->get_profiler()) {
                model->get_profiler()
                        ->to_json_full(model->get_async_func().get())
                        ->writeto_fpath(profile_path);
                mgb_log("profiling result written to %s", profile_path.c_str());
            }
        }
#endif
    }
}

}  // namespace lar

using namespace lar;
void PluginOption::update() {
    m_option_name = "plugin";
    range = FLAGS_range;
    enable_check_dispatch = FLAGS_check_dispatch;
    var_value_check_str = FLAGS_check_var_value;
#if MGB_ENABLE_JSON
    enable_profile_host = false;
    if (!FLAGS_profile.empty()) {
        profile_path = FLAGS_profile;
    }
    if (!FLAGS_profile_host.empty()) {
        enable_profile_host = !FLAGS_profile_host.empty();
        profile_path = FLAGS_profile_host;
    }
#endif
}

bool PluginOption::is_valid() {
    bool ret = FLAGS_check_dispatch;
    ret = ret || FLAGS_range > 0;
    ret = ret || !FLAGS_check_var_value.empty();
#if MGB_ENABLE_JSON
    ret = ret || !FLAGS_profile.empty();
    ret = ret || !FLAGS_profile_host.empty();
#endif
    return ret;
}

std::shared_ptr<OptionBase> PluginOption::create_option() {
    static std::shared_ptr<PluginOption> option(new PluginOption);
    if (PluginOption::is_valid()) {
        option->update();
        return std::static_pointer_cast<OptionBase>(option);
    } else {
        return nullptr;
    }
}

void PluginOption::config_model(
        RuntimeParam& runtime_param, std::shared_ptr<ModelBase> model) {
    CONFIG_MODEL_FUN;
}

///////////////////// Debug options///////////////////////////
namespace lar {
template <>
void DebugOption::format_and_print(
        const std::string& tablename, std::shared_ptr<ModelLite> model) {
    auto table = mgb::TextTable(tablename);
    auto&& network = model->get_lite_network();
    table.padding(1);
    table.align(mgb::TextTable::Align::Mid)
            .add("type")
            .add("name")
            .add("shape")
            .add("dtype")
            .eor();

    auto to_string = [&](lite::Layout& layout) {
        std::string shape("{");
        for (size_t i = 0; i < layout.ndim; i++) {
            if (i)
                shape.append(",");
            shape.append(std::to_string(layout.shapes[i]));
        }
        shape.append("}");
        return shape;
    };
    auto get_dtype = [&](lite::Layout& layout) {
        std::map<LiteDataType, std::string> type_map = {
                {LiteDataType::LITE_FLOAT, "float32"},
                {LiteDataType::LITE_HALF, "float16"},
                {LiteDataType::LITE_INT64, "int64"},
                {LiteDataType::LITE_INT, "int32"},
                {LiteDataType::LITE_UINT, "uint32"},
                {LiteDataType::LITE_INT16, "int16"},
                {LiteDataType::LITE_UINT16, "uint16"},
                {LiteDataType::LITE_INT8, "int8"},
                {LiteDataType::LITE_UINT8, "uint8"}};
        return type_map[layout.data_type];
    };

    auto input_name = network->get_all_input_name();
    for (auto& i : input_name) {
        auto layout = network->get_io_tensor(i)->get_layout();
        table.align(mgb::TextTable::Align::Mid)
                .add("INPUT")
                .add(i)
                .add(to_string(layout))
                .add(get_dtype(layout))
                .eor();
    }

    auto output_name = network->get_all_output_name();
    for (auto& i : output_name) {
        auto layout = network->get_io_tensor(i)->get_layout();
        table.align(mgb::TextTable::Align::Mid)
                .add("OUTPUT")
                .add(i)
                .add(to_string(layout))
                .add(get_dtype(layout))
                .eor();
    }

    std::stringstream ss;
    ss << table;
    LITE_LOG("\n%s\n", ss.str().c_str());
}

template <>
void DebugOption::format_and_print(
        const std::string& tablename, std::shared_ptr<ModelMdl> model) {
    auto table = mgb::TextTable(tablename);
    table.padding(1);
    table.align(mgb::TextTable::Align::Mid)
            .add("type")
            .add("name")
            .add("shape")
            .add("dtype")
            .eor();
    auto get_dtype = [&](megdnn::DType data_type) {
        std::map<megdnn::DTypeEnum, std::string> type_map = {
                {mgb::dtype::Float32().enumv(), "float32"},
                {mgb::dtype::Int32().enumv(), "int32"},
                {mgb::dtype::Int16().enumv(), "int16"},
                {mgb::dtype::Uint16().enumv(), "uint16"},
                {mgb::dtype::Int8().enumv(), "int8"},
                {mgb::dtype::Uint8().enumv(), "uint8"}};
        return type_map[data_type.enumv()];
    };
    for (auto&& i : model->get_mdl_load_result().tensor_map) {
        table.align(mgb::TextTable::Align::Mid)
                .add("INPUT")
                .add(i.first)
                .add(i.second->shape().to_string())
                .add(get_dtype(i.second->dtype()))
                .eor();
    }

    for (auto&& i : model->get_mdl_load_result().output_var_list) {
        table.align(mgb::TextTable::Align::Mid)
                .add("OUTPUT")
                .add(i.node()->name())
                .add(i.shape().to_string())
                .add(get_dtype(i.dtype()))
                .eor();
    }

    std::stringstream ss;
    ss << table;
    mgb_log("\n%s\n", ss.str().c_str());
}

template <>
void DebugOption::config_model_internel<ModelLite>(
        RuntimeParam& runtime_param, std::shared_ptr<ModelLite> model) {
    if (runtime_param.stage == RunStage::BEFORE_MODEL_LOAD) {
        LITE_ASSERT(
                !disable_assert_throw, "lite model don't support disable assert throw");
#ifndef __IN_TEE_ENV__
#if MGB_ENABLE_JSON
        LITE_ASSERT(
                static_mem_log_dir_path.empty(),
                "lite model don't support static memory information export");
#endif
#endif
        if (enable_verbose) {
            LITE_LOG("enable verbose");
            lite::set_log_level(LiteLogLevel::DEBUG);
        }

#if __linux__ || __unix__
        if (enable_wait_gdb) {
            printf("wait for gdb attach (pid=%d): ", getpid());
            getchar();
        }
#endif
    } else if (runtime_param.stage == RunStage::AFTER_MODEL_LOAD) {
        if (enable_display_model_info) {
            LITE_LOG("enable display model information");
            format_and_print<ModelLite>("Runtime Model Info", model);
        }
    } else if (runtime_param.stage == RunStage::AFTER_MODEL_RUNNING) {
        if (enable_display_model_info) {
            format_and_print<ModelLite>("Runtime Model Info", model);
        }
    }
}

template <>
void DebugOption::config_model_internel<ModelMdl>(
        RuntimeParam& runtime_param, std::shared_ptr<ModelMdl> model) {
    if (runtime_param.stage == RunStage::BEFORE_MODEL_LOAD) {
        if (enable_verbose) {
            mgb_log("enable verbose");
            mgb::set_log_level(mgb::LogLevel::DEBUG);
        }

#if __linux__ || __unix__
        if (enable_wait_gdb) {
            printf("wait for gdb attach (pid=%d): ", getpid());
            getchar();
        }
#endif
    } else if (runtime_param.stage == RunStage::BEFORE_OUTSPEC_SET) {
        if (enable_display_model_info) {
            mgb_log("enable display model information");
            format_and_print<ModelMdl>("Runtime Model Info", model);
        }
    } else if (runtime_param.stage == RunStage::AFTER_OUTSPEC_SET) {
#ifndef __IN_TEE_ENV__
#if MGB_ENABLE_JSON
        if (!static_mem_log_dir_path.empty()) {
            mgb_log("enable get static memeory information");
            model->get_async_func()->get_static_memory_alloc_info(
                    static_mem_log_dir_path);
        }
#endif
#endif
        if (disable_assert_throw) {
            mgb_log("disable assert throw");
            auto on_opr = [](mgb::cg::OperatorNodeBase* opr) {
                if (opr->same_type<mgb::opr::AssertEqual>()) {
                    opr->cast_final<mgb::opr::AssertEqual>().disable_throw_on_error();
                }
            };
            mgb::cg::DepOprIter iter{on_opr};
            for (auto&& i : model->get_output_spec()) {
                iter.add(i.first.node()->owner_opr());
            }
        }
    } else if (runtime_param.stage == RunStage::AFTER_MODEL_RUNNING) {
        if (enable_display_model_info) {
            format_and_print<ModelMdl>("Runtime Model Info", model);
        }
    }
}

}  // namespace lar

void DebugOption::update() {
    m_option_name = "debug";
    enable_display_model_info = FLAGS_model_info;
    enable_verbose = FLAGS_verbose;
    disable_assert_throw = FLAGS_disable_assert_throw;
#if __linux__ || __unix__
    enable_wait_gdb = FLAGS_wait_gdb;
#endif
#ifndef __IN_TEE_ENV__
#if MGB_ENABLE_JSON
    static_mem_log_dir_path = FLAGS_get_static_mem_info;
#endif
#endif
}

bool DebugOption::is_valid() {
    bool ret = FLAGS_model_info;
    ret = ret || FLAGS_verbose;
    ret = ret || FLAGS_disable_assert_throw;

#if __linux__ || __unix__
    ret = ret || FLAGS_wait_gdb;
#endif
#ifndef __IN_TEE_ENV__
#if MGB_ENABLE_JSON
    ret = ret || !FLAGS_get_static_mem_info.empty();
#endif
#endif
    return ret;
}

std::shared_ptr<OptionBase> DebugOption::create_option() {
    static std::shared_ptr<DebugOption> option(new DebugOption);
    if (DebugOption::is_valid()) {
        option->update();
        return std::static_pointer_cast<OptionBase>(option);
    } else {
        return nullptr;
    }
}

void DebugOption::config_model(
        RuntimeParam& runtime_param, std::shared_ptr<ModelBase> model) {
    CONFIG_MODEL_FUN;
}
///////////////////// Plugin gflags///////////////////////////
DEFINE_double(
        range, 0,
        "check whether absolute value of all numbers in computing graph "
        "is in the given range");

DEFINE_bool(
        check_dispatch, false,
        "check whether an operator call dispatch on cpu comp nodes This is used to "
        "find potential bugs in MegDNN");

DEFINE_string(
        check_var_value, "",
        "--check-var-value [interval]|[interval:init_idx], Enable "
        "VarValueChecker plugin. check values of all vars in a graph from given var "
        "ID(init_idx) with step interval");
#if MGB_ENABLE_JSON
DEFINE_string(
        profile, "",
        "Write profiling result to given file. The output file is in "
        "JSON format");
DEFINE_string(
        profile_host, "",
        "focus on host time profiling For some backends(such as openCL)");
#endif

///////////////////// Debug gflags///////////////////////////
DEFINE_bool(
        model_info, false,
        " Format and display model input/output  tensor inforamtion");

DEFINE_bool(verbose, false, "get more inforamtion for debug");

DEFINE_bool(disable_assert_throw, false, "disable assert throw on error check");
#if __linux__ || __unix__
DEFINE_bool(wait_gdb, false, "print current process PID and wait for gdb attach");
#endif
#ifndef __IN_TEE_ENV__
#if MGB_ENABLE_JSON
DEFINE_string(
        get_static_mem_info, "",
        "Record the static computing graph's static memory information");
#endif
#endif
REGIST_OPTION_CREATOR(plugin, lar::PluginOption::create_option);

REGIST_OPTION_CREATOR(debug, lar::DebugOption::create_option);