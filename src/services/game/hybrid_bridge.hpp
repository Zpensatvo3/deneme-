#pragma once
#include <array>
#include <jni.h>

namespace services::game::hybrid
{
    enum class feature : jint { no_jump_delay = 1 };
    enum class result { native_fallback, unavailable, ready };
    struct snapshot
    {
        // Local JNI references, valid only inside the caller's JNI local frame.
        jobjectArray packet = nullptr;
        jobject player = nullptr;
        jobject world = nullptr;
        std::array<double, 11> data{};
        bool flag(unsigned bit) const { return (static_cast<unsigned>(data[2]) & bit) != 0; }
    };
    // Render/client thread only. A failed capture never falls back in that frame.
    result capture(feature operation, snapshot& out);
    bool apply(feature operation, const snapshot& observed);
    // Call after renderer callbacks have joined, before the JVM session closes.
    void shutdown();
}
