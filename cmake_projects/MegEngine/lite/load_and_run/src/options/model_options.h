#pragma once
#include <gflags/gflags.h>
#include "megbrain/graph/operator_node.h"
#include "models/model.h"
#include "option_base.h"
DECLARE_string(model_dump);
DECLARE_string(packed_model_dump);
DECLARE_string(pack_info_json);
DECLARE_string(pack_cache);
DECLARE_string(pack_info_cryption);
DECLARE_string(pack_model_cryption);

namespace lar {
class PackModelOption : public OptionBase {
public:
    static bool is_valid();
    static std::shared_ptr<OptionBase> create_option();
    void config_model(
            RuntimeParam& runtime_param, std::shared_ptr<ModelBase> model) override;
    std::string option_name() const override { return m_option_name; }

    void update() override;

private:
    PackModelOption() = default;

    template <typename ModelImpl>
    void config_model_internel(RuntimeParam&, std::shared_ptr<ModelImpl>);

    std::string m_option_name;
    std::string packed_model_dump;
    std::string pack_info_json;
    std::string pack_cache;
    std::string pack_binary_cache;
    std::string pack_info_cryption;
    std::string pack_model_cryption;
    bool is_fast_run_cache = true;
};

class RawModelOption : public OptionBase {
public:
    static bool is_valid();
    static std::shared_ptr<OptionBase> create_option();
    void config_model(
            RuntimeParam& runtime_param, std::shared_ptr<ModelBase> model) override;
    std::string option_name() const override { return m_option_name; }

private:
    RawModelOption();

    template <typename ModelImpl>
    void config_model_internel(RuntimeParam&, std::shared_ptr<ModelImpl>);

    std::string m_option_name;
    std::string model_dump;
};
}  // namespace lar
