#include <map>

#include "helpers/data_parser.h"
#include "misc.h"
#include "models/model_lite.h"
#include "models/model_mdl.h"

#include "io_options.h"
namespace lar {
template <>
void InputOption::config_model_internel<ModelLite>(
        RuntimeParam& runtime_param, std::shared_ptr<ModelLite> model) {
    if (runtime_param.stage == RunStage::BEFORE_MODEL_LOAD) {
        auto&& parser = model->get_input_parser();
        auto&& io = model->get_networkIO();
        for (size_t idx = 0; idx < data_path.size(); ++idx) {
            parser.feed(data_path[idx].c_str());
        }

        auto inputs = parser.inputs;
        bool is_host = true;
        for (auto& i : inputs) {
            io.inputs.push_back({i.first, is_host});
        }
    } else if (runtime_param.stage == RunStage::AFTER_MODEL_LOAD) {
        auto&& parser = model->get_input_parser();
        auto&& network = model->get_lite_network();

        //! datd type map from lite data type to  mgb data type
        std::map<LiteDataType, megdnn::DTypeEnum> type_map = {
                {LiteDataType::LITE_FLOAT, megdnn::DTypeEnum::Float32},
                {LiteDataType::LITE_INT, megdnn::DTypeEnum::Int32},
                {LiteDataType::LITE_INT8, megdnn::DTypeEnum::Int8},
                {LiteDataType::LITE_UINT8, megdnn::DTypeEnum::Uint8}};

        if (m_force_batch_size > 0) {
            LITE_WARN("force set batch size to %d", m_force_batch_size);
            auto all_inputs_name = network->get_all_input_name();
            for (auto& name : all_inputs_name) {
                std::shared_ptr<lite::Tensor> input_tensor =
                        network->get_io_tensor(name);
                //! set lite layout
                lite::Layout layout;
                mgb::TensorShape new_shape;
                new_shape.ndim = input_tensor->get_layout().ndim;
                layout.ndim = input_tensor->get_layout().ndim;
                for (size_t idx = 0; idx < new_shape.ndim; idx++) {
                    new_shape.shape[idx] = input_tensor->get_layout().shapes[idx];
                    layout.shapes[idx] = new_shape.shape[idx];
                }
                new_shape.shape[0] = m_force_batch_size;
                layout.shapes[0] = m_force_batch_size;

                //! gengrate tesnor copy from origin tensor
                mgb::HostTensorND hv;
                hv.comp_node(mgb::CompNode::default_cpu(), true)
                        .dtype(megdnn::DType::from_enum(
                                type_map[input_tensor->get_layout().data_type]))
                        .resize(new_shape);
                mgb::dt_byte* raw_ptr = hv.raw_ptr();
                //! single batch input size
                size_t batch_stride = hv.dtype().size() * hv.layout().total_nr_elems() /
                                      m_force_batch_size;
                size_t curr_batch_size = m_force_batch_size;
                //! copy data from origin input_tensor
                size_t init_batch = input_tensor->get_layout().shapes[0];
                while (curr_batch_size > init_batch) {
                    memcpy((char*)raw_ptr, (char*)(input_tensor->get_memory_ptr()),
                           batch_stride * init_batch);
                    curr_batch_size -= init_batch;
                    raw_ptr += batch_stride * init_batch;
                }
                memcpy((char*)raw_ptr, (char*)(input_tensor->get_memory_ptr()),
                       batch_stride * curr_batch_size);

                input_tensor->reset(hv.raw_ptr(), layout);
                parser.inputs[name] = std::move(hv);
            }
        } else {
            for (auto& i : parser.inputs) {
                //! get tensor information from data parser
                auto tensor = i.second;
                auto tensor_shape = tensor.shape();
                mgb::dt_byte* src = tensor.raw_ptr();
                std::shared_ptr<lite::Tensor> input_tensor =
                        network->get_io_tensor(i.first);
                //! set lite layout
                lite::Layout layout;
                layout.ndim = tensor_shape.ndim;
                for (size_t idx = 0; idx < tensor_shape.ndim; idx++) {
                    layout.shapes[idx] = tensor_shape[idx];
                }
                layout.data_type = input_tensor->get_layout().data_type;

                //! set data for only given shape
                if (tensor.storage().empty()) {
                    mgb::HostTensorND hv;
                    hv.comp_node(mgb::CompNode::default_cpu(), true)
                            .dtype(megdnn::DType::from_enum(type_map[layout.data_type]))
                            .resize(tensor.shape());
                    mgb::dt_byte* raw_ptr = hv.raw_ptr();
                    //! set all value in tesnor to 1
                    memset((char*)raw_ptr, 1,
                           hv.layout().total_nr_elems() * hv.dtype().size());
                    parser.inputs[i.first] = std::move(hv);
                    input_tensor->reset(raw_ptr, layout);
                } else {
                    //! set network input tensor
                    input_tensor->reset(src, layout);
                }
            }
        }
    }
}

template <>
void InputOption::config_model_internel<ModelMdl>(
        RuntimeParam& runtime_param, std::shared_ptr<ModelMdl> model) {
    if (runtime_param.stage == RunStage::BEFORE_MODEL_LOAD) {
        auto&& parser = model->get_input_parser();
        for (size_t idx = 0; idx < data_path.size(); ++idx) {
            parser.feed(data_path[idx].c_str());
        }
    } else if (runtime_param.stage == RunStage::AFTER_MODEL_LOAD) {
        auto&& parser = model->get_input_parser();
        auto&& network = model->get_mdl_load_result();
        auto&& tensormap = network.tensor_map;

        if (m_force_batch_size > 0) {
            mgb_log_warn("force set batch size to %d", m_force_batch_size);
            for (auto& iter : tensormap) {
                auto& in = iter.second;
                mgb::HostTensorND hv;
                mgb::TensorShape new_shape = in->shape();
                new_shape[0] = m_force_batch_size;
                hv.comp_node(mgb::CompNode::default_cpu(), true)
                        .dtype(in->dtype())
                        .resize(new_shape);
                mgb::dt_byte* raw_ptr = hv.raw_ptr();

                //! copy given batch data into new tensor
                size_t batch_stride = in->dtype().size() *
                                      in->layout().total_nr_elems() / (in->shape()[0]);
                size_t curr_batch_size = m_force_batch_size;

                //! copy data from origin input_tensor
                size_t init_batch = in->shape()[0];
                while (curr_batch_size > init_batch) {
                    memcpy((char*)raw_ptr, (char*)(in->raw_ptr()),
                           batch_stride * init_batch);
                    curr_batch_size -= init_batch;
                    raw_ptr += batch_stride * init_batch;
                }
                memcpy((char*)raw_ptr, (char*)(in->raw_ptr()),
                       batch_stride * curr_batch_size);
                //! set input tensor
                in->copy_from(hv);
                parser.inputs[iter.first] = std::move(hv);
            }
        } else {
            for (auto& i : parser.inputs) {
                mgb_assert(
                        tensormap.find(i.first) != tensormap.end(),
                        "can't find tesnor named %s", i.first.c_str());
                auto& in = tensormap.find(i.first)->second;
                if (i.second.storage().empty()) {
                    mgb::HostTensorND hv;
                    hv.comp_node(mgb::CompNode::default_cpu(), true)
                            .dtype(in->dtype())
                            .resize(i.second.shape());
                    mgb::dt_byte* raw_ptr = hv.raw_ptr();
                    memset((char*)raw_ptr, 1,
                           hv.layout().total_nr_elems() * hv.dtype().size());
                    in->copy_from(hv);
                    parser.inputs[i.first] = std::move(hv);
                } else {
                    in->copy_from(i.second);
                }
            }
        }
    }
}

template <>
void IOdumpOption::config_model_internel<ModelLite>(
        RuntimeParam& runtime_param, std::shared_ptr<ModelLite> model) {
    if (runtime_param.stage == RunStage::AFTER_MODEL_LOAD) {
        if (enable_io_dump) {
            LITE_LOG("enable text io dump");
            lite::Runtime::enable_io_txt_dump(model->get_lite_network(), dump_path);
        }
        if (enable_bin_io_dump) {
            LITE_LOG("enable binary io dump");
            lite::Runtime::enable_io_bin_dump(model->get_lite_network(), dump_path);
        }
        //! FIX:when add API in lite complate this
        if (enable_io_dump_stdout || enable_io_dump_stderr) {
            LITE_THROW("lite model don't support the stdout or stderr io dump");
        }
        if (enable_bin_out_dump) {
            LITE_THROW("lite model don't support the binary output dump");
        }
        if (enable_copy_to_host) {
            LITE_LOG("lite model set copy to host defaultly");
        }
    }
}

template <>
void IOdumpOption::config_model_internel<ModelMdl>(
        RuntimeParam& runtime_param, std::shared_ptr<ModelMdl> model) {
    if (runtime_param.stage == RunStage::BEFORE_MODEL_LOAD) {
        if (enable_io_dump) {
            mgb_log("enable text io dump");
            auto iodump = std::make_unique<mgb::TextOprIODump>(
                    model->get_mdl_config().comp_graph.get(), dump_path.c_str());
            iodump->print_addr(false);
            io_dumper = std::move(iodump);
        }

        if (enable_io_dump_stdout) {
            mgb_log("enable text io dump to stdout");
            std::shared_ptr<FILE> std_out(stdout, [](FILE*) {});
            auto iodump = std::make_unique<mgb::TextOprIODump>(
                    model->get_mdl_config().comp_graph.get(), std_out);
            iodump->print_addr(false);
            io_dumper = std::move(iodump);
        }

        if (enable_io_dump_stderr) {
            mgb_log("enable text io dump to stderr");
            std::shared_ptr<FILE> std_err(stderr, [](FILE*) {});
            auto iodump = std::make_unique<mgb::TextOprIODump>(
                    model->get_mdl_config().comp_graph.get(), std_err);
            iodump->print_addr(false);
            io_dumper = std::move(iodump);
        }

        if (enable_bin_io_dump) {
            mgb_log("enable binary io dump");
            auto iodump = std::make_unique<mgb::BinaryOprIODump>(
                    model->get_mdl_config().comp_graph.get(), dump_path);
            io_dumper = std::move(iodump);
        }

        if (enable_bin_out_dump) {
            mgb_log("enable binary output dump");
            out_dumper = std::make_unique<OutputDumper>(dump_path.c_str());
        }
    } else if (runtime_param.stage == RunStage::AFTER_MODEL_LOAD) {
        if (enable_bin_out_dump) {
            auto&& load_result = model->get_mdl_load_result();
            out_dumper->set(load_result.output_var_list);

            std::vector<mgb::ComputingGraph::Callback> cb;
            for (size_t i = 0; i < load_result.output_var_list.size(); i++) {
                cb.push_back(out_dumper->bind());
            }
            model->set_output_callback(cb);
        }
        if (enable_copy_to_host) {
            auto&& load_result = model->get_mdl_load_result();

            std::vector<mgb::ComputingGraph::Callback> cb;
            for (size_t i = 0; i < load_result.output_var_list.size(); i++) {
                mgb::HostTensorND val;
                auto callback = [val](const mgb::DeviceTensorND& dv) mutable {
                    val.copy_from(dv);
                };
                cb.push_back(callback);
            }
            model->set_output_callback(cb);
        }
    } else if (runtime_param.stage == RunStage::AFTER_RUNNING_WAIT) {
        if (enable_bin_out_dump) {
            out_dumper->write_to_file();
        }
    }
}

}  // namespace lar

