#include <iostream>
#include <sstream>
#include "lite/global.h"
#include "megbrain/comp_node_env.h"
#include "misc.h"
#include "device_options.h"
#include "models/model_lite.h"
#include "models/model_mdl.h"

DECLARE_bool(weight_preprocess);

using namespace lar;

/////////////////// XPUDeviceOption //////////////////////
namespace lar {
template <>
void XPUDeviceOption::config_model_internel<ModelLite>(
        RuntimeParam& runtime_param, std::shared_ptr<ModelLite> model) {
    if (runtime_param.stage == RunStage::BEFORE_MODEL_LOAD) {
        if ((enable_cpu) || (enable_cpu_default) || (enable_multithread) ||
            (enable_multithread_default)) {
            LITE_LOG("using cpu device\n");
            model->get_config().device_type = LiteDeviceType::LITE_CPU;
        }
#if LITE_WITH_CUDA
        if (enable_cuda) {
            LITE_LOG("using cuda device\n");
            model->get_config().device_type = LiteDeviceType::LITE_CUDA;
        }
#endif
    } else if (runtime_param.stage == RunStage::AFTER_NETWORK_CREATED) {
        auto&& network = model->get_lite_network();
        if (enable_cpu_default) {
            LITE_LOG("using cpu default device\n");
            lite::Runtime::set_cpu_inplace_mode(network);
        }
        if (enable_multithread) {
            LITE_LOG("using multithread device\n");
            lite::Runtime::set_cpu_threads_number(network, thread_num);
        }
        if (enable_multithread_default) {
            LITE_LOG("using multithread  default device\n");
            lite::Runtime::set_cpu_inplace_mode(network);
            lite::Runtime::set_cpu_threads_number(network, thread_num);
        }
        if (enable_set_core_ids) {
            std::string core_str;
            for (auto id : core_ids) {
                core_str += std::to_string(id) + ",";
            }
            LITE_LOG("multi thread core ids: %s\n", core_str.c_str());
            lite::ThreadAffinityCallback affinity_callback = [&](size_t thread_id) {
                mgb::sys::set_cpu_affinity({core_ids[thread_id]});
            };
            lite::Runtime::set_runtime_thread_affinity(network, affinity_callback);
        }
    }
}

template <>
void XPUDeviceOption::config_model_internel<ModelMdl>(
        RuntimeParam& runtime_param, std::shared_ptr<ModelMdl> model) {
    if (runtime_param.stage == RunStage::BEFORE_MODEL_LOAD) {
        if (enable_cpu) {
            mgb_log("using cpu device\n");
            model->get_mdl_config().comp_node_mapper = [](mgb::CompNode::Locator& loc) {
                loc.type = mgb::CompNode::DeviceType::CPU;
            };
        }
#if LITE_WITH_CUDA
        if (enable_cuda) {
            mgb_log("using cuda device\n");
            model->get_mdl_config().comp_node_mapper = [](mgb::CompNode::Locator& loc) {
                if (loc.type == mgb::CompNode::DeviceType::UNSPEC) {
                    loc.type = mgb::CompNode::DeviceType::CUDA;
                }
                loc.device = 0;
            };
        }
#endif
        if (enable_cpu_default) {
            mgb_log("using cpu default device\n");
            model->get_mdl_config().comp_node_mapper = [](mgb::CompNode::Locator& loc) {
                loc.type = mgb::CompNode::DeviceType::CPU;
                loc.device = mgb::CompNode::Locator::DEVICE_CPU_DEFAULT;
            };
        }
        if (enable_multithread) {
            mgb_log("using multithread(threads number:%ld) device\n", thread_num);
            model->get_mdl_config().comp_node_mapper =
                    [&](mgb::CompNode::Locator& loc) {
                        loc.type = mgb::CompNode::DeviceType::MULTITHREAD;
                        loc.device = 0;
                        loc.stream = thread_num;
                    };
        }
        if (enable_multithread_default) {
            mgb_log("using multithread default device\n");
            model->get_mdl_config().comp_node_mapper =
                    [&](mgb::CompNode::Locator& loc) {
                        loc.type = mgb::CompNode::DeviceType::MULTITHREAD;
                        loc.device = mgb::CompNode::Locator::DEVICE_MULTITHREAD_DEFAULT;
                        loc.stream = thread_num;
                    };
        }
        if (enable_set_core_ids) {
            std::string core_str;
            for (auto id : core_ids) {
                core_str += std::to_string(id) + ",";
            }
            mgb_log("set multi thread core ids:%s\n", core_str.c_str());
            auto affinity_callback = [&](size_t thread_id) {
                mgb::sys::set_cpu_affinity({core_ids[thread_id]});
            };
            mgb::CompNode::Locator loc;
            model->get_mdl_config().comp_node_mapper(loc);
            auto comp_node = mgb::CompNode::load(loc);
            mgb::CompNodeEnv::from_comp_node(comp_node).cpu_env().set_affinity(
                    affinity_callback);
        }
    }
}
}  // namespace lar

