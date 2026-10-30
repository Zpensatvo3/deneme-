#pragma once

#include "../../mappings.hpp"

namespace services::target
{
    maps::Entity crosshair_entity(const maps::MinecraftClient& instance);
    maps::Entity living_crosshair_target(const maps::MinecraftClient& instance,
        bool ignore_friends = true);
    maps::Entity blocking_player_crosshair_target(
        const maps::MinecraftClient& instance,
        const maps::ClientPlayerEntity& player,
        double maximum_distance = 5.0,
        bool ignore_friends = true);
}
