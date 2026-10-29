#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/sprint_settings.hpp"

namespace modules
{
class sprint : public module
	{
	public:
		sprint() : module("sprint", "Movement") {}

		void run(::cache& cache) override;
		void render_options() override;
		void on_disable(::cache& cache) override;
        void render_overlay() override;
        void on_render_shutdown() override;
        bool render_when_disabled() const override { return true; }
		float get_options_height() override { return 115.0f; }
		void visit_config(config_visitor& v) override { settings_.visit(v, module_settings::sprint::schema()); }

	private:
		modules::settings::store<module_settings::sprint> settings_;
        std::atomic_bool reset_requested_{false};
	};
}
