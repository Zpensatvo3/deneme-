#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

#include <windows.h>
#include <gl/GL.h>
#include <imgui/imgui.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#ifndef GL_LINEAR_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#endif

#include "spotify_hud.hpp"
#include "../../ui/settings_renderer.hpp"
#include "spotify_source.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/ui_state.hpp"

namespace
{
	constexpr ImU32 color_title = IM_COL32(255, 255, 255, 255);
	constexpr ImU32 color_artist = IM_COL32(166, 169, 178, 255);
	constexpr ImU32 color_label = IM_COL32(30, 215, 96, 255);
	constexpr ImU32 color_time = IM_COL32(137, 143, 152, 255);
	constexpr ImU32 color_brand = IM_COL32(30, 215, 96, 255);

	struct card_layout
	{
		float width;
		float height;
		float radius;
		float pad;
		float art_size;
		float art_radius;
		float text_left;
		float text_right;
		float label_y;
		float title_y;
		float artist_y;
		float bar_y;
		float bar_height;
		float time_y;
		float text_block_h;
		float header_content_h;
		float divider_y;
		float lyric_start_y;
		float lyric_row_spacing;
		float lyric_viewport_h;
		float brand_radius;
		float title_font;
		float artist_font;
		float label_font;
		float time_font;
		float lyric_font;
	};

	card_layout compute_layout(float size_scale, bool show_artwork, bool, bool show_lyrics, int)
	{
		card_layout l{};
		size_scale = modules::spotify_hud::fixed_scale;
		l.width = std::round(260.0f * size_scale);
		l.pad = std::round(10.0f * size_scale);
		l.radius = std::round(12.0f * size_scale);

		// Non-linear typography scaling curve so fonts never collapse into illegible unreadable smudges
		const float font_scale = 0.35f + 0.65f * size_scale;
		l.title_font = std::round(14.0f * font_scale);
		l.artist_font = std::round(11.0f * font_scale);
		l.label_font = std::round(8.0f * font_scale);
		l.time_font = std::round(9.0f * font_scale);
		l.lyric_font = std::round(11.5f * font_scale);

		l.art_size = show_artwork ? std::round(48.0f * size_scale) : 0.0f;
		l.art_radius = std::round(7.0f * size_scale);

		l.text_left = show_artwork
			? std::round(l.pad + l.art_size + 10.0f * size_scale)
			: l.pad;
		l.text_right = l.width - l.pad;

		// Stack the right column items adaptively with proportional breathing room
		float cur_y = l.pad;
		l.label_y = cur_y;
		cur_y += l.label_font + std::round(4.0f * size_scale);

		l.title_y = cur_y;
		cur_y += l.title_font + std::round(3.0f * size_scale);

		l.artist_y = cur_y;
		cur_y += l.artist_font;
		l.text_block_h = cur_y - l.pad;
		l.header_content_h = (std::max)(l.art_size, l.text_block_h);
		cur_y = l.pad + l.header_content_h + std::round(6.0f * size_scale);

		l.bar_y = cur_y;
		l.bar_height = 2.0f;
		l.time_y = std::round(cur_y - l.time_font * 0.5f + 1.0f);
		// Times and the short rail share one row beneath the right column.
		const float header_h = l.time_y + l.time_font + std::round(6.0f * size_scale);

		l.brand_radius = std::round((std::max)(10.0f, 12.0f * size_scale));

		if (show_lyrics)
		{
			l.divider_y = header_h;
			const float lyrics_pad_top = std::round(6.0f * size_scale);
			const float lyrics_pad_bottom = std::round(6.0f * size_scale);
			l.lyric_start_y = l.divider_y + lyrics_pad_top;
			l.lyric_row_spacing = l.lyric_font + 3.0f;
			l.lyric_viewport_h = l.lyric_row_spacing;
			l.height = std::round(l.lyric_start_y + l.lyric_viewport_h + lyrics_pad_bottom);
		}
		else
		{
			l.divider_y = header_h;
			l.lyric_start_y = header_h;
			l.lyric_row_spacing = 0.0f;
			l.lyric_viewport_h = 0.0f;
			l.height = std::round(header_h);
		}

		return l;
	}

	float hash_unit(std::string_view value)
	{
		std::uint32_t hash = 2166136261u;
		for (const unsigned char character : value)
		{
			hash ^= character;
			hash *= 16777619u;
		}
		return static_cast<float>(hash & 0xFFFFu) / 65535.0f;
	}

