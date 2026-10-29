#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/fast_exp_settings.hpp"

namespace modules
{
    class fast_exp : public module
    {
    public:
        fast_exp() : module("fast exp", "World") {}
        void run(::cache&) override;
        void render_overlay() override;
        void render_options() override;
        bool render_when_disabled() const override { return true; }
        float get_options_height() override { return 140.0f; }
        void visit_config(config_visitor& visitor) override
        { settings_.visit(visitor, module_settings::fast_exp::schema()); }

    private:
        modules::settings::store<module_settings::fast_exp> settings_;
    };
}