////////////////////// Input options ////////////////////////
using namespace lar;

void InputOption::update() {
    data_path.clear();
    m_option_name = "input";
    size_t start = 0;
    auto end = FLAGS_input.find(";", start);
    while (end != std::string::npos) {
        std::string path = FLAGS_input.substr(start, end - start);
        data_path.emplace_back(path);
        start = end + 1;
        end = FLAGS_input.find(";", start);
    }
    data_path.emplace_back(FLAGS_input.substr(start));
    m_force_batch_size = FLAGS_batch_size;
}

std::shared_ptr<lar::OptionBase> lar::InputOption::create_option() {
    static std::shared_ptr<InputOption> option(new InputOption);
    if (InputOption::is_valid()) {
        option->update();
        return std::static_pointer_cast<OptionBase>(option);
    } else {
        return nullptr;
    }
}

void InputOption::config_model(
        RuntimeParam& runtime_param, std::shared_ptr<ModelBase> model) {
    CONFIG_MODEL_FUN;
}

////////////////////// OprIOdump options ////////////////////////

void IOdumpOption::update() {
    m_option_name = "iodump";
    size_t valid_flag = 0;
    if (!FLAGS_io_dump.empty()) {
        dump_path = FLAGS_io_dump;
        enable_io_dump = true;
        valid_flag = valid_flag | (1 << 0);
    }
    if (!FLAGS_bin_io_dump.empty()) {
        dump_path = FLAGS_bin_io_dump;
        enable_bin_io_dump = true;
        valid_flag = valid_flag | (1 << 1);
    }
    if (!FLAGS_bin_out_dump.empty()) {
        dump_path = FLAGS_bin_out_dump;
        enable_bin_out_dump = true;
        valid_flag = valid_flag | (1 << 2);
    }
    if (FLAGS_io_dump_stdout) {
        enable_io_dump_stdout = FLAGS_io_dump_stdout;
        valid_flag = valid_flag | (1 << 3);
    }
    if (FLAGS_io_dump_stderr) {
        enable_io_dump_stderr = FLAGS_io_dump_stderr;
        valid_flag = valid_flag | (1 << 4);
    }
    // not only one dump set valid
    if (valid_flag && (valid_flag & (valid_flag - 1))) {
        mgb_log_warn(
                "ONLY the last io dump option is validate and others is "
                "skipped!!!");
    }

    enable_copy_to_host = FLAGS_copy_to_host;
}

