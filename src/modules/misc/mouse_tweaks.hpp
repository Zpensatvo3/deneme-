#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/mouse_tweaks_settings.hpp"
#include "../../platform/adapter/target_version.hpp"

namespace modules
{
class mouse_tweaks : public module
	{
	public:
		mouse_tweaks();
		~mouse_tweaks() override;

		void run(::cache& cache) override;
		void render_options() override;
		void on_disable(::cache& cache) override;
        void render_overlay() override;
        void on_render_shutdown() override;
        bool render_when_disabled() const override { return mc::target.build_id == 12111; }
		float get_options_height() override { return 120.0f; }
	private:
		std::atomic_bool reset_requested_{true};
modules::settings::store<module_settings::mouse_tweaks> settings_{"mouse_tweaks"};
	};
}
