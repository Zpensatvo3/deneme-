#pragma once
#include <atomic>
#include <cstdint>
#include "hybrid_bridge.hpp"
#include "../../platform/adapter/target_version.hpp"

namespace trigger_engine { struct config; }
namespace jump_reset_config { struct config; }
namespace module_settings { struct auto_totem; }

namespace services::game::world_bridge
{
    struct pearl_snapshot { jobjectArray packet = nullptr; int selected = -1; bool owned = false; std::array<bool, 9> pearls{}; };
    hybrid::result capture_pearl(pearl_snapshot& out);
    bool begin_pearl(const pearl_snapshot& observed, int slot);
    bool use_pearl();
    bool release_pearl();
    void tick_trigger(const trigger_engine::config& settings, bool active, const std::atomic<std::uint64_t>& generation);
    void cancel_trigger();
    void tick_jump(const jump_reset_config::config& settings, bool active, const std::atomic<std::uint64_t>& generation);
    void cancel_jump();
    void tick_totem(const module_settings::auto_totem& settings, bool active,
        const std::atomic<std::uint64_t>& generation, std::uint64_t expected_generation);
    void cancel_totem();
    void shutdown();
}
