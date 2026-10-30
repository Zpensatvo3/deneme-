#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/spotify_hud_settings.hpp"

namespace modules
{
	// Now-playing overlay built on top of spotify::source.
	class spotify_hud : public module
	{
	public:
		static constexpr float fixed_scale = 1.2f;
		spotify_hud() : module("spotify hud", "Visual") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_enable(::cache& cache) override;
		void on_disable(::cache& cache) override;

		float get_options_height() override { const auto settings_snapshot = settings_.read(); return settings_snapshot.show_lyrics ? 422.0f : 384.0f; }

		float get_pos_x() const { const auto settings_snapshot = settings_.read(); return settings_snapshot.position_x; }
		float get_pos_y() const { const auto settings_snapshot = settings_.read(); return settings_snapshot.position_y; }
		void set_pos_x(float x) { settings_.update([&](auto& c) { c.position_x = x; return true; }); }
		void set_pos_y(float y) { settings_.update([&](auto& c) { c.position_y = y; return true; }); }

		struct bounds
		{
			float x = 0.0f;
			float y = 0.0f;
			float w = 0.0f;
			float h = 0.0f;
		};
		bounds get_bounds(float display_w, float display_h) const;
		void render_hud_preview(ImDrawList* draw, float x, float y);

	private:
modules::settings::store<module_settings::spotify_hud> settings_{"spotify_hud"};

		bool applied_lyrics = false;
	};
}
