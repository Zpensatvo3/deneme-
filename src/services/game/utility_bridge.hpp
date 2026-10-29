#pragma once
#include "hybrid_bridge.hpp"
#include "../../platform/adapter/target_version.hpp"

namespace services::game::utility
{
    enum class feature : jint { fast_place = 1, sprint = 2, full_bright = 3 };
    // Client/render thread only. Full Bright is visual, so menus/focus do not block it.
    hybrid::result update(feature operation, bool active, int value = 0, bool physical_use_held = false);
    bool release(feature operation);
    // After renderer cleanup/join, before the JVM session is closed.
    void shutdown();
}