	std::string format_clock(double seconds)
	{
		const bool negative = seconds < 0.0;
		const long long total = std::llround(negative ? -seconds : seconds);
		const long long hours = static_cast<long long>(total / 3600.0);
		const long long minutes = static_cast<long long>((total % 3600) / 60);
		const long long remainder = static_cast<long long>(total % 60);

		char buffer[32]{};
		if (hours > 0)
			sprintf_s(buffer, "%lld:%02lld:%02lld", hours, minutes, remainder);
		else
			sprintf_s(buffer, "%lld:%02lld", minutes, remainder);

		return (negative ? std::string("-") : std::string()) + buffer;
	}

	std::string fit_text(const ImFont* font, float font_size, std::string_view text, float max_width)
	{
		if (!font || max_width <= 0.0f || text.empty()) return {};
		if (font->CalcTextSizeA(font_size, FLT_MAX, 0.0f,
			text.data(), text.data() + text.size()).x <= max_width)
			return std::string(text);

		std::size_t keep = text.size();
		while (keep > 0)
		{
			keep -= keep > 3 ? 1 : keep;
			// Avoid slicing across multi-byte UTF-8 sequence
			while (keep > 0 && (static_cast<unsigned char>(text[keep]) & 0xC0) == 0x80)
			{
				--keep;
			}
			std::string candidate(text.substr(0, keep));
			candidate.append("...");
			if (font->CalcTextSizeA(font_size, FLT_MAX, 0.0f,
				candidate.c_str(), candidate.c_str() + candidate.size()).x <= max_width)
				return candidate;
			if (keep == 0) break;
		}
		return {};
	}

	ImU32 lerp_color(const ImU32 from, const ImU32 to, float amount)
	{
		amount = std::clamp(amount, 0.0f, 1.0f);
		const ImVec4 a = ImGui::ColorConvertU32ToFloat4(from);
		const ImVec4 b = ImGui::ColorConvertU32ToFloat4(to);
		return ImGui::ColorConvertFloat4ToU32(ImVec4(
			a.x + (b.x - a.x) * amount,
			a.y + (b.y - a.y) * amount,
			a.z + (b.z - a.z) * amount,
			a.w + (b.w - a.w) * amount));
	}

	void draw_glass_panel(ImDrawList* draw, const ImVec2& minimum, const ImVec2& maximum,
		float radius, float opacity)
	{
		// Soft emerald edge bloom, followed by a translucent rounded gradient.
		for (int layer = 10; layer >= 1; --layer)
		{
			const float spread = static_cast<float>(layer) * 1.2f;
			const int glow_alpha = static_cast<int>(2.0f + 16.0f * (1.0f - layer / 10.0f));
			draw->AddRect(ImVec2(minimum.x - spread, minimum.y - spread),
				ImVec2(maximum.x + spread, maximum.y + spread),
				IM_COL32(30, 215, 115, glow_alpha), radius + spread, 0, 1.6f);
		}
		const int background_alpha = static_cast<int>(std::clamp(opacity, 0.20f, 1.0f) * 215.0f);
		const int first_vertex = draw->VtxBuffer.Size;
		draw->AddRectFilled(minimum, maximum, IM_COL32(255, 255, 255, background_alpha), radius);
		for (int vertex = first_vertex; vertex < draw->VtxBuffer.Size; ++vertex)
		{
			ImDrawVert& v = draw->VtxBuffer[vertex];
			const float t = (v.pos.y - minimum.y) / (maximum.y - minimum.y);
			const ImU32 tint = lerp_color(IM_COL32(27, 38, 35, 255), IM_COL32(12, 17, 22, 255), t);
			v.col = (v.col & IM_COL32_A_MASK) | (tint & ~IM_COL32_A_MASK);
		}
		draw->AddRect(minimum, maximum, IM_COL32(167, 241, 196, 38), radius, 0, 1.0f);
		draw->AddLine(ImVec2(minimum.x + radius, minimum.y + 1.0f),
			ImVec2(maximum.x - radius, minimum.y + 1.0f), IM_COL32(223, 255, 234, 58));
	}

