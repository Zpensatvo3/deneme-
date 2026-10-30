#pragma once

#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct fast_place
    {
        int delay = modules::catalog::fast_place_delay_default;
        bool operator==(const fast_place&) const = default;

        static std::span<const modules::settings::descriptor<fast_place>> schema()
        {
            using model = fast_place;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::integer("delay", {"Place Delay"}, &model::delay, modules::catalog::fast_place_delay_minimum, modules::catalog::fast_place_delay_maximum, "%d ticks"),
            };
            return fields;
        }
    };
}
