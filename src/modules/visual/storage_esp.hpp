#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/storage_esp_settings.hpp"
#include "../../services/game/overlay_context.hpp"

namespace modules
{
class storage_esp : public module
	{
	public:
		storage_esp() : module("storage esp", "Visual") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 250.0f; }
	private:
modules::settings::store<module_settings::storage_esp> settings_{"storage_esp"};
		struct marker { double x, y, z; std::string label; };
		std::size_t scan_index = 0;
        std::chrono::steady_clock::time_point next_scan{}; // Worker-owned throttle.
		std::vector<marker> markers{};
		std::mutex marker_mutex{};
        services::game::overlay_session session_;
        void clear_snapshot();
	};
}