bool IOdumpOption::is_valid() {
    bool ret = !FLAGS_io_dump.empty();
    ret = ret || FLAGS_io_dump_stdout;
    ret = ret || FLAGS_io_dump_stderr;
    ret = ret || !FLAGS_bin_io_dump.empty();
    ret = ret || !FLAGS_bin_out_dump.empty();
    ret = ret || FLAGS_copy_to_host;
    return ret;
}

std::shared_ptr<OptionBase> IOdumpOption::create_option() {
    static std::shared_ptr<IOdumpOption> option(new IOdumpOption);
    if (IOdumpOption::is_valid()) {
        option->update();
        return std::static_pointer_cast<OptionBase>(option);
    } else {
        return nullptr;
    }
}

void IOdumpOption::config_model(
        RuntimeParam& runtime_param, std::shared_ptr<ModelBase> model) {
    CONFIG_MODEL_FUN;
}
////////////////////// Input gflags ////////////////////////
DEFINE_string(
        input, "", "Set up inputs data for model --input [ file_path | data_string]");
DEFINE_int32(
        batch_size, -1,
        "set the batch size of input(especially for global layout transform "
        "optimization working on)");
////////////////////// OprIOdump gflags ////////////////////////

DEFINE_string(io_dump, "", "set the io dump file path in text format");
DEFINE_bool(io_dump_stdout, false, "dump io opr to stdout in text format");
DEFINE_bool(io_dump_stderr, false, "dump io opr to stderr in text format");
DEFINE_string(
        bin_io_dump, "",
        "set the io dump directory path where variable in binary format located");
DEFINE_string(
        bin_out_dump, "",
        "set the out dump directory path where output variable in binary format "
        "located");
DEFINE_bool(copy_to_host, false, "copy device data to host");

REGIST_OPTION_CREATOR(input, lar::InputOption::create_option);

REGIST_OPTION_CREATOR(iodump, lar::IOdumpOption::create_option);
