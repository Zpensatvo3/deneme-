#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct block_esp
    {
        float line_thickness = 1.6f;
        float fill_opacity = 0.08f;
        std::vector<std::string> selected_blocks = {
			"minecraft:diamond_ore", "minecraft:deepslate_diamond_ore",
			"minecraft:ancient_debris"
		};
        bool operator==(const block_esp&) const = default;
        static std::span<const modules::settings::descriptor<block_esp>> schema()
        {
            using model = block_esp;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::number("line_thickness", {"Line Thickness"}, &model::line_thickness, 0.5f, 3.0f, "%.1f"),
                setting::number("fill_opacity", {"Fill Opacity"}, &model::fill_opacity, 0.0f, 0.35f, "%.2f"),
                setting::string_list("selected_blocks", {"selected_blocks"}, &model::selected_blocks).hidden(),
            };
            return fields;
        }
    };
}
