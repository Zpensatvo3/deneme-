#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/anti_bot_settings.hpp"

namespace modules
{
    class anti_bot final : public module
    {
    public:
        anti_bot();
        ~anti_bot() override;
        void run(::cache& cache) override;
        void on_disable(::cache&) override;
        void render_options() override;
        float get_options_height() override { return 220.f; }
        void visit_config(config_visitor& v) override { settings_.visit(v, module_settings::anti_bot::schema()); }
    private:
        modules::settings::store<module_settings::anti_bot> settings_;
    };
}
