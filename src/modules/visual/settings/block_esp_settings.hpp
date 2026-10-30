#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct block_esp
    {
        float line_thickness = modules::catalog::block_esp_line_thickness_default;
        float fill_opacity = modules::catalog::block_esp_fill_opacity_default;
        std::vector<std::string> selected_blocks = modules::catalog::block_esp_selected_blocks_default;
        bool operator==(const block_esp&) const = default;
        static std::span<const modules::settings::descriptor<block_esp>> schema()
        {
            using model = block_esp;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::number("line_thickness", {"Line Thickness"}, &model::line_thickness, modules::catalog::block_esp_line_thickness_minimum, modules::catalog::block_esp_line_thickness_maximum, "%.1f"),
                setting::number("fill_opacity", {"Fill Opacity"}, &model::fill_opacity, modules::catalog::block_esp_fill_opacity_minimum, modules::catalog::block_esp_fill_opacity_maximum, "%.2f"),
                setting::string_list("selected_blocks", {"selected_blocks"}, &model::selected_blocks).hidden(),
            };
            return fields;
        }
    };
}
