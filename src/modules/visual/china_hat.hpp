#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/china_hat_settings.hpp"

namespace modules
{
    class china_hat final : public module
    {
    public:
        china_hat() : module("china hat", "Visual", "china_hat") {}
        void run(::cache&) override {}
        void render_overlay() override;
        void render_options() override;
        float get_options_height() override { return 366.f; }
    private:
modules::settings::store<module_settings::china_hat> settings_{"china_hat"};
    };
}
