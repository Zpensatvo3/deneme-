#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    inline constexpr bool anti_bot_default_enabled = modules::catalog::anti_bot_enabled_default;
    struct anti_bot
    {
        bool check_player_list = modules::catalog::anti_bot_check_player_list_default;
        int spawn_grace_ticks = modules::catalog::anti_bot_spawn_grace_ticks_default;
        int missing_list_ticks = modules::catalog::anti_bot_missing_list_ticks_default;
        bool operator==(const anti_bot&) const = default;
        static std::span<const modules::settings::descriptor<anti_bot>> schema()
        {
            using d = modules::settings::descriptor<anti_bot>;
            static const d fields[]{
                d::toggle("check_player_list", {"Player List Check"}, &anti_bot::check_player_list),
                d::integer("spawn_grace_ticks", {"Spawn Grace"}, &anti_bot::spawn_grace_ticks, modules::catalog::anti_bot_spawn_grace_ticks_minimum, modules::catalog::anti_bot_spawn_grace_ticks_maximum, "%d ticks"),
                d::integer("missing_list_ticks", {"Missing List Delay"}, &anti_bot::missing_list_ticks, modules::catalog::anti_bot_missing_list_ticks_minimum, modules::catalog::anti_bot_missing_list_ticks_maximum, "%d ticks")
            };
            return fields;
        }
    };
}
