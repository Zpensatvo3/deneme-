#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "cosmetic_renderer.hpp"
#include <atomic>

namespace modules
{
    class cape final : public module
    {
    public:
        cape() : module("cape", "Visual", "cape") {}
        void run(::cache&) override {}
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void on_enable(::cache&) override { reset_generation_.fetch_add(1, std::memory_order_release); }
        void on_disable(::cache&) override { reset_generation_.fetch_add(1, std::memory_order_release); }
        void render_options() override;
        void on_render_shutdown() override { motion_.clear(); }
        float get_options_height() override { return 460.f; }
    private:
modules::settings::store<module_settings::cape> settings_{"cape"};
        cosmetics::cape_motion motion_;
        std::atomic<unsigned> reset_generation_{0};
        unsigned applied_generation_ = 0;
    };
}
