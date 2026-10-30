#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/fast_place_settings.hpp"

namespace modules
{
class fast_place : public module
	{
	public:
		fast_place() : module("fast place", "World") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		float get_options_height() override { return 155.0f; }
	private:
modules::settings::store<module_settings::fast_place> settings_{"fast_place"};
	};
}