	void draw_progress_bar(ImDrawList* draw, float left, float right, float center_y,
		float progress, float size_scale)
	{
		const float width = right - left;
		if (!draw || width <= 8.f) return;
		progress = std::clamp(progress, 0.f, 1.f);
		const float fill_x = left + width * progress;
		const float half_height = 1.f * size_scale;
		draw->AddRectFilled(ImVec2(left, center_y - half_height), ImVec2(right, center_y + half_height),
			IM_COL32(255, 255, 255, 42), half_height);
		if (fill_x > left)
		{
			for (int layer = 3; layer > 0; --layer)
			{
				const float spread = layer * size_scale;
				draw->AddRectFilled(ImVec2(left - spread, center_y - half_height - spread),
					ImVec2(fill_x + spread, center_y + half_height + spread),
					IM_COL32(30, 215, 96, 10), half_height + spread);
			}
			draw->AddRectFilled(ImVec2(left, center_y - half_height), ImVec2(fill_x, center_y + half_height),
				color_brand, half_height);
		}
		const ImVec2 marker(fill_x, center_y);
		draw->AddCircleFilled(marker, 5.f * size_scale, IM_COL32(30, 215, 96, 22), 32);
		draw->AddCircleFilled(marker, 3.5f * size_scale, IM_COL32(30, 215, 96, 45), 24);
		draw->AddCircleFilled(marker, 2.f * size_scale, IM_COL32(242, 255, 246, 255), 24);
	}

