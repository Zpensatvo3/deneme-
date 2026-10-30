#pragma once
#include "trigger_engine.hpp"
#include <atomic>
#include <cstdint>
namespace trigger_engine
{
    // Native IO cancellation only; settings and requested lifecycle are Java-owned.
    struct adapter_runtime {
        std::atomic<std::uint64_t> reset_generation{1};
    };
    void request_reset(adapter_runtime& state);
    void run_overlay(adapter_runtime& state, bool enabled);
    void render_options(config& settings, adapter_runtime& state, const char* id_suffix);
}
