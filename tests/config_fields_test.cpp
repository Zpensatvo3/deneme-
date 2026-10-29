#include "../src/modules/core/module.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace modules
{
    module::module(const char* module_name, const char* module_category, const char*)
        : id_storage("config_test"), name(module_name), category(module_category) {}
    void module::run(::cache&) {}
    void module::render_options() {}
    void module::on_enable(::cache&) {}
    void module::on_disable(::cache&) {}
    std::string module::export_config_settings() const
    {
        return serialize_config_fields(const_cast<module&>(*this));
    }
    bool module::apply_config_settings(const std::string& object)
    {
        return deserialize_config_fields(*this, object);
    }
    bool module::validate_config_settings(const std::string& object) const
    {
        return validate_config_fields(const_cast<module&>(*this), object);
    }
}

class sample_module final : public modules::module
{
public:
    sample_module() : module("sample") {}

    void visit_config(modules::config_visitor& visitor) override
    {
        visitor.field("enabled_option", enabled_option);
        visitor.field("count", count);
        visitor.field("strength", strength);
        visitor.field("range", range);
        visitor.field("color", color);
        visitor.field("blocks", blocks);
    }

    void change()
    {
        enabled_option = false;
        count = 99;
        strength = 9.0f;
        range[0] = range[1] = -1;
        for (float& channel : color) channel = 0.0f;
        blocks.clear();
    }

    bool restored() const
    {
        return enabled_option && count == 7 && std::abs(strength - 0.625f) < 0.0001f &&
            range[0] == 2 && range[1] == 8 && std::abs(color[2] - 0.75f) < 0.0001f &&
            blocks == std::vector<std::string>{ "minecraft:diamond_ore", "minecraft:ancient_debris" };
    }

private:
    bool enabled_option = true;
    int count = 7;
    float strength = 0.625f;
    int range[2] = { 2, 8 };
    float color[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
    std::vector<std::string> blocks{ "minecraft:diamond_ore", "minecraft:ancient_debris" };
};

int main()
{
    sample_module sample;
    const std::string payload = sample.export_config_settings();
    sample.change();
    if (!sample.apply_config_settings(payload) || !sample.restored())
    {
        std::cerr << "round-trip failed: " << payload << '\n';
        return 1;
    }
    if (sample.apply_config_settings("{\"count\":\"invalid\"}"))
    {
        std::cerr << "invalid payload accepted\n";
        return 2;
    }
    std::cout << payload << '\n';
    return 0;
}