	void draw_visualizer(ImDrawList* draw, float left, float center_y,
		const modules::spotify::now_playing& info, float size_scale)
	{
		if (!draw) return;
		const bool reduce_motion = gui::ui_state::get_preferences().reduce_motion;
		static int last_frame = -1;
		static float phase = 0.f;
		static std::array<float, 6> heights{};
		static std::string track_key;
		const std::string key = info.title + "\n" + info.artist;
		if (key != track_key)
		{
			track_key = key;
			phase = hash_unit(key) * 6.2831853f;
		}
		if (last_frame != ImGui::GetFrameCount())
		{
			last_frame = ImGui::GetFrameCount();
			const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.f, .05f);
			if (info.playing && !reduce_motion)
				phase = std::fmod(phase + dt * 4.8f, 6.2831853f);
			for (int index = 0; index < static_cast<int>(heights.size()); ++index)
			{
				const float envelope = .55f + .45f * std::sin((index + .5f) / static_cast<float>(heights.size()) * 3.14159265f);
				const float movement = reduce_motion ? .65f : .5f + .5f * std::sin(phase + index * 1.35f);
				const float target = info.playing ? 1.f + 4.5f * envelope * movement : .8f;
				heights[index] = reduce_motion ? target
					: heights[index] + (target - heights[index]) * (1.f - std::exp(-14.f * dt));
			}
		}
		for (int index = 0; index < static_cast<int>(heights.size()); ++index)
		{
			const float bar_left = std::round(left + index * 4.f * size_scale);
			const float bar_width = 2.f * size_scale;
			const float half = (std::max)(.8f, heights[index]) * size_scale;
			draw->AddRectFilled(ImVec2(bar_left - size_scale, center_y - half - size_scale),
				ImVec2(bar_left + bar_width + size_scale, center_y + half + size_scale),
				IM_COL32(30, 215, 96, 18), bar_width);
			draw->AddRectFilled(ImVec2(bar_left, center_y - half),
				ImVec2(bar_left + bar_width, center_y + half), color_brand, bar_width * .5f);
		}
	}

	void draw_spotify_logo(ImDrawList* draw, const ImVec2& center, float mark_radius)
	{
		if (!draw) return;
		const ImVec2 minimum(std::round(center.x - mark_radius), std::round(center.y - mark_radius));
		const float diameter = std::round(mark_radius * 2.f);
        // Use the supplied PNG with its original alpha and colors. No vector
        // approximation, black backing disc or generated cutout fill.
        if (const auto texture = gui::get_icon_texture(gui::icon_asset::spotify))
            draw->AddImage(texture, minimum, ImVec2(minimum.x + diameter, minimum.y + diameter));
	}

	void draw_card(ImDrawList* draw, float x, float y, float size_scale,
		float background_opacity, bool show_artwork, bool show_progress,
		bool show_visualizer, bool show_brand, bool show_lyrics, int,
		const modules::spotify::now_playing& info, bool is_idle)
	{
		if (!draw) return;
		size_scale = modules::spotify_hud::fixed_scale;

		const ImFont* regular_font = gui::ui_font ? gui::ui_font : ImGui::GetFont();
		const ImFont* bold_font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
		const ImFont* small_font = gui::arraylist_font ? gui::arraylist_font : regular_font;
		if (!regular_font || !bold_font || !small_font) return;

		if (is_idle)
		{
			// Sleek, compact idle card when no track is currently playing
			const float idle_w = std::round(190.0f * size_scale);
			const float idle_h = std::round(40.0f * size_scale);
			const ImVec2 cmin(std::round(x), std::round(y));
			const ImVec2 cmax(std::round(x + idle_w), std::round(y + idle_h));
			const float radius = std::round((std::max)(8.0f, 12.0f * size_scale));
			draw_glass_panel(draw, cmin, cmax, radius, background_opacity);

			// Spotify mark
			const float logo_r = std::round((std::max)(9.0f, 16.0f * size_scale));
			const ImVec2 logo_center(std::round(cmin.x + (std::max)(18.0f, 24.0f * size_scale)), std::round(cmin.y + idle_h * 0.5f));
			if (show_brand) draw_spotify_logo(draw, logo_center, logo_r);

			const float t_x = std::round(cmin.x + (show_brand
				? (std::max)(38.0f, 46.0f * size_scale) : (std::max)(12.0f, 16.0f * size_scale)));
			const float font_scale = 0.35f + 0.65f * size_scale;
			const float title_sz = std::round((std::max)(11.5f, 14.0f * font_scale));
			const float sub_sz = std::round((std::max)(9.5f, 11.0f * font_scale));
			draw->AddText(bold_font, title_sz, ImVec2(t_x, std::round(cmin.y + (idle_h * 0.5f - title_sz * 0.95f))),
				color_title, "Spotify");
			const std::string idle_text = fit_text(small_font, sub_sz,
				"No track playing", cmax.x - t_x - 12.0f);
			draw->AddText(small_font, sub_sz, ImVec2(t_x, std::round(cmin.y + (idle_h * 0.5f + 1.5f))),
				color_artist, idle_text.c_str());
			return;
		}

		const auto layout = compute_layout(size_scale, show_artwork, show_progress, show_lyrics, 1);
		const ImVec2 card_min(std::round(x), std::round(y));
		const ImVec2 card_max(std::round(x + layout.width), std::round(y + layout.height));
		draw_glass_panel(draw, card_min, card_max, layout.radius, background_opacity);

		// ---- Album art (with trilinear mipmap downsampling for razor sharpness) ----
		if (show_artwork)
		{
			const float art_top = std::round(card_min.y + layout.pad +
				((layout.header_content_h > layout.art_size) ? (layout.header_content_h - layout.art_size) * 0.5f : 0.0f));
			const ImVec2 art_min(std::round(card_min.x + layout.pad), art_top);
			const ImVec2 art_max(std::round(art_min.x + layout.art_size), std::round(art_top + layout.art_size));

			// Shadow under album art
			draw->AddRectFilled(
				ImVec2(art_min.x - 1.0f, art_min.y + 1.0f),
				ImVec2(art_max.x + 1.0f, art_max.y + (std::max)(2.0f, 3.0f * size_scale)),
				IM_COL32(0, 0, 0, 75), layout.art_radius + 1.0f);

			// Art slot base
			draw->AddRectFilled(art_min, art_max, IM_COL32(18, 22, 20, 255), layout.art_radius);

			static GLuint texture = 0;
			static std::uint64_t texture_revision = static_cast<std::uint64_t>(-1);

			if (!info.artwork.empty() && info.artwork_width > 0 && info.artwork_height > 0)
			{
				if (texture_revision != info.artwork_revision)
				{
					if (texture) { glDeleteTextures(1, &texture); texture = 0; }
					glGenTextures(1, &texture);
					if (texture)
					{
						glBindTexture(GL_TEXTURE_2D, texture);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
						glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info.artwork_width, info.artwork_height,
							0, GL_RGBA, GL_UNSIGNED_BYTE, info.artwork.data());

						// Generate mipmaps for smooth, aliasing-free downsampling at any scale
						typedef void (APIENTRY* PFNGLGENERATEMIPMAPPROC)(GLenum target);
						static PFNGLGENERATEMIPMAPPROC pfn_glGenerateMipmap = nullptr;
						static bool pfn_resolved = false;
						if (!pfn_resolved)
						{
							pfn_glGenerateMipmap = reinterpret_cast<PFNGLGENERATEMIPMAPPROC>(wglGetProcAddress("glGenerateMipmap"));
							pfn_resolved = true;
						}

						if (pfn_glGenerateMipmap)
						{
							pfn_glGenerateMipmap(GL_TEXTURE_2D);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
						}
						else
						{
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
						}
						glBindTexture(GL_TEXTURE_2D, 0);
					}
					texture_revision = info.artwork_revision;
				}
			}
			else if (texture && texture_revision != info.artwork_revision)
			{
				glDeleteTextures(1, &texture);
				texture = 0;
				texture_revision = info.artwork_revision;
			}

			if (texture)
			{
				draw->AddImageRounded(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture)),
					art_min, art_max, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
					IM_COL32(255, 255, 255, 255), layout.art_radius);
			}
			else
			{
				// A neutral record sleeve when album artwork is unavailable.
				const ImVec2 art_center((art_min.x + art_max.x) * 0.5f, (art_min.y + art_max.y) * 0.5f);
				const float record_radius = layout.art_size * 0.29f;
				draw->AddCircleFilled(art_center, record_radius, IM_COL32(34, 39, 44, 255), 0);
				draw->AddCircle(art_center, record_radius * 0.74f, IM_COL32(255, 255, 255, 18), 0);
				draw->AddCircle(art_center, record_radius * 0.48f, IM_COL32(255, 255, 255, 18), 0);
				draw->AddCircleFilled(art_center, record_radius * 0.22f, color_brand, 0);
			}
			// Clean inner bevel highlight
			draw->AddRect(art_min, art_max, IM_COL32(255, 255, 255, 26), layout.art_radius, 0, 1.0f);
		}

		// Vertical centering for the right text block if artwork is taller
		const float y_base = card_min.y;

		// ---- Title block + Pulsing Live Dot ----------------------------------
		const float dot_r = (std::max)(1.5f, 2.0f * size_scale);
		const float dot_x = std::round(card_min.x + layout.text_left + dot_r);
		const float label_pos_y = std::round(y_base + layout.label_y);
		const float dot_y = std::round(label_pos_y + layout.label_font * 0.5f);
		const float label_left = dot_x + dot_r + (std::max)(3.5f, 4.5f * size_scale);
		const float logo_left = card_max.x - layout.pad - (show_brand ? layout.brand_radius * 2.f : 0.f);
		const float visualizer_right = logo_left - (show_brand ? 7.f * size_scale : 0.f);
		const float visualizer_left = visualizer_right - 22.f * size_scale;
		const float label_right = show_visualizer ? visualizer_left - 7.f * size_scale
			: logo_left - (show_brand ? 6.f * size_scale : 0.f);
		const std::string status_text = fit_text(bold_font, layout.label_font,
			info.playing ? "PLAYING" : "PAUSED",
			label_right - label_left);

		if (info.playing)
		{
			draw->AddCircleFilled(ImVec2(dot_x, dot_y), dot_r, IM_COL32(30, 215, 96, 255), 0);
			draw->AddText(bold_font, layout.label_font,
				ImVec2(label_left, label_pos_y), color_label, status_text.c_str());
		}
		else
		{
			draw->AddCircleFilled(ImVec2(dot_x, dot_y), dot_r, IM_COL32(235, 185, 55, 220), 0);
			draw->AddText(bold_font, layout.label_font,
				ImVec2(label_left, label_pos_y), color_artist, status_text.c_str());
		}

		// Spotify Brand Logo in top-right
		if (show_brand)
		{
			const ImVec2 brand_center(std::round(card_max.x - layout.pad - layout.brand_radius),
				std::round(card_min.y + layout.pad + layout.brand_radius));
			draw_spotify_logo(draw, brand_center, layout.brand_radius);
		}
		if (show_visualizer)
			draw_visualizer(draw, visualizer_left, std::round(card_min.y + layout.pad + layout.brand_radius), info, size_scale);

		const float title_pos_y = std::round(y_base + layout.title_y);
		const float title_right_edge = label_right;
		const float title_max_w = (std::max)(20.0f, title_right_edge - (card_min.x + layout.text_left));
		const std::string title = fit_text(bold_font, layout.title_font, info.title, title_max_w);
		draw->AddText(bold_font, layout.title_font,
			ImVec2(std::round(card_min.x + layout.text_left), title_pos_y),
			color_title, title.c_str());

		if (!info.artist.empty())
		{
			const float artist_pos_y = std::round(y_base + layout.artist_y);
			const float artist_max_w = (std::max)(20.0f, (card_max.x - layout.pad) - (card_min.x + layout.text_left));
			const std::string artist = fit_text(small_font, layout.artist_font, info.artist, artist_max_w);
			draw->AddText(small_font, layout.artist_font,
				ImVec2(std::round(card_min.x + layout.text_left), artist_pos_y),
				color_artist, artist.c_str());
		}

		// ---- Progress bar with smooth gliding and glowing playhead -----------
		const double duration = info.duration;
		const double raw_progress = duration > 0.0
			? std::clamp(info.position / duration, 0.0, 1.0)
			: 0.0;

		static float visual_progress = 0.0f;
		static std::string active_track_title;
		if (active_track_title != info.title)
		{
			active_track_title = info.title;
			visual_progress = static_cast<float>(raw_progress);
		}
		else
		{
			const float target = static_cast<float>(raw_progress);
			const float dt = ImGui::GetIO().DeltaTime;
			if (std::abs(target - visual_progress) > 0.15f)
			{
				visual_progress = target;
			}
			else
			{
				visual_progress += (target - visual_progress) * (std::min)(1.0f, dt * 14.0f);
			}
		}
		visual_progress = std::clamp(visual_progress, 0.0f, 1.0f);

		const std::string elapsed_text = format_clock(info.position);
		const double remaining = duration > 0.0 ? (std::max)(0.0, duration - info.position) : 0.0;
		const std::string remaining_text = duration > 0.0 ? format_clock(-remaining) : "--:--";
		const float elapsed_width = small_font->CalcTextSizeA(layout.time_font, FLT_MAX, 0.0f, elapsed_text.c_str()).x;
		const float remaining_width = small_font->CalcTextSizeA(layout.time_font, FLT_MAX, 0.0f, remaining_text.c_str()).x;
		if (show_progress && duration > 0.0)
		{
			const ImVec2 bar_min(std::round(card_min.x + layout.text_left + elapsed_width + 8.0f), std::round(y_base + layout.bar_y));
			const ImVec2 bar_max(std::round(card_min.x + layout.text_right - remaining_width - 8.0f), std::round(bar_min.y + layout.bar_height));
			draw_progress_bar(draw, bar_min.x, bar_max.x, (bar_min.y + bar_max.y) * .5f,
				visual_progress, size_scale);
		}

		// ---- Times ---------------------------------------------------------
		const float times_y = std::round(y_base + layout.time_y);
		const float text_left_x = std::round(card_min.x + layout.text_left);
		draw->AddText(small_font, layout.time_font, ImVec2(text_left_x, times_y), color_time, elapsed_text.c_str());

		const float remaining_x = std::round(card_min.x + layout.text_right - remaining_width);
		draw->AddText(small_font, layout.time_font, ImVec2(remaining_x, times_y), color_time,
			remaining_text.c_str());

		// Exactly one current lyric; fade the new line in without exposing neighbours.
		if (show_lyrics)
		{
			const float divider_y = std::round(card_min.y + layout.divider_y);
			draw->AddLine(ImVec2(card_min.x + layout.pad, divider_y),
				ImVec2(card_max.x - layout.pad, divider_y), IM_COL32(220, 255, 236, 18));
			const int index = info.current_lyric();
			const bool has_line = index >= 0 && index < static_cast<int>(info.lyrics.size());
			const std::string identity = info.title + "\\n" + info.artist + "\\n" + std::to_string(index);
			static std::string last_identity;
			static double line_changed_at = 0.0;
			const double now = ImGui::GetTime();
			if (identity != last_identity)
			{
				last_identity = identity;
				line_changed_at = now;
			}
			const float fade = 0.85f + 0.15f * std::clamp(static_cast<float>((now - line_changed_at) / 0.18), 0.0f, 1.0f);
			const float text_x = std::round(card_min.x + layout.pad + 9.0f);
			const float text_y = std::round(card_min.y + layout.lyric_start_y + 1.0f);
			const char* message = has_line ? info.lyrics[index].text.c_str()
				: (!info.lyrics_loaded ? "Searching lyrics..."
					: (!info.lyrics.empty() ? "Waiting for lyrics..."
						: "No lyrics available"));
			const ImFont* line_font = has_line ? bold_font : small_font;
			const std::string line = fit_text(line_font, layout.lyric_font, message, card_max.x - layout.pad - text_x);
			draw->PushClipRect(ImVec2(card_min.x + layout.pad - 3.0f, text_y - 3.0f),
				ImVec2(card_max.x - layout.pad, card_min.y + layout.lyric_start_y + layout.lyric_viewport_h), true);
			if (has_line)
			{
				const ImVec2 indicator(card_min.x + layout.pad + 1.0f, text_y + layout.lyric_font * 0.5f);
				draw->AddCircleFilled(indicator, 6.0f, IM_COL32(30, 215, 96, 16), 0);
				draw->AddRectFilled(ImVec2(indicator.x - 1.0f, text_y + 2.0f),
					ImVec2(indicator.x + 1.0f, text_y + layout.lyric_font - 1.0f), color_brand, 1.0f);
				for (const ImVec2 offset : { ImVec2(-1, 0), ImVec2(1, 0), ImVec2(0, -1), ImVec2(0, 1) })
					draw->AddText(line_font, layout.lyric_font, ImVec2(text_x + offset.x, text_y + offset.y),
						IM_COL32(115, 255, 168, static_cast<int>(14.0f * fade)), line.c_str());
			}
			draw->AddText(line_font, layout.lyric_font, ImVec2(text_x, text_y),
				has_line ? IM_COL32(241, 255, 246, static_cast<int>(255.0f * fade)) : color_artist, line.c_str());
			draw->PopClipRect();
		}
	}
}

