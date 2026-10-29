#pragma once
#include "trigger_engine.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>
namespace trigger_engine
{
    // Only the settings/reset transport lives in C++; gameplay state is Java-owned.
    struct adapter_runtime {
        std::atomic<std::uint64_t> reset_generation{1};
        std::mutex submission_mutex;
        std::uint64_t settings_revision = 0;
    };
    void request_reset(adapter_runtime& state);
    void run_overlay(const config& settings, adapter_runtime& state, bool enabled);
    void render_options(config& settings, adapter_runtime& state, const char* id_suffix);
}
