#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/radar_settings.hpp"

namespace modules
{
class radar : public module
	{
	public:
		radar() : module("radar", "Visual") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 490.0f; }
	private:
modules::settings::store<module_settings::radar> settings_{"radar"};
	};
}
