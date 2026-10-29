#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/auto_totem_settings.hpp"

namespace modules
{
class auto_totem : public module
	{
	public:
		auto_totem() : module("auto totem", "Player", "auto_totem") {}

        void run(::cache&) override {}
        void render_overlay() override;
        void render_options() override;
        void on_enable(::cache&) override { generation_.fetch_add(1, std::memory_order_release); lifecycle_reset_.store(true, std::memory_order_release); }
        void on_disable(::cache&) override { generation_.fetch_add(1, std::memory_order_release); lifecycle_reset_.store(true, std::memory_order_release); }
        void on_render_shutdown() override { cancel_on_client_thread(); }
        void cancel_on_client_thread();
        bool render_when_disabled() const override { return true; }
        float get_options_height() override { return 420.f; }
        void visit_config(config_visitor& v) override
        {
            if (settings_.visit(v, module_settings::auto_totem::schema()))
                generation_.fetch_add(1, std::memory_order_release);
        }
    private:
        settings::store<module_settings::auto_totem> settings_;
        std::atomic<std::uint64_t> generation_{1};
        std::atomic_bool lifecycle_reset_{false};
    };
}
