#pragma once
#include <array>
#include <jni.h>

namespace services::game::hybrid
{
    enum class result { native_fallback, unavailable, ready };
    // Render/client thread only; Java owns eligibility and state mutation.
    void tick_no_jump_delay();
    // Call after renderer callbacks have joined, before the JVM session closes.
    void shutdown();
}
