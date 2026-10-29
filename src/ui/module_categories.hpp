#pragma once
#include <string_view>

namespace gui::module_categories
{
    // Presentation only. Stable module/config categories are not rewritten.
    constexpr std::string_view display(std::string_view category)
    {
        if (category == "Movement" || category == "Player" || category == "World" || category == "Misc") return "Misc";
        if (category == "Visual" || category == "Visuals") return "Visuals";
        if (category == "Render" || category == "Hud") return "Hud";
        if (category == "Friends") return {}; // Separate management page, not a feature category.
        return category;
    }
}
