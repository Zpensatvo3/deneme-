#pragma once
#include "../core/setting_schema.hpp"
#include <algorithm>

namespace jump_reset_config
{
    struct config
    {
        int mode = 0; // 0 = configured game jump binding, 1 = game Space binding (legacy System Space configs).
        int chance = 100, jump_delay_ms = 0;
        bool only_combat = true;
    };
    inline std::span<const modules::settings::descriptor<config>> schema()
    {
        using d = modules::settings::descriptor<config>;
        static constexpr modules::settings::label modes[]{
            {"Game Binding"}, {"Game Space"}
        };
        static const d fields[]{
            d::choice("mode", {"Jump Input"}, &config::mode, modes),
            d::integer("chance", {"Activation Chance"}, &config::chance, 0, 100, "%d%%"),
            d::integer("jump_delay_ms", {"Delay after Damage"}, &config::jump_delay_ms, 0, 100, "%d ms"),
            d::toggle("only_combat", {"Living Attacks Only"}, &config::only_combat)
        };
        return fields;
    }
    enum class profile { custom = 0, immediate = 1, balanced = 2, controlled = 3 };
    inline bool normalize(config& value)
    {
        if (value.mode < 0 || value.mode > 1) return false;
        value.chance = std::clamp(value.chance, 0, 100);
        value.jump_delay_ms = std::clamp(value.jump_delay_ms, 0, 100);
        return true;
    }
    inline bool same_config(const config& a, const config& b)
    {
        return a.mode == b.mode && a.chance == b.chance && a.jump_delay_ms == b.jump_delay_ms && a.only_combat == b.only_combat;
    }
    inline void apply_profile(config& value, profile selected)
    {
        // Profiles change timing/eligibility, never silently change input route.
        switch (selected) {
        case profile::immediate: value.chance = 100; value.jump_delay_ms = 0; break;
        case profile::balanced: value.chance = 85; value.jump_delay_ms = 15; break;
        case profile::controlled: value.chance = 65; value.jump_delay_ms = 35; break;
        default: return;
        }
        value.only_combat = true;
    }
    inline profile selected_profile(const config& value)
    {
        if (!value.only_combat) return profile::custom;
        if (value.chance == 100 && value.jump_delay_ms == 0) return profile::immediate;
        if (value.chance == 85 && value.jump_delay_ms == 15) return profile::balanced;
        if (value.chance == 65 && value.jump_delay_ms == 35) return profile::controlled;
        return profile::custom;
    }
    inline bool visit_config(modules::config_visitor& visitor, config& live)
    {
        config candidate = live;
        const bool importing = visitor.importing();
        if (importing) visitor.stage_values(true);
        modules::settings::visit_fields(visitor, candidate, schema());
        if (!importing) return false;
        if (!normalize(candidate)) visitor.reject("Jump Reset: invalid input mode.");
        visitor.stage_values(false);
        if (visitor.reading() && visitor.valid()) { live = candidate; return true; }
        return false;
    }
}
