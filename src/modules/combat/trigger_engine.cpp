#include "trigger_engine.hpp"
#include <algorithm>
#include <cmath>

bool trigger_engine::normalize(config& value)
{
    if (!std::isfinite(value.cooldown[0]) || !std::isfinite(value.cooldown[1]) ||
        static_cast<int>(value.targets) < 0 || static_cast<int>(value.targets) > 2 ||
        static_cast<int>(value.weapon) < 0 || static_cast<int>(value.weapon) > 2 ||
        static_cast<int>(value.criticals) < 0 || static_cast<int>(value.criticals) > 2) return false;
    if (value.cooldown[0] > value.cooldown[1]) std::swap(value.cooldown[0], value.cooldown[1]);
    if (value.reaction[0] > value.reaction[1]) std::swap(value.reaction[0], value.reaction[1]);
    for (auto& item : value.cooldown) item = std::clamp(item, .5f, 1.f);
    for (auto& item : value.reaction) item = std::clamp(item, 0, 150);
    return true;
}
bool trigger_engine::same_config(const config& a, const config& b)
{
    return a.cooldown[0] == b.cooldown[0] && a.cooldown[1] == b.cooldown[1] &&
        a.reaction[0] == b.reaction[0] && a.reaction[1] == b.reaction[1] &&
        a.targets == b.targets && a.weapon == b.weapon && a.criticals == b.criticals && a.on_click == b.on_click && a.swing_hand == b.swing_hand &&
        a.click_simulation == b.click_simulation;
}