void XPUDeviceOption::update() {
    m_option_name = "xpu_device";
    enable_cpu = FLAGS_cpu;
#if LITE_WITH_CUDA
    enable_cuda = FLAGS_cuda;
#endif
    enable_cpu_default = FLAGS_cpu_default;

    if (FLAGS_multithread >= 0) {
        thread_num = FLAGS_multithread;
        enable_multithread = true;
    }

    if (FLAGS_multithread_default >= 0) {
        thread_num = FLAGS_multithread_default;
        enable_multithread_default = true;
    }

    if (!FLAGS_multi_thread_core_ids.empty()) {
        mgb_assert(
                enable_multithread || enable_multithread_default,
                "core ids should be set after --multithread or --multithread-default");
        std::stringstream id_stream(FLAGS_multi_thread_core_ids);
        std::string id;
        size_t thread_cnt = 0;
        while (getline(id_stream, id, ',')) {
            thread_cnt++;
            core_ids.push_back(atoi(id.c_str()));
        }
        mgb_assert(
                thread_cnt == thread_num,
                "core ids number should be same with thread number set before");
        enable_set_core_ids = true;
    } else {
        enable_set_core_ids = false;
    }

    m_option = {
        {"cpu", lar::Bool::make(false)},
#if LITE_WITH_CUDA
        {"cuda", lar::Bool::make(false)},
#endif
        {"cpu_default", lar::Bool::make(false)},
        {"multithread", lar::NumberInt32::make(-1)},
        {"multithread_default", lar::NumberInt32::make(-1)},
        {"multi_thread_core_ids", lar::String::make("")},
    };
    std::static_pointer_cast<lar::Bool>(m_option["cpu"])->set_value(FLAGS_cpu);
#if LITE_WITH_CUDA
    std::static_pointer_cast<lar::Bool>(m_option["cuda"])->set_value(FLAGS_cuda);
#endif
    std::static_pointer_cast<lar::Bool>(m_option["cpu_default"])
            ->set_value(FLAGS_cpu_default);
    std::static_pointer_cast<lar::NumberInt32>(m_option["multithread"])
            ->set_value(FLAGS_multithread);
    std::static_pointer_cast<lar::NumberInt32>(m_option["multithread_default"])
            ->set_value(FLAGS_multithread_default);
    std::static_pointer_cast<lar::String>(m_option["multi_thread_core_ids"])
            ->set_value(FLAGS_multi_thread_core_ids);
}
bool XPUDeviceOption::m_valid;
bool XPUDeviceOption::is_valid() {
    bool ret = FLAGS_cpu || FLAGS_cpu_default;
#if LITE_WITH_CUDA
    ret = ret || FLAGS_cuda;
#endif
    ret = ret || FLAGS_multithread >= 0;
    ret = ret || FLAGS_multithread_default >= 0;
    ret = ret || !FLAGS_multi_thread_core_ids.empty();

    return ret || m_valid;
}

std::shared_ptr<OptionBase> XPUDeviceOption::create_option() {
    static std::shared_ptr<lar::XPUDeviceOption> option(new XPUDeviceOption);
    if (XPUDeviceOption::is_valid()) {
        option->update();
        return std::static_pointer_cast<lar::OptionBase>(option);
    } else {
        return nullptr;
    }
}

void XPUDeviceOption::config_model(
        RuntimeParam& runtime_param, std::shared_ptr<ModelBase> model) {
    enable_cpu = std::static_pointer_cast<lar::Bool>(m_option["cpu"])->get_value();
#if LITE_WITH_CUDA
    enable_cuda = std::static_pointer_cast<lar::Bool>(m_option["cuda"])->get_value();
#endif
    enable_cpu_default =
            std::static_pointer_cast<lar::Bool>(m_option["cpu_default"])->get_value();
    int32_t num_of_thread =
            std::static_pointer_cast<lar::NumberInt32>(m_option["multithread"])
                    ->get_value();
    enable_multithread = num_of_thread >= 0;
    int32_t num_of_thread_dft =
            std::static_pointer_cast<lar::NumberInt32>(m_option["multithread_default"])
                    ->get_value();
    enable_multithread_default = num_of_thread_dft >= 0;
    mgb_assert(
            num_of_thread < 0 || num_of_thread_dft < 0,
            "multithread and multithread_default should not bet set at the same time");
    thread_num = num_of_thread >= 0 ? num_of_thread
                                    : (num_of_thread_dft >= 0 ? num_of_thread_dft : -1);
    std::string core_id_str =
            std::static_pointer_cast<lar::String>(m_option["multi_thread_core_ids"])
                    ->get_value();
    if (!core_id_str.empty()) {
        mgb_assert(
                enable_multithread || enable_multithread_default,
                "core ids should be set after --multithread or --multithread-default");
        std::stringstream id_stream(core_id_str);
        std::string id;
        size_t thread_cnt = 0;
        while (getline(id_stream, id, ',')) {
            thread_cnt++;
            core_ids.push_back(atoi(id.c_str()));
        }
        mgb_assert(
                thread_cnt == thread_num,
                "core ids number should be same with thread number set before");
        enable_set_core_ids = true;
    } else {
        enable_set_core_ids = false;
    }

    CONFIG_MODEL_FUN;
}
///////////////////////// xpu gflags ////////////////////////////
DEFINE_bool(cpu, false, "set CPU device as running device");
#if LITE_WITH_CUDA
DEFINE_bool(cuda, false, "set CUDA device as running device ");
#endif
DEFINE_bool(cpu_default, false, "set running device as CPU device with inplace mode");
DEFINE_int32(multithread, -1, "set multithread device as running device");
DEFINE_int32(
        multithread_default, -1,
        "set multithread device as running device with inplace mode");
DEFINE_string(multi_thread_core_ids, "", "set multithread core id");
REGIST_OPTION_CREATOR(xpu_device, lar::XPUDeviceOption::create_option);
REGIST_OPTION_VALIDATER(xpu_device, lar::XPUDeviceOption::set_valid);
