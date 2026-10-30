#pragma once

#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct sprint
    {
        bool stop_while_using_item = modules::catalog::sprint_stop_while_using_item_default;
        bool operator==(const sprint&) const = default;

        static std::span<const modules::settings::descriptor<sprint>> schema()
        {
            using model = sprint;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::toggle("stop_while_using_item", {"Stop While Using Items"}, &model::stop_while_using_item),
            };
            return fields;
        }
    };
}
