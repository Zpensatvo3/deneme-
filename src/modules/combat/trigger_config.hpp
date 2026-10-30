#pragma once
#include "trigger_engine.hpp"
#include "../core/setting_schema.hpp"

namespace trigger_engine
{
    inline std::span<const modules::settings::descriptor<config>> schema()
    {
        using d = modules::settings::descriptor<config>;
        using label = modules::settings::label;
        static constexpr label targets[]{{"Players"},{"Players + Mobs"},{"All Living"}};
        static constexpr label weapons[]{{"Any Weapon"},{"Sword"},{"Axe"}};
        static constexpr label criticals[]{{"Off"},{"Smart"},{"Criticals Only"}};
        static const d fields[]{
            d::real_pair("cooldown", {"Cooldown"}, &config::cooldown, modules::catalog::triggerbot_cooldown_minimum, modules::catalog::triggerbot_cooldown_maximum, "%.0f%%").scaled(100.f),
            d::integer_pair("reaction", {"Reaction"}, &config::reaction, modules::catalog::triggerbot_reaction_minimum, modules::catalog::triggerbot_reaction_maximum, "%d ms"),
            d::enumeration<&config::targets>("targets", {"Targets"}, targets),
            d::enumeration<&config::weapon>("weapon", {"Starting Weapon"}, weapons),
            d::enumeration<&config::criticals>("criticals", {"Criticals"}, criticals),
            d::toggle("on_click", {"On Click Only"}, &config::on_click),
            d::toggle("swing_hand", {"Swing Hand"}, &config::swing_hand),
            d::toggle("click_simulation", {"Click Simulation"}, &config::click_simulation)
        };
        return fields;
    }
    inline bool visit_config(modules::config_visitor& v, config& live)
    {
        config candidate = live;
        const bool importing = v.importing();
        if (importing) v.stage_values(true);
        modules::settings::visit_fields(v, candidate, schema());
        int targets = static_cast<int>(candidate.targets), weapon = static_cast<int>(candidate.weapon);
        int criticals = static_cast<int>(candidate.criticals);
        if (importing) {
            if (!v.has_field("cooldown")) {
                v.field("min_cooldown", candidate.cooldown[0]); v.field("max_cooldown", candidate.cooldown[1]);
            }
            if (!v.has_field("reaction")) {
                v.field("reaction_delay_min", candidate.reaction[0]); v.field("reaction_delay_max", candidate.reaction[1]);
            }
            if (!v.has_field("weapon") && (v.has_field("sword_only") || v.has_field("axe_only"))) {
                bool sword = false, axe = false;
                v.field("sword_only", sword); v.field("axe_only", axe);
                if (sword && axe) v.reject("Triggerbot: sword_only and axe_only cannot both be enabled.");
                weapon = sword ? 1 : axe ? 2 : 0;
            }
            if (!v.has_field("targets") && (v.has_field("players") || v.has_field("mobs") || v.has_field("animals"))) {
                bool players = true, mobs = targets >= 1, animals = targets == 2;
                v.field("players", players); v.field("mobs", mobs); v.field("animals", animals);
                if (!players || (animals && !mobs)) v.reject("Triggerbot: legacy target selection cannot be represented safely. Choose Players, Players + Mobs or All Living.");
                targets = animals ? 2 : mobs ? 1 : 0;
            }
            if (!v.has_field("criticals") && v.has_field("wait_critical")) {
                bool only = criticals == 2; v.field("wait_critical", only); criticals = only ? 2 : 0;
            }
            bool priority = false;
            if (v.has_field("prioritize_crits")) v.field("prioritize_crits", priority);
            if (priority && criticals == 0) criticals = 1;
            if (!v.has_field("on_click")) v.field("on_left_click", candidate.on_click);
            candidate.targets = static_cast<target_mode>(targets); candidate.weapon = static_cast<weapon_mode>(weapon);
            candidate.criticals = static_cast<critical_mode>(criticals);
            if (!normalize(candidate)) v.reject("Triggerbot: invalid ranges or enum values.");
            v.stage_values(false);
            if (v.reading() && v.valid()) {
                live = candidate; return true; // Even an identical import invalidates an in-flight cycle.
            }
        }
        return false;
    }
}
