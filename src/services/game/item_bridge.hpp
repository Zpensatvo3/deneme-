#pragma once
#include "hybrid_bridge.hpp"
#include "../../platform/adapter/target_version.hpp"

namespace services::game::items
{
    struct exp_snapshot
    {
        jobjectArray packet = nullptr;
        jobject player = nullptr, world = nullptr;
        bool holding_exp = false;
    };
    hybrid::result capture_exp(exp_snapshot& out);
    bool use_exp(const exp_snapshot& observed);
    void shutdown(); // After render callbacks have joined.
}
