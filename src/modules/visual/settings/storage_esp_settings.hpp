#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct storage_esp
    {
        bool show_labels = true;
        bool show_distance = true;
        int box_style = 1;
        bool operator==(const storage_esp&) const = default;
        static std::span<const modules::settings::descriptor<storage_esp>> schema()
        {
            using model = storage_esp;
            using setting = modules::settings::descriptor<model>;
            static constexpr modules::settings::label box_style_labels[]{{"2D Box"},{"3D Box"}};
            static const setting fields[]{
                setting::choice("box_style", {"Box Style"}, &model::box_style, box_style_labels),
                setting::toggle("show_labels", {"Show Labels"}, &model::show_labels),
                setting::toggle("show_distance", {"Show Distance"}, &model::show_distance),
            };
            return fields;
        }
    };
}
