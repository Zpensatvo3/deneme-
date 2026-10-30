#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/player_esp_settings.hpp"
#include <cstdint>

namespace modules
{
class player_esp : public module
	{
	public:
		player_esp() : module("player esp", "Visual") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 300.0f; }
		struct preview_settings
		{
			int mode = 0;
			float thickness = 1.5f;
			float fill_opacity = 0.16f;
			bool names = true;
			bool health = true;
			bool distance = true;
			bool theme_color = false;
			float color[4]{ 0.35f, 0.75f, 1.0f, 1.0f };
		};
		preview_settings get_preview_settings() const
		{
            const auto settings_snapshot = settings_.read();
			preview_settings result{};
			result.mode = settings_snapshot.esp_mode;
			result.thickness = settings_snapshot.box_thickness;
			result.fill_opacity = settings_snapshot.fill_opacity;
			result.names = settings_snapshot.show_names;
			result.health = settings_snapshot.show_health;
			result.distance = settings_snapshot.show_distance;
			result.theme_color = settings_snapshot.use_theme_color;
			for (int i = 0; i < 4; ++i) result.color[i] = settings_snapshot.esp_color[i];
			return result;
		}
		static std::uintptr_t local_skin_texture_id();
		static void refresh_local_skin_texture();
	private:
modules::settings::store<module_settings::player_esp> settings_{"player_esp"};
	};
}
