#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct shield_breaker
    {
        int switch_delay = modules::catalog::shield_breaker_switch_delay_default;
        int return_delay = modules::catalog::shield_breaker_return_delay_default;
        int config_axe_slot = modules::catalog::shield_breaker_axe_slot_default;
        bool operator==(const shield_breaker&) const = default;
        static std::span<const modules::settings::descriptor<shield_breaker>> schema()
        {
            using model = shield_breaker;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::integer("axe_slot", {"Preferred Axe Slot"}, &model::config_axe_slot, modules::catalog::shield_breaker_axe_slot_minimum, modules::catalog::shield_breaker_axe_slot_maximum, "Slot %d"),
                setting::integer("return_delay", {"Return Delay"}, &model::return_delay, modules::catalog::shield_breaker_return_delay_minimum, modules::catalog::shield_breaker_return_delay_maximum, "%d ms"),
                setting::integer("switch_delay", {"switch_delay"}, &model::switch_delay, modules::catalog::shield_breaker_switch_delay_minimum, modules::catalog::shield_breaker_switch_delay_maximum, "%d").hidden(),
            };
            return fields;
        }
    };
}
