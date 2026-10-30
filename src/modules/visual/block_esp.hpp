#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/block_esp_settings.hpp"
#include "../../services/game/overlay_context.hpp"
#include "../../ui/block_picker.hpp"
#include "../../ui/item_icon.hpp"
#include <unordered_map>

namespace modules
{
class block_esp : public module
	{
	public:
		block_esp() : module("block esp", "Visual") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		void on_render_shutdown() override { block_icons_.clear(); }
		float get_options_height() override { return 500.0f; }
	private:
modules::settings::store<module_settings::block_esp> settings_{"block_esp"};
		struct marker { double x, y, z; std::string key; };
		std::size_t scan_index = 0, scan_section = 0;
        std::chrono::steady_clock::time_point next_scan{}; // Worker-owned throttle.
		std::vector<marker> markers{};
		std::mutex marker_mutex{};
        services::game::overlay_session session_;
        void clear_snapshot();
		std::vector<std::string> available_blocks{};
		bool registry_loaded = false;
        // Render-thread-only native icon data. No Java objects or owned GL textures.
        struct block_icon {
            std::vector<gui::item_icons::quad> quads;
            double refresh_at = 0.;
            int last_used = 0;
        };
        std::unordered_map<std::string, block_icon> block_icons_;
        std::vector<gui::block_picker::entry> picker_catalog_;
        gui::block_picker::state picker_state_;
        int picker_frame_ = -2;
	};
}
