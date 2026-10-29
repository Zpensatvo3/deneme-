#define main existing_config_fields_round_trip_test
#include "config_fields_test.cpp"
#undef main
#include "../src/modules/misc/sprint.hpp"
#include "../src/modules/misc/fast_place.hpp"
#include "../src/modules/misc/pearl_macro.hpp"
#include "../src/modules/misc/mouse_tweaks.hpp"
#include "../src/modules/misc/pot_refill.hpp"
#include <cassert>
#include <set>

template<class Model>
class settings_fixture final : public modules::module
{
public:
    settings_fixture() : module("utility_fixture") {}
    modules::settings::store<Model> settings;
    void visit_config(modules::config_visitor& visitor) override
    {
        settings.visit(visitor, Model::schema());
    }
};

template<class Model>
void check_contract(std::initializer_list<const char*> expected)
{
    settings_fixture<Model> owner;
    const Model initial;
    const auto encoded = owner.export_config_settings();
    assert(owner.validate_config_settings(encoded) && owner.settings.read() == initial);
    assert(owner.apply_config_settings(encoded) && owner.settings.read() == initial);
    assert(owner.apply_config_settings("{}") && owner.settings.read() == initial);
    std::set<std::string> keys;
    for (const auto& d : Model::schema())
    {
        assert(keys.insert(d.key).second);
        assert(encoded.find(std::string("\"") + d.key + "\":") != std::string::npos);
        const auto saved = owner.export_config_settings();
        const std::string invalid = std::string("{\"") + d.key + "\":\"bad\"}";
        assert(!owner.validate_config_settings(invalid));
        assert(!owner.apply_config_settings(invalid));
        assert(owner.export_config_settings() == saved);
        if (d.type == modules::settings::control::integer)
        {
            for (const char* value : {"0.5", "2147483648", "-2147483649", "1e999"})
            {
                assert(!owner.apply_config_settings(std::string("{\"") + d.key + "\":" + value + "}"));
                assert(owner.export_config_settings() == saved);
            }
        }
    }
    assert(keys == std::set<std::string>(expected.begin(), expected.end()));
}

int main()
{
    using namespace module_settings;
    check_contract<sprint>({"stop_while_using_item"});
    check_contract<fast_place>({"delay"});
    check_contract<pearl_macro>({"switch_delay", "return_delay"});
    check_contract<mouse_tweaks>({"right_click_tweak", "left_click_with_item", "left_click_without_item"});
    check_contract<pot_refill>({"delay_ms", "random_delay", "max_delay_ms", "slot_order"});

    assert(sprint{}.stop_while_using_item && fast_place{}.delay == 0);
    assert(pearl_macro{}.switch_delay == 25 && pearl_macro{}.return_delay == 75);
    assert(mouse_tweaks{}.right_click_tweak && mouse_tweaks{}.left_click_with_item && mouse_tweaks{}.left_click_without_item);
    assert(pot_refill{}.delay_ms == 100);
    assert(!pot_refill{}.random_delay && pot_refill{}.max_delay_ms == 160 && pot_refill{}.slot_order == 0);
    settings_fixture<pot_refill> refill;
    // Old configs retain their fixed speed, including delays above the new default maximum.
    assert(refill.apply_config_settings(R"({"delay_ms":700})"));
    assert(refill.settings.read().delay_ms == 700 && refill.settings.read().max_delay_ms == 700);
    assert(!refill.settings.read().random_delay && refill.settings.read().slot_order == 0);
    assert(refill.apply_config_settings(R"({"random_delay":true,"max_delay_ms":50,"slot_order":2})"));
    assert(refill.settings.read().max_delay_ms == 700 && refill.settings.read().slot_order == 2);
    const auto refill_before = refill.export_config_settings();
    assert(!refill.apply_config_settings(R"({"delay_ms":100,"slot_order":3})"));
    assert(refill.export_config_settings() == refill_before);

    settings_fixture<fast_place> place;
    assert(place.apply_config_settings(R"({"delay":1000000})") && place.settings.read().delay == 4);
    assert(place.apply_config_settings(R"({"delay":-1000000})") && place.settings.read().delay == 0);
    // Preserve the serializer's existing global numeric limit as well.
    assert(!place.apply_config_settings(R"({"delay":2147483647})") && place.settings.read().delay == 0);
    std::cout << "Utility schemas: defaults, stable keys, hidden fields, integer limits and atomic imports passed\n";
}
