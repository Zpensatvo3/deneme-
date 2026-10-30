#pragma once

#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct pearl_macro
    {
        int switch_delay = modules::catalog::pearl_macro_switch_delay_default;
        int return_delay = modules::catalog::pearl_macro_return_delay_default;
        bool operator==(const pearl_macro&) const = default;

        static std::span<const modules::settings::descriptor<pearl_macro>> schema()
        {
            using model = pearl_macro;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::integer("switch_delay", {"Switch Delay"}, &model::switch_delay, modules::catalog::pearl_macro_switch_delay_minimum, modules::catalog::pearl_macro_switch_delay_maximum, "%d ms"),
                setting::integer("return_delay", {"Return Delay"}, &model::return_delay, modules::catalog::pearl_macro_return_delay_minimum, modules::catalog::pearl_macro_return_delay_maximum, "%d ms"),
            };
            return fields;
        }
    };
}
