#pragma once

#include "../../mappings.hpp"

namespace services::game
{
    struct client_context
    {
        maps::MinecraftClient instance{};
        maps::ClientPlayerEntity player{};
        maps::ClientWorld world{};
        maps::ClientPlayerInteractionManager interaction{};
        maps::Screen screen{};
    };

    // Captures all wrappers from one MinecraftClient snapshot. This prevents a
    // module from accidentally mixing player/world objects from adjacent ticks.
    bool capture_client_context(client_context& output,
        bool require_world = true,
        bool require_interaction = true);
}
