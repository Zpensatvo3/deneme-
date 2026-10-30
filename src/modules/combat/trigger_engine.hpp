#pragma once
#include "../core/setting_schema.hpp"
// Shared Triggerbot settings; gameplay decisions live in WorldBridge.java.
namespace trigger_engine
{
    enum class target_mode { players, players_and_mobs, all_living };
    enum class weapon_mode { any, sword, axe };
    enum class critical_mode { off, smart, only };
    struct config
    {
        float cooldown[2]{ modules::catalog::triggerbot_cooldown_default[0], modules::catalog::triggerbot_cooldown_default[1] };
        int reaction[2]{ modules::catalog::triggerbot_reaction_default[0], modules::catalog::triggerbot_reaction_default[1] };
        target_mode targets = static_cast<target_mode>(modules::catalog::triggerbot_targets_default);
        weapon_mode weapon = static_cast<weapon_mode>(modules::catalog::triggerbot_weapon_default);
        critical_mode criticals = static_cast<critical_mode>(modules::catalog::triggerbot_criticals_default);
        bool on_click = modules::catalog::triggerbot_on_click_default;
        bool swing_hand = modules::catalog::triggerbot_swing_hand_default;
        bool click_simulation = modules::catalog::triggerbot_click_simulation_default;
    };
    bool normalize(config& value);
    bool same_config(const config& a, const config& b);
}
