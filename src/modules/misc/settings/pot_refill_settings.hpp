#pragma once

#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct pot_refill
    {
        int delay_ms = 100;
        bool random_delay = false;
        int max_delay_ms = 160;
        int slot_order = 0;
        bool operator==(const pot_refill&) const = default;

        static std::span<const modules::settings::descriptor<pot_refill>> schema()
        {
            using model = pot_refill;
            using setting = modules::settings::descriptor<model>;
            static const modules::settings::label orders[]{
                {"Left to Right"}, {"Right to Left"},
                {"Random"}
            };
            static const setting fields[]{
                setting::integer("delay_ms", {"Refill Delay"}, &model::delay_ms, 50, 1000, "%d ms"),
                setting::toggle("random_delay", {"Variable Delay"}, &model::random_delay),
                setting::integer("max_delay_ms", {"Maximum Delay"}, &model::max_delay_ms, 50, 1000, "%d ms", &model::delay_ms)
                    .when([](const model& s) { return s.random_delay; }),
                setting::choice("slot_order", {"Slot Order"}, &model::slot_order, orders),
            };
            return fields;
        }
    };
}
