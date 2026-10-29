#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include "../../services/game/anti_bot_service.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/overlay_context.hpp"
#include <chrono>
#include <cmath>
#include <mutex>
#include <string>
#include <vector>

#include <imgui/imgui.h>

#include "radar.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../ui_redirect.hpp"
#include "../../friends/friends.hpp"
#include "../../gui/gui.hpp"

namespace
{
	struct radar_player
	{
		float relative_x = 0.0f;
		float relative_z = 0.0f;
		float distance = 0.0f;
		std::string name;
		bool friend_player = false;
	};

	std::mutex radar_mutex;
    services::game::overlay_session session;
	std::vector<radar_player> radar_players;
	float radar_yaw = 0.0f;
	std::chrono::steady_clock::time_point next_update{};

	void clear_snapshot()
	{
		std::lock_guard lock(radar_mutex);
        if (jni::get_env()) session.reset(); else session.invalidate();
		radar_players.clear();
        radar_yaw = 0.f;
	}

	ImVec2 rotate_point(float relative_x, float relative_z, float yaw, bool rotate)
	{
		if (!rotate) return ImVec2(relative_x, relative_z);
		const float radians = yaw * 0.01745329251994329577f;
		const float sine = std::sin(radians);
		const float cosine = std::cos(radians);
		return ImVec2(
			relative_x * cosine + relative_z * sine,
			relative_x * sine - relative_z * cosine);
	}
}

