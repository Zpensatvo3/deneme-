#pragma once

#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct pearl_macro
    {
        int switch_delay = 25;
        int return_delay = 75;
        bool operator==(const pearl_macro&) const = default;

        static std::span<const modules::settings::descriptor<pearl_macro>> schema()
        {
            using model = pearl_macro;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::integer("switch_delay", {"Switch Delay"}, &model::switch_delay, 0, 150, "%d ms"),
                setting::integer("return_delay", {"Return Delay"}, &model::return_delay, 0, 250, "%d ms"),
            };
            return fields;
        }
    };
}
