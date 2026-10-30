#pragma once

#include "../module_support.hpp"
#include "jump_reset_config.hpp"
#include "../core/settings_store.hpp"

namespace modules
{
    class jump_reset : public module
    {
    public:
        jump_reset() : module("jump reset", "Combat") {}
        void run(::cache& cache) override;
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void on_enable(::cache& cache) override;
        void on_disable(::cache& cache) override;
        void cancel_on_client_thread();
        void on_render_shutdown() override { cancel_on_client_thread(); }
        void render_options() override;
        float get_options_height() override { return 320.0f; }
    private:
        settings::store<jump_reset_config::config> settings{"jump_reset", jump_reset_config::schema()};
        std::atomic<std::uint64_t> reset_generation{1};
        jump_reset_config::config settings_snapshot();
        void apply_settings(jump_reset_config::config candidate);
        void request_reset();
    };
}
