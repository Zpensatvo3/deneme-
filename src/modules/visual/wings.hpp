#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/wings_settings.hpp"
#include "cosmetic_renderer.hpp"
#include <atomic>

namespace modules
{
    class wings final : public module
    {
    public:
        wings() : module("wings", "Visual", "wings") {}
        void run(::cache&) override {}
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void on_enable(::cache&) override { reset_.fetch_add(1,std::memory_order_release); }
        void on_disable(::cache&) override { reset_.fetch_add(1,std::memory_order_release); }
        void on_render_shutdown() override { motion_.clear(); }
        void render_options() override;
        float get_options_height() override { return 460.f; }
        void visit_config(config_visitor& visitor) override { settings_.visit(visitor, module_settings::wings::schema()); }
    private:
        modules::settings::store<module_settings::wings> settings_;
        cosmetics::cape_motion motion_;
        std::atomic<unsigned> reset_{0};
        unsigned applied_=0;
    };
}