void modules::spotify_hud::run(::cache&)
{
    auto settings_snapshot = settings_.read();
	// Ensure background worker is active
	if (!spotify::source::instance().is_running())
	{
		applied_lyrics = settings_snapshot.show_lyrics && settings_snapshot.enable_lyrics;
		spotify::source::instance().start(applied_lyrics);
	}

	const bool wants_lyrics = settings_snapshot.show_lyrics && settings_snapshot.enable_lyrics;
	if (applied_lyrics != wants_lyrics)
	{
		applied_lyrics = wants_lyrics;
		spotify::source::instance().set_lyrics_enabled(applied_lyrics);
	}
}

void modules::spotify_hud::on_enable(::cache&)
{
    auto settings_snapshot = settings_.read();
	applied_lyrics = settings_snapshot.show_lyrics && settings_snapshot.enable_lyrics;
	spotify::source::instance().start(applied_lyrics);
}

void modules::spotify_hud::on_disable(::cache&)
{
	spotify::source::instance().stop();
}

modules::spotify_hud::bounds modules::spotify_hud::get_bounds(float display_w, float display_h) const
{
    auto settings_snapshot = settings_.read();
	const float size_scale = fixed_scale;
	const spotify::now_playing info = spotify::source::instance().snapshot();
	const bool is_idle_state = (!info.active || info.title.empty()) && !gui::draw;

	float width = 0.0f;
	float height = 0.0f;
	if (is_idle_state && settings_snapshot.show_when_idle)
	{
		width = std::round(190.0f * size_scale);
		height = std::round(40.0f * size_scale);
	}
	else
	{
		const auto layout = compute_layout(size_scale, settings_snapshot.show_artwork, settings_snapshot.show_progress, settings_snapshot.show_lyrics, 1);
		width = layout.width;
		height = layout.height;
	}

	const float x = std::round(std::clamp(settings_snapshot.position_x, 4.0f, (std::max)(4.0f, display_w - width - 4.0f)));
	const float y = std::round(std::clamp(settings_snapshot.position_y, 4.0f, (std::max)(4.0f, display_h - height - 4.0f)));
	return { x, y, width, height };
}

