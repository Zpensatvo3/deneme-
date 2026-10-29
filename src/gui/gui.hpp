#pragma once
#include <atomic>
#include <Windows.h>
#include <imgui/imgui.h>
#include "widgets.hpp"
#include "../modules/render/settings/watermark_settings.hpp"
#include "../modules/render/settings/arraylist_settings.hpp"

namespace gui
{
	enum class icon_asset
	{
		helmet,
		background,
		spotify,
        wing_dragon, wing_angel, wing_butterfly, cape_phoenix, cape_astral,
        count
	};

	inline bool draw = false;
	inline std::atomic_bool should_unload{false};
	inline bool loader_visible = true;
	// Fixed shortcut: Click GUI is always opened with Right Shift.
	inline int menu_key = VK_RSHIFT;
	inline bool menu_key_scanning = false;
	inline bool suppress_menu_key_until_release = false;
	inline std::atomic_bool suppress_escape_until_release{false};
	inline int active_theme = 0; // 0: onestep, 1: Crimson, 2: Emerald, 3: Ocean, 4: Rose, 5: Cyber, 6: Sunset, 7: Mono
	// Render-thread preferences; reference aliases keep the HUD editor API stable.
	inline module_settings::watermark watermark_settings{};
	inline module_settings::arraylist arraylist_settings{};
	inline float& hud_scale = watermark_settings.scale;
	inline float& arraylist_scale = arraylist_settings.scale;
	inline int hud_style = 0; // 0: Glass, 1: Minimal, 2: Compact
	inline float& hud_pos_x = watermark_settings.position_x;
	inline float& hud_pos_y = watermark_settings.position_y;
	inline float& arraylist_pos_x = arraylist_settings.position_x;
	inline float& arraylist_pos_y = arraylist_settings.position_y;
	inline bool hud_editor_active = false;
	inline bool hud_layout_editor_active = false;
	inline ImFont* arraylist_font = nullptr;
	inline ImFont* icon_font = nullptr;
	inline ImFont* ui_font = nullptr;
	inline ImFont* ui_bold_font = nullptr;
	inline ImFont* watermark_font = nullptr;

	bool init();
	void shutdown();

	int get_last_pressed_key();
	void reset_last_pressed_key();
	HWND get_window();

	ImColor get_dynamic_color(int offset = 0);
	ImTextureID get_icon_texture(icon_asset asset);
	void render_click_gui_options();
	void render_theme_selection();
	void request_panic();
}
