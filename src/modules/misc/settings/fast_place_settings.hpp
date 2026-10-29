#pragma once

#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct fast_place
    {
        int delay = 0;
        bool operator==(const fast_place&) const = default;

        static std::span<const modules::settings::descriptor<fast_place>> schema()
        {
            using model = fast_place;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::integer("delay", {"Place Delay"}, &model::delay, 0, 4, "%d ticks"),
            };
            return fields;
        }
    };
}