void modules::spotify_hud::render_hud_preview(ImDrawList* draw, float x, float y)
{
    auto settings_snapshot = settings_.read();
	if (!draw) return;
	const float size_scale = fixed_scale;
	const spotify::now_playing info = spotify::source::instance().snapshot();
	if (info.active && !info.title.empty())
	{
		draw_card(draw, x, y, size_scale, settings_snapshot.background_opacity, settings_snapshot.show_artwork, settings_snapshot.show_progress,
			settings_snapshot.show_visualizer, settings_snapshot.show_brand, settings_snapshot.show_lyrics, settings_snapshot.lyric_lines, info, false);
	}
	else
	{
		spotify::now_playing preview{};
		preview.active = true;
		preview.playing = true;
		preview.title = "Starboy";
		preview.artist = "The Weeknd, Daft Punk";
		preview.position = 74.0;
		preview.duration = 230.0;
		preview.lyrics_loaded = true;
		preview.lyrics = {
			{ 70.0, "I'm tryna put you in the worst mood, ah" },
			{ 74.0, "P1 cleaner than your church shoes, ah" },
			{ 78.0, "Milli point two just to hurt you, ah" }
		};
		draw_card(draw, x, y, size_scale, settings_snapshot.background_opacity, settings_snapshot.show_artwork, settings_snapshot.show_progress,
			settings_snapshot.show_visualizer, settings_snapshot.show_brand, settings_snapshot.show_lyrics, settings_snapshot.lyric_lines, preview, false);
	}
}

