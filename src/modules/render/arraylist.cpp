#include "../../ui/settings_renderer.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "arraylist.hpp"
#include "../../platform/input/key_codes.hpp"
#include "arraylist_layout.hpp"

#include "../../cache/cache.hpp"
#include "../ui_redirect.hpp"
#include "../core/module_registry.hpp"

#include <imgui/imgui.h>

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cctype>
#include <cstring>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// The layout types live in modules::arraylist_layout. A short alias keeps the
// helper bodies below readable; inside modules::arraylist the fully qualified
// modules::arraylist_layout resolves to the same namespace.
namespace arraylist_layout = ::modules::arraylist_layout;

namespace
{
	// Animation state per stable module id. The map never holds module pointers, so
	// it stays safe across module destruction. It is touched from the render thread
	// only; on_disable merely raises a flag that the next drawn frame consumes.
	struct animation_state
	{
		float visibility = 0.0f;
		bool active = false;
		std::string raw_label;
		std::string label;
		std::string keybind;
	};

	std::unordered_map<std::string, animation_state> states;
	std::atomic<bool> reset_requested{ false };

	// Text measurements only change when the drawn font size or the drawn strings
	// change, so they are cached against both and refreshed when either differs.
	struct measured_widths
	{
		std::string label;
		std::string keybind;
		float text = 0.0f;
		float keybind_width = 0.0f;
	};

	std::unordered_map<std::string, measured_widths> width_cache;
	float width_cache_font_size = -1.0f;
	ImFont* width_cache_font = nullptr;

	std::string display_name(std::string value)
	{
		bool capitalize = true;
		for (char& character : value)
		{
			if (character == '_' || character == '-')
			{
				character = ' ';
				capitalize = true;
				continue;
			}
			if (character == ' ')
			{
				capitalize = true;
				continue;
			}
			if (capitalize && std::isalpha(static_cast<unsigned char>(character)))
				character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
			capitalize = false;
		}
		return value;
	}

	// The overlay never lists the modules that describe the overlay itself.
	bool describes_overlay(const char* id)
	{
		return !id || std::strcmp(id, "watermark") == 0 ||
			std::strcmp(id, "arraylist") == 0 ||
			std::strcmp(id, "friends_manager") == 0;
	}

	std::string keybind_label(int keycode)
	{
		if (!keycode) return {};
		return platform::keys::label(keycode);
	}

	ImU32 with_alpha(const ImColor& color, float alpha)
	{
		ImVec4 value = color.Value;
		value.w *= std::clamp(alpha, 0.0f, 1.0f);
		return ImGui::ColorConvertFloat4ToU32(value);
	}

	struct sample_entry
	{
		const char* label;
		const char* keybind;
	};

	// Keeps the editor handle grabbable when no module is enabled at all.
	constexpr sample_entry preview_samples[] = {
		{ "Player Esp", "" },
		{ "Triggerbot", "R" },
		{ "Full Bright", "" },
		{ "Sprint", "" },
		{ "Radar", "B" }
	};

	constexpr const char* preview_prefix = "preview:";
	constexpr float animation_speed = 16.0f;
	constexpr float edge_margin = 4.0f;

	arraylist_layout::geometry build_geometry(ImFont*)
	{
		const float scale = std::clamp(gui::arraylist_scale, 0.5f, 2.0f);

		arraylist_layout::geometry metrics{};
		metrics.font_size = 14.0f * scale;
		metrics.padding_x = 9.0f * scale;
		metrics.padding_y = 4.0f * scale;
		metrics.bar_width = std::clamp(modules::arraylist::bar_width, 1.5f, 6.0f) * scale;
		metrics.keybind_gap = 6.0f * scale;
		metrics.chip_padding = 4.0f * scale;
		metrics.keybind_font_size = 10.0f * scale;
		metrics.row_gap = 0.0f;
		return metrics;
	}

	const measured_widths& measure(ImFont* font, const arraylist_layout::geometry& metrics,
		const std::string& key, const std::string& label, const std::string& keybind)
	{
		if (width_cache_font_size != metrics.font_size || width_cache_font != font)
		{
			width_cache.clear();
			width_cache_font_size = metrics.font_size;
			width_cache_font = font;
		}

		// Re-measure when the label or the keybind chip changed, otherwise a new
		// keybind would keep drawing at the previous width until the next resize.
		const auto found = width_cache.find(key);
		if (found != width_cache.end() && found->second.label == label &&
			found->second.keybind == keybind)
			return found->second;

		measured_widths widths{};
		widths.label = label;
		widths.keybind = keybind;
		widths.text = font->CalcTextSizeA(metrics.font_size, FLT_MAX, 0.0f, label.c_str()).x;
		if (!keybind.empty())
			widths.keybind_width = font->CalcTextSizeA(metrics.keybind_font_size, FLT_MAX, 0.0f, keybind.c_str()).x;

		return width_cache[key] = std::move(widths);
	}

