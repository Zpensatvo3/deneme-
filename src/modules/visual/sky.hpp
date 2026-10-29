#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/sky_settings.hpp"
#include <atomic>
#include <memory>

namespace modules
{
    class sky final : public module
    {
    public:
        sky();
        ~sky() override;
        void run(::cache&) override {}
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void on_render_shutdown() override;
        void on_enable(::cache&) override { reset_generation_.fetch_add(1, std::memory_order_release); }
        void on_disable(::cache&) override { reset_generation_.fetch_add(1, std::memory_order_release); }
        void render_options() override;
        float get_options_height() override { return 350.f; }
        void visit_config(config_visitor& visitor) override { settings_.visit(visitor,module_settings::sky::schema()); }
    private:
        struct runtime;
        std::unique_ptr<runtime> state_;
        modules::settings::store<module_settings::sky> settings_;
        std::atomic<unsigned> reset_generation_{0};
    };
}
