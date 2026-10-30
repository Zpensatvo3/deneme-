#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "cosmetic_renderer.hpp"
#include <atomic>

namespace modules
{
    class trails final : public module
    {
    public:
        trails() : module("trails","Visual","trails") {}
        void run(::cache&) override {}
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void on_render_shutdown() override { state_.clear(); }
        void on_enable(::cache&) override { reset_generation_.fetch_add(1,std::memory_order_release); }
        void on_disable(::cache&) override { reset_generation_.fetch_add(1,std::memory_order_release); }
        void render_options() override;
        float get_options_height() override { return 380.f; }
    private:
modules::settings::store<module_settings::trails> settings_{"trails"};
        cosmetics::trail_state state_;
        std::atomic<unsigned> reset_generation_{0};
        unsigned applied_generation_ = 0;
    };
}
