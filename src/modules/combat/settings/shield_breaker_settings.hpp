#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct shield_breaker
    {
        int switch_delay = 0;
        int return_delay = 110;
        int config_axe_slot = 1;
        bool operator==(const shield_breaker&) const = default;
        static std::span<const modules::settings::descriptor<shield_breaker>> schema()
        {
            using model = shield_breaker;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::integer("axe_slot", {"Preferred Axe Slot"}, &model::config_axe_slot, 1, 9, "Slot %d"),
                setting::integer("return_delay", {"Return Delay"}, &model::return_delay, 75, 500, "%d ms"),
                setting::integer("switch_delay", {"switch_delay"}, &model::switch_delay, -1000000, 1000000, "%d").hidden(),
            };
            return fields;
        }
    };
}
