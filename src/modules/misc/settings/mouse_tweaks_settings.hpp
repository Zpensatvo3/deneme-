#pragma once

#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct mouse_tweaks
    {
        bool right_click_tweak = true;
        bool left_click_with_item = true;
        bool left_click_without_item = true;
        bool operator==(const mouse_tweaks&) const = default;

        static std::span<const modules::settings::descriptor<mouse_tweaks>> schema()
        {
            using model = mouse_tweaks;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::toggle("right_click_tweak", {"RMB Drag Distribution"}, &model::right_click_tweak),
                setting::toggle("left_click_with_item", {"LMB Collect Matching Stacks"}, &model::left_click_with_item),
                setting::toggle("left_click_without_item", {"Shift + LMB Quick Move"}, &model::left_click_without_item),
            };
            return fields;
        }
    };
}