	// Marks this frame's rows, animates every cached row and erases the ones that
	// finished fading out. Rows still fading keep their captured label and keybind.
	void step_animation(const std::vector<const std::string*>& active_keys, float animation_amount)
	{
		for (const std::string* key : active_keys)
		{
			animation_state& state = states[*key];
			state.active = true;
			state.visibility += (1.0f - state.visibility) * animation_amount;
			if (state.visibility > 0.995f) state.visibility = 1.0f;
		}

		for (auto iterator = states.begin(); iterator != states.end();)
		{
			animation_state& state = iterator->second;
			if (state.active)
			{
				state.active = false;
				++iterator;
				continue;
			}

			state.visibility += (0.0f - state.visibility) * animation_amount;
			if (state.visibility < 0.005f)
			{
				iterator = states.erase(iterator);
				continue;
			}
			++iterator;
		}
	}
}

bool modules::arraylist::draw_overlay(ImDrawList* draw_list, float right_margin, float top,
	bool preview_mode, overlay_bounds* out_bounds)
{
	if (out_bounds) *out_bounds = {};
	if (!draw_list) return false;

	if (reset_requested.exchange(false, std::memory_order_acq_rel))
	{
		states.clear();
		width_cache.clear();
		width_cache_font_size = -1.0f;
	}

	ImFont* font = gui::arraylist_font ? gui::arraylist_font : ImGui::GetFont();
	if (!font) return false;

	ImGuiIO& io = ImGui::GetIO();
	const float delta_time = std::clamp(io.DeltaTime, 0.0f, 0.10f);
	const float animation_amount = 1.0f - std::exp(-animation_speed * delta_time);

	const arraylist_layout::geometry metrics = build_geometry(font);

	// Collect the live module list once. The HUD editor preview shows these same
	// rows and only falls back to samples when nothing at all is enabled.
	struct live_entry
	{
		std::string key;
		modules::module* module = nullptr;
		const char* sample_label = nullptr;
		const char* sample_keybind = nullptr;
	};

	std::vector<live_entry> entries;
	for (modules::module* module : modules::get_modules())
	{
		if (!module || !module->enabled) continue;
		const char* id = module->get_id();
		if (describes_overlay(id)) continue;

		live_entry entry;
		entry.key = id ? id : (module->get_name() ? module->get_name() : "");
		entry.module = module;
		entries.push_back(std::move(entry));
	}

	const bool use_samples = entries.empty() && preview_mode;
	if (use_samples)
	{
		entries.reserve(std::size(preview_samples));
		for (const sample_entry& sample : preview_samples)
		{
			live_entry entry;
			entry.key = preview_prefix + std::string(sample.label);
			entry.sample_label = sample.label;
			entry.sample_keybind = sample.keybind;
			entries.push_back(std::move(entry));
		}
	}

	std::vector<const std::string*> active_keys;
	active_keys.reserve(entries.size());
	for (const live_entry& entry : entries)
		active_keys.push_back(&entry.key);

	step_animation(active_keys, animation_amount);

	std::vector<arraylist_layout::row> rows;
	rows.reserve(entries.size() + states.size());

	std::unordered_set<std::string> live_keys;
	live_keys.reserve(entries.size() * 2);

	// Live rows first, then any row that is still fading out so it animates away
	// instead of disappearing on the frame the module is switched off.
	for (const live_entry& entry : entries)
	{
		animation_state& state = states[entry.key];
		live_keys.insert(entry.key);

		arraylist_layout::row row;
		row.key = entry.key;

		if (entry.sample_label)
		{
			row.label = entry.sample_label;
			row.keybind = modules::arraylist::show_keybinds ? entry.sample_keybind : "";
			state.label = row.label;
			state.keybind = row.keybind;
		}
		else
		{
			modules::module* module = entry.module;
			const char* name = module->get_name();
			const std::string raw = name ? name : "";
			if (raw != state.raw_label)
			{
				state.raw_label = raw;
				state.label = display_name(raw);
			}

			const std::string bind = modules::arraylist::show_keybinds
				? keybind_label(module->get_keybind())
				: std::string();
			if (bind != state.keybind) state.keybind = bind;

			row.label = state.label;
			row.keybind = state.keybind;
		}

		row.visibility = state.visibility;

		const measured_widths& widths = measure(font, metrics, row.key, row.label, row.keybind);
		row.text_width = widths.text;
		row.keybind_width = widths.keybind_width;

		rows.push_back(std::move(row));
	}

	for (const auto& [key, state] : states)
	{
		if (state.visibility <= 0.0f) continue;
		if (live_keys.find(key) != live_keys.end()) continue;

		arraylist_layout::row row;
		row.key = key;
		row.label = state.label;
		row.keybind = state.keybind;
		row.visibility = state.visibility;

		const measured_widths& widths = measure(font, metrics, row.key, row.label, row.keybind);
		row.text_width = widths.text;
		row.keybind_width = widths.keybind_width;

		rows.push_back(std::move(row));
	}

	if (rows.empty()) return false;

	arraylist_layout::sort_rows(rows, metrics);
	const arraylist_layout::result layout = arraylist_layout::build(rows, metrics);
	if (!layout.drawable) return false;

	float clamped_right = right_margin;
	float clamped_top = top;
	arraylist_layout::clamp_anchor(clamped_right, clamped_top, layout.width, layout.height,
		io.DisplaySize.x, io.DisplaySize.y, edge_margin);

	const float right_edge = std::floor(io.DisplaySize.x - clamped_right);
	const float block_top = std::floor(clamped_top);
	const float block_bottom = block_top + layout.height;

	if (out_bounds)
	{
		out_bounds->minimum_x = right_edge - layout.width;
		out_bounds->minimum_y = block_top;
		out_bounds->maximum_x = right_edge;
		out_bounds->maximum_y = block_bottom;
	}

	const float bar_left = right_edge - metrics.bar_width;
	const float line_height = metrics.line_height();

	// Every rectangle is derived once here and reused by all draw passes.
	struct row_rect
	{
		float left = 0.0f;
		float top = 0.0f;
		float bottom = 0.0f;
		float visibility = 0.0f;
		bool visible = false;
	};

	std::vector<row_rect> rects;
	rects.reserve(rows.size());
	for (std::size_t index = 0; index < rows.size(); ++index)
	{
		const float height = line_height * rows[index].visibility;
		row_rect rect{};
		rect.left = std::floor(right_edge - layout.row_left[index]);
		rect.top = block_top + layout.row_top[index];
		rect.bottom = rect.top + height;
		rect.visibility = rows[index].visibility;
		rect.visible = height > 0.5f;
		rects.push_back(rect);
	}

	// One rounded staircase contour keeps the glass connected without equalizing row widths.
	const float scale = std::clamp(gui::arraylist_scale, 0.5f, 2.0f);
	float panel_visibility = 0.0f;
	std::vector<const row_rect*> contour_rows;
	contour_rows.reserve(rects.size());
	for (const row_rect& rect : rects)
	{
		panel_visibility = std::max(panel_visibility, rect.visibility);
		// A newly enabled row can have zero height on a zero-delta frame.
		// Excluding it avoids a self-overlapping spike in the concave outline.
		if (rect.bottom - rect.top > 0.001f) contour_rows.push_back(&rect);
	}
	if (contour_rows.empty()) return false;
	const ImVec2 minimum(contour_rows.front()->left, block_top);
	const float radius = std::min(7.0f * scale, layout.height * 0.5f);
	std::vector<ImVec2> corners;
	auto append_corner = [&](float x, float y)
	{
		if (!corners.empty() && std::fabs(corners.back().x - x) < 0.01f &&
			std::fabs(corners.back().y - y) < 0.01f) return;
		corners.emplace_back(x, y);
	};
	append_corner(contour_rows.front()->left, block_top);
	append_corner(right_edge, block_top);
	append_corner(right_edge, block_bottom);
	append_corner(contour_rows.back()->left, block_bottom);
	for (std::size_t index = contour_rows.size(); index-- > 0;)
	{
		append_corner(contour_rows[index]->left, contour_rows[index]->top);
		if (index > 0) append_corner(contour_rows[index - 1]->left, contour_rows[index]->top);
	}
	// The path is implicitly closed; its last corner must not repeat the first.
	if (std::fabs(corners.back().x - corners.front().x) < 0.01f &&
		std::fabs(corners.back().y - corners.front().y) < 0.01f) corners.pop_back();
	std::vector<ImVec2> contour;
	contour.reserve(corners.size() * 5);
	for (std::size_t index = 0; index < corners.size(); ++index)
	{
		const ImVec2 p = corners[index];
		const ImVec2 previous = corners[(index + corners.size() - 1) % corners.size()];
		const ImVec2 next = corners[(index + 1) % corners.size()];
		const float before = std::hypot(previous.x - p.x, previous.y - p.y);
		const float after = std::hypot(next.x - p.x, next.y - p.y);
		const float rounding = std::min(index < 4 ? radius : 2.0f * scale, std::min(before, after) * 0.4f);
		const ImVec2 from(p.x + (previous.x - p.x) * rounding / before,
			p.y + (previous.y - p.y) * rounding / before);
		const ImVec2 to(p.x + (next.x - p.x) * rounding / after,
			p.y + (next.y - p.y) * rounding / after);
		contour.push_back(from);
		const int segments = std::clamp(static_cast<int>(std::ceil(std::sqrt(rounding /
			std::max(.01f, ImGui::GetStyle().CurveTessellationTol)))), 4, 12);
		for (int segment = 1; segment <= segments; ++segment)
		{
			const float t = static_cast<float>(segment) / static_cast<float>(segments);
			const float u = 1.0f - t;
			contour.emplace_back(u * u * from.x + 2.0f * u * t * p.x + t * t * to.x,
				u * u * from.y + 2.0f * u * t * p.y + t * t * to.y);
		}
	}
	const int background_alpha = static_cast<int>(215.0f * std::clamp(bg_opacity, 0.0f, 1.0f) * panel_visibility);
	const int first_vertex = draw_list->VtxBuffer.Size;
	draw_list->AddConcavePolyFilled(contour.data(), static_cast<int>(contour.size()), IM_COL32(255, 255, 255, background_alpha));
	for (int vertex = first_vertex; vertex < draw_list->VtxBuffer.Size; ++vertex)
	{
		ImDrawVert& v = draw_list->VtxBuffer[vertex];
		const float t = std::clamp((v.pos.y - block_top) / layout.height, 0.0f, 1.0f);
		const ImU32 tint = IM_COL32(static_cast<int>(23.0f - 10.0f * t),
			static_cast<int>(26.0f - 11.0f * t), static_cast<int>(33.0f - 11.0f * t), 255);
		v.col = (v.col & IM_COL32_A_MASK) | (tint & ~IM_COL32_A_MASK);
	}
	draw_list->AddPolyline(contour.data(), static_cast<int>(contour.size()),
		IM_COL32(231, 235, 255, static_cast<int>(22.0f * panel_visibility)), ImDrawFlags_Closed, 1.0f);
	draw_list->AddLine(ImVec2(minimum.x + radius, block_top + 1.0f),
		ImVec2(right_edge - radius, block_top + 1.0f), IM_COL32(240, 244, 255, static_cast<int>(25.0f * panel_visibility)));

	const float inset = std::min(5.0f * scale, layout.height * 0.25f);
	const ImVec2 rail_min(bar_left, block_top + inset);
	const ImVec2 rail_max(right_edge, block_bottom - inset);
	const ImColor accent_top = gui::get_dynamic_color(0);
	const ImColor accent_bottom = gui::get_dynamic_color(200);
	if (bar_glow)
	{
		for (int layer = 4; layer >= 1; --layer)
		{
			const float spread = static_cast<float>(layer) * 0.8f * scale;
			draw_list->AddRectFilled(ImVec2(rail_min.x - spread, rail_min.y - spread),
				ImVec2(rail_max.x, rail_max.y + spread), with_alpha(accent_top, 0.022f * panel_visibility),
				metrics.bar_width * 0.5f + spread);
		}
	}
	const int rail_first_vertex = draw_list->VtxBuffer.Size;
	draw_list->AddRectFilled(rail_min, rail_max, with_alpha(accent_top, panel_visibility), metrics.bar_width * 0.5f);
	for (int vertex = rail_first_vertex; vertex < draw_list->VtxBuffer.Size; ++vertex)
	{
		ImDrawVert& v = draw_list->VtxBuffer[vertex];
		const float t = std::clamp((v.pos.y - rail_min.y) / (rail_max.y - rail_min.y), 0.0f, 1.0f);
		const ImVec4 a = accent_top.Value, b = accent_bottom.Value;
		const ImU32 tint = ImGui::ColorConvertFloat4ToU32(ImVec4(a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, 1.0f));
		v.col = (v.col & IM_COL32_A_MASK) | (tint & ~IM_COL32_A_MASK);
	}

	// 4. Labels, then 5. keybind chips. Both reuse the rectangles computed above.
	for (std::size_t index = 0; index < rects.size(); ++index)
	{
		const row_rect& rect = rects[index];
		if (!rect.visible) continue;

		const arraylist_layout::row& entry = rows[index];
		const float row_height = rect.bottom - rect.top;

		const float progress = rows.size() > 1
			? static_cast<float>(index) / static_cast<float>(rows.size() - 1)
			: 0.0f;
		const ImColor accent = gui::get_dynamic_color(static_cast<int>(progress * 200.0f));
		const ImVec4 hue = accent.Value;
		const ImColor label_color(0.94f * 0.18f + hue.x * 0.82f,
			0.94f * 0.18f + hue.y * 0.82f, 0.97f * 0.18f + hue.z * 0.82f, 1.0f);
		const ImU32 text_color = with_alpha(label_color, rect.visibility);

		draw_list->PushClipRect(ImVec2(rect.left, rect.top), ImVec2(bar_left, rect.bottom), true);

		// Rows shrink while fading, so the label slides instead of popping away.
		const float slide = (1.0f - rect.visibility) * 12.0f;
		const float text_x = rect.left + metrics.padding_x + slide;
		const float text_y = rect.top + (row_height - metrics.font_size) * 0.5f;
		const float label_right = entry.keybind.empty() ? bar_left - metrics.padding_x
			: bar_left - metrics.padding_x - entry.keybind_width - metrics.chip_padding * 2.0f - metrics.keybind_gap;
		const ImVec4 label_clip(rect.left, rect.top, label_right, rect.bottom);

		if (text_shadow)
		{
			draw_list->AddText(font, metrics.font_size,
				ImVec2(std::floor(text_x + 1.0f), std::floor(text_y + 1.0f)),
				IM_COL32(0, 0, 0, static_cast<int>(180.0f * rect.visibility)), entry.label.c_str(), nullptr, 0.0f, &label_clip);
		}

		draw_list->AddText(font, metrics.font_size, ImVec2(std::floor(text_x), std::floor(text_y)),
			text_color, entry.label.c_str(), nullptr, 0.0f, &label_clip);

		if (entry.keybind.empty())
		{
			draw_list->PopClipRect();
			continue;
		}

		// The chip hugs the bar; row_width() already reserved exactly this much room.
		const float chip_width = entry.keybind_width + metrics.chip_padding * 2.0f;
		const float chip_right = bar_left - metrics.padding_x;
		const float chip_left = chip_right - chip_width;
		const float chip_height = metrics.keybind_font_size + 4.0f * scale;
		const float chip_top = rect.top + (row_height - chip_height) * 0.5f;

		draw_list->AddRectFilled(
			ImVec2(std::floor(chip_left), std::floor(chip_top)),
			ImVec2(std::ceil(chip_right), std::ceil(chip_top + chip_height)),
			with_alpha(accent, 0.10f * rect.visibility),
			std::max(2.0f, metrics.chip_padding * 0.8f));

		draw_list->AddText(font, metrics.keybind_font_size,
			ImVec2(std::floor(chip_left + metrics.chip_padding),
				std::floor(chip_top + 2.0f * scale)),
			IM_COL32(235, 235, 245, static_cast<int>(215.0f * rect.visibility)),
			entry.keybind.c_str());
		draw_list->PopClipRect();
	}

	return true;
}

void modules::arraylist::render_overlay()
{
	if (gui::hud_editor_active) return;

	draw_overlay(ImGui::GetForegroundDrawList(), gui::arraylist_pos_x, gui::arraylist_pos_y,
		false, nullptr);
}

void modules::arraylist::on_disable(::cache&)
{
	// The animation map belongs to the render thread, so the module loop only
	// requests a reset; the next drawn frame performs it.
	reset_requested.store(true, std::memory_order_release);
}

void modules::arraylist::render_options()
{
    gui::settings::render(gui::arraylist_settings, module_settings::arraylist::schema());
}
