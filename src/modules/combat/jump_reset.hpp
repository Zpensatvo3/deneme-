#pragma once

#include "../module_support.hpp"
#include "jump_reset_config.hpp"

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
        void visit_config(config_visitor& v) override
        {
            auto candidate = settings_snapshot();
            if (jump_reset_config::visit_config(v, candidate)) apply_settings(candidate, true);
        }

    private:
        jump_reset_config::config settings{};
        std::mutex settings_mutex;
        std::atomic<std::uint64_t> reset_generation{1};
        jump_reset_config::config settings_snapshot();
        void apply_settings(jump_reset_config::config candidate, bool force_reset = false);
        void request_reset();
    };
}