void modules::spotify_hud::render_overlay()
{
    auto settings_snapshot = settings_.read();
	if (gui::hud_editor_active) return;

	const ImVec2 display = ImGui::GetIO().DisplaySize;
	if (display.x < 40.0f || display.y < 40.0f) return;

	// Keep background service alive while module is active
	if (!spotify::source::instance().is_running())
	{
		applied_lyrics = settings_snapshot.show_lyrics && settings_snapshot.enable_lyrics;
		spotify::source::instance().start(applied_lyrics);
	}

	// Overlays must render on the foreground draw list so they remain visible on top of Minecraft and the UI
	ImDrawList* draw = ImGui::GetForegroundDrawList();
	if (!draw) return;

	const spotify::now_playing info = spotify::source::instance().snapshot();
	const bool has_track = info.active && !info.title.empty();
	const float size_scale = fixed_scale;

	float width = 0.0f;
	float height = 0.0f;
	if (!has_track && !gui::draw && settings_snapshot.show_when_idle)
	{
		width = std::round(190.0f * size_scale);
		height = std::round(40.0f * size_scale);
	}
	else
	{
		const auto layout = compute_layout(size_scale, settings_snapshot.show_artwork, settings_snapshot.show_progress, settings_snapshot.show_lyrics, 1);
		width = layout.width;
		height = layout.height;
	}

	const float x = std::clamp(settings_snapshot.position_x, 4.0f, (std::max)(4.0f, display.x - width - 4.0f));
	const float y = std::clamp(settings_snapshot.position_y, 4.0f, (std::max)(4.0f, display.y - height - 4.0f));

	if (has_track)
	{
		draw_card(draw, x, y, size_scale, settings_snapshot.background_opacity, settings_snapshot.show_artwork, settings_snapshot.show_progress,
			settings_snapshot.show_visualizer, settings_snapshot.show_brand, settings_snapshot.show_lyrics, settings_snapshot.lyric_lines, info, false);
	}
	else if (gui::draw)
	{
		// In ClickGUI: display interactive preview card so the user can see their adjustments live
		spotify::now_playing preview{};
		preview.active = true;
		preview.playing = true;
		preview.title = "Starboy";
		preview.artist = "The Weeknd, Daft Punk";
		preview.position = 74.0;
		preview.duration = 230.0;
		preview.lyrics_loaded = true;
		preview.lyrics = {
			{ 70.0, "I'm tryna put you in the worst mood, ah" },
			{ 74.0, "P1 cleaner than your church shoes, ah" },
			{ 78.0, "Milli point two just to hurt you, ah" }
		};
		draw_card(draw, x, y, size_scale, settings_snapshot.background_opacity, settings_snapshot.show_artwork, settings_snapshot.show_progress,
			settings_snapshot.show_visualizer, settings_snapshot.show_brand, settings_snapshot.show_lyrics, settings_snapshot.lyric_lines, preview, false);
	}
	else if (settings_snapshot.show_when_idle)
	{
		// In-game without active music: display compact idle badge
		draw_card(draw, x, y, size_scale, settings_snapshot.background_opacity, settings_snapshot.show_artwork, settings_snapshot.show_progress,
			settings_snapshot.show_visualizer, settings_snapshot.show_brand, settings_snapshot.show_lyrics, settings_snapshot.lyric_lines, info, true);
	}
}

void modules::spotify_hud::render_options()
{
    gui::settings::edit(settings_, module_settings::spotify_hud::schema(), [&](auto& candidate) {
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	const float max_x = display.x > 100.0f ? display.x : 1920.0f;
	const float max_y = display.y > 100.0f ? display.y : 1080.0f;

	// All controls below are redirected to OneStep widgets via ui_redirect.hpp,
	// ensuring identical styling, colors, and animations with the rest of the client modules.
	ImGui::SliderFloat("Position X", &candidate.position_x, 0.0f, max_x, "%.0f px");
	ImGui::SliderFloat("Position Y", &candidate.position_y, 0.0f, max_y, "%.0f px");
    gui::settings::render(candidate, module_settings::spotify_hud::schema().subspan(2));

    });
}