void modules::radar::run(::cache& cache)
{
	if (!cache.is_valid() || !cache.player || !cache.world || !jni::get_env())
	{
		clear_snapshot();
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	if (now < next_update) return;
	next_update = now + std::chrono::milliseconds(32);

	try
	{
		jni::frame frame{ 160 };
        std::uint64_t generation;
        {
            std::lock_guard lock(radar_mutex);
            if (session.observe({cache.world, cache.player})) radar_players.clear();
            generation = session.ticket();
        }
		const maths::vector3d local_position = cache.player.get_position();
		const float local_yaw = cache.player.yaw.get();
		if (platform::jni_context::clear_exception()) return;

		std::vector<radar_player> updated;
		auto process_entity = [&](const maps::Entity& entity)
		{
			if (!entity || entity.is_same_object(cache.player) ||
				!entity.is_instance_of<maps::PlayerEntity>() ||
				entity.isAlive.call() != JNI_TRUE || platform::jni_context::clear_exception())
				return;
			if (services::anti_bot::is_bot(entity)) return;
			const maths::vector3d position = platform::adapter::get().get_position(entity);
			const double delta_x = position.x - local_position.x;
			const double delta_z = position.z - local_position.z;
			const double distance = std::hypot(delta_x, delta_z);
			if (!std::isfinite(distance) || distance > 256.0) return;
			maps::String java_name = entity.getEntityName.call();
			std::string name = java_name && !platform::jni_context::clear_exception()
				? java_name.to_string() : std::string{};
			platform::jni_context::clear_exception();
			updated.push_back({
				static_cast<float>(delta_x), static_cast<float>(delta_z),
				static_cast<float>(distance), name, friends::is_friend(name)
			});
		};

		maps::List players = cache.players ? cache.players : platform::adapter::get().get_players(cache.world);
		bool processed = false;
		if (players)
		{
			jni::array<maps::Object> array = players.toArray.call();
			if (array && !platform::jni_context::clear_exception())
			{
				for (const maps::Object& object : array.to_vector())
				{
					maps::Entity entity(object.object_instance, false);
					process_entity(entity);
				}
				processed = true;
			}
		}

		if (!processed)
		{
			auto iterable = platform::adapter::get().get_entities(cache.world);
			if (iterable)
			{
				auto iterator = iterable.iterator.call();
				if (iterator)
				{
					while (iterator.hasNext.call() == JNI_TRUE)
					{
						auto obj = iterator.next.call();
						if (obj)
						{
							maps::Entity entity(obj.object_instance, false);
							process_entity(entity);
						}
					}
				}
			}
		}

		std::lock_guard lock(radar_mutex);
        if (!session.accepts(generation)) return;
		radar_players = std::move(updated);
		float active_yaw = local_yaw;
		float td = platform::adapter::get().get_tick_delta(cache.instance);
		auto cam = platform::adapter::get().get_camera_data(cache.instance, cache.player, td);
		if (cam.valid) active_yaw = cam.yaw;
		radar_yaw = active_yaw;
	}
	catch (...) { platform::jni_context::clear_exception(); clear_snapshot(); }
}

void modules::radar::render_overlay()
{
    auto settings_snapshot = settings_.read();
    if (!platform::jni_context::bind_current_thread()) { clear_snapshot(); return; }
    jni::frame render_frame{64};
    services::game::client_context context;
    if (!services::game::capture_client_context(context, true, false)) { clear_snapshot(); return; }
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	if (display.x < 10.0f || display.y < 10.0f) return;

	std::vector<radar_player> players;
	float yaw = 0.0f;
	{
		std::lock_guard lock(radar_mutex);
        if (session.observe({context.world, context.player})) { radar_players.clear(); radar_yaw = 0.f; return; }
		players = radar_players;
		yaw = radar_yaw;
	}

	const float diameter = std::clamp(settings_snapshot.radar_size, 120.0f, 260.0f);
	const float radius = diameter * 0.5f;
	const float x = std::clamp(settings_snapshot.position_x, 6.0f,
		std::max(6.0f, display.x - diameter - 6.0f));
	const float y = std::clamp(settings_snapshot.position_y, 6.0f,
		std::max(6.0f, display.y - diameter - 6.0f));
	const ImVec2 center(x + radius, y + radius);
	const float range = std::clamp(settings_snapshot.radar_range, 8.0f, 128.0f);
	const float usable_radius = radius - 12.0f;
	const float opacity = std::clamp(settings_snapshot.background_opacity, 0.20f, 1.0f);
	ImDrawList* draw = ImGui::GetBackgroundDrawList();
	if (!draw) return;

	const ImColor accent = gui::get_dynamic_color();
	const ImU32 accent_color = ImGui::ColorConvertFloat4ToU32(ImVec4(
		accent.Value.x, accent.Value.y, accent.Value.z, 0.94f));
	const ImU32 accent_soft = ImGui::ColorConvertFloat4ToU32(ImVec4(
		accent.Value.x, accent.Value.y, accent.Value.z, 0.16f));

	// Tek parca, sade ve yumusak dairesel yuzey.
	draw->AddCircleFilled(ImVec2(center.x + 3.0f, center.y + 4.0f), radius,
		IM_COL32(0, 0, 0, static_cast<int>(85 * opacity)), 0);
	draw->AddCircleFilled(center, radius,
		ImGui::ColorConvertFloat4ToU32(ImVec4(0.025f, 0.028f, 0.037f, opacity * 0.88f)), 0);
	draw->AddCircle(center, radius - 0.5f, IM_COL32(112, 118, 136, 80), 0, 1.0f);
	draw->AddCircle(center, radius - 2.0f, accent_soft, 0, 1.0f);

	if (settings_snapshot.show_distance_rings)
	{
		draw->AddCircle(center, usable_radius * 0.5f, IM_COL32(132, 137, 155, 32), 0, 1.0f);
		draw->AddCircle(center, usable_radius * 0.76f, IM_COL32(132, 137, 155, 22), 0, 1.0f);
	}
	draw->AddLine(ImVec2(center.x - usable_radius, center.y),
		ImVec2(center.x + usable_radius, center.y), IM_COL32(132, 137, 155, 27), 1.0f);
	draw->AddLine(ImVec2(center.x, center.y - usable_radius),
		ImVec2(center.x, center.y + usable_radius), IM_COL32(132, 137, 155, 27), 1.0f);

	if (settings_snapshot.show_cardinals)
	{
		const ImU32 quiet = IM_COL32(145, 150, 168, 175);
		draw->AddText(ImVec2(center.x - 4.0f, center.y - usable_radius + 3.0f), accent_color, "N");
		draw->AddText(ImVec2(center.x - 4.0f, center.y + usable_radius - 16.0f), quiet, "S");
		draw->AddText(ImVec2(center.x + usable_radius - 12.0f, center.y - 7.0f), quiet, "E");
		draw->AddText(ImVec2(center.x - usable_radius + 4.0f, center.y - 7.0f), quiet, "W");
	}

	std::size_t visible_count = 0;
	for (const radar_player& player : players)
	{
		const ImVec2 local = rotate_point(player.relative_x, player.relative_z, yaw, settings_snapshot.rotate_with_player);
		const float length = std::sqrt(local.x * local.x + local.y * local.y);
		const bool outside = length > range;
		const float scale = usable_radius / std::max(range, 0.001f);
		const float clamped_distance = std::min(length, range);
		const float direction_x = length > 0.001f ? local.x / length : 0.0f;
		const float direction_y = length > 0.001f ? local.y / length : 0.0f;
		const ImVec2 point(center.x + direction_x * clamped_distance * scale,
			center.y + direction_y * clamped_distance * scale);
		const ImU32 color = player.friend_player ? IM_COL32(75, 226, 155, 245) : accent_color;

		if (outside)
		{
			const ImVec2 side(-direction_y, direction_x);
			draw->AddTriangleFilled(
				ImVec2(point.x + direction_x * 3.5f, point.y + direction_y * 3.5f),
				ImVec2(point.x - direction_x * 3.0f + side.x * 2.8f,
					point.y - direction_y * 3.0f + side.y * 2.8f),
				ImVec2(point.x - direction_x * 3.0f - side.x * 2.8f,
					point.y - direction_y * 3.0f - side.y * 2.8f), color);
			continue;
		}

		++visible_count;
		draw->AddCircleFilled(point, 5.0f, player.friend_player
			? IM_COL32(75, 226, 155, 28) : accent_soft, 0);
		draw->AddCircleFilled(point, 2.4f, color, 0);
		if (settings_snapshot.show_names && !player.name.empty())
		{
			const auto display_name=player.name;
			const ImVec2 text_size = ImGui::CalcTextSize(display_name.c_str());
			draw->AddText(ImVec2(point.x - text_size.x * 0.5f, point.y + 5.0f),
				IM_COL32(226, 229, 238, 220), display_name.c_str());
		}
	}

	// Merkez oyuncu isareti.
	draw->AddCircleFilled(center, 5.0f, IM_COL32(4, 5, 8, 235), 0);
	draw->AddTriangleFilled(ImVec2(center.x, center.y - 4.0f),
		ImVec2(center.x - 3.3f, center.y + 3.0f),
		ImVec2(center.x + 3.3f, center.y + 3.0f), IM_COL32(240, 242, 248, 250));

	if (settings_snapshot.show_player_count)
	{
		char count[24]{};
		sprintf_s(count, "%zu", visible_count);
		const ImVec2 text_size = ImGui::CalcTextSize(count);
		const ImVec2 badge(center.x, center.y + radius - 10.0f);
		draw->AddCircleFilled(badge, 9.0f, IM_COL32(19, 21, 29, 225), 0);
		draw->AddText(ImVec2(badge.x - text_size.x * 0.5f, badge.y - text_size.y * 0.5f),
			IM_COL32(184, 188, 202, 235), count);
	}
}

void modules::radar::render_options()
{
    gui::settings::render(settings_, module_settings::radar::schema());
}

void modules::radar::on_disable(::cache&)
{
	clear_snapshot();
	next_update = {};
}
