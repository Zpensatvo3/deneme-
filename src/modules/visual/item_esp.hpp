#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/item_esp_settings.hpp"

namespace modules
{
    class item_esp final : public module
    {
    public:
        item_esp() : module("item esp", "Visual", "item_esp") {}
        // No worker snapshots or cached Java entities: every frame uses the
        // current world's live item list, so disconnects cannot leave ghosts.
        void run(::cache&) override {}
        void render_overlay() override;
        void render_options() override;
        float get_options_height() override { return 340.f; }
        void visit_config(config_visitor& v) override { settings_.visit(v, module_settings::item_esp::schema()); }
    private:
        modules::settings::store<module_settings::item_esp> settings_;
    };
}
