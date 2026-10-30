#include "../src/modules/core/config_fields.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace modules
{
    class config_fixture
    {
    public:
        explicit config_fixture(const char*) {}
        virtual ~config_fixture() = default;
        virtual void visit_config(config_visitor&) {}
        std::string export_config_settings() const {
            return serialize_config_fields([&](config_visitor& visitor) { const_cast<config_fixture&>(*this).visit_config(visitor); });
        }
        bool apply_config_settings(const std::string& object) {
            return deserialize_config_fields([&](config_visitor& visitor) { visit_config(visitor); }, object);
        }
        bool validate_config_settings(const std::string& object) const {
            return validate_config_fields([&](config_visitor& visitor) { const_cast<config_fixture&>(*this).visit_config(visitor); }, object);
        }
        std::string validation_error(const std::string& object) {
            return config_validation_error([&](config_visitor& visitor) { visit_config(visitor); }, object);
        }
    };
}

class sample_module final : public modules::config_fixture
{
public:
    sample_module() : config_fixture("sample") {}

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
