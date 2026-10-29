#include "storage_esp.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../../services/game/loaded_chunks.hpp"
#include "../ui_redirect.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_set>

#include <imgui/imgui.h>

#include "../../gui/gui.hpp"


namespace
{
	bool camera_snapshot(const services::game::client_context& context, double& x, double& y, double& z,
		float& yaw, float& pitch, float& fov)
	{
		const auto& instance = context.instance;
		const auto& player = context.player;
		auto& adapter = platform::adapter::get();
		float tick_delta = adapter.get_tick_delta(instance);
		auto cam = adapter.get_camera_data(instance, player, tick_delta);
		if (platform::jni_context::clear_exception() || !cam.valid) return false;
		x = cam.pos.x;
		y = cam.pos.y;
		z = cam.pos.z;
		yaw = cam.yaw;
		pitch = cam.pitch;
		fov = cam.fov;
		return true;
	}

	bool project(double wx, double wy, double wz, double cx, double cy, double cz,
		float yaw, float pitch, float width, float height, float fov, float& sx, float& sy)
	{
		constexpr double pi = 3.14159265358979323846;
		const double rx = wx - cx, ry = wy - cy, rz = wz - cz;
		const double yaw_r = yaw * pi / 180.0, pitch_r = pitch * pi / 180.0;
		const double syaw = std::sin(yaw_r), cyaw = std::cos(yaw_r);
		const double sp = std::sin(pitch_r), cp = std::cos(pitch_r);
		const double vx = rx * -cyaw + rz * -syaw;
		const double vy = rx * -syaw * sp + ry * cp + rz * cyaw * sp;
		const double vz = rx * -syaw * cp - ry * sp + rz * cyaw * cp;
		if (vz < 0.08) return false;
		const double focal = (height * .5) / std::tan(std::clamp(fov, 1.0f, 179.0f) * pi / 360.0);
		sx = static_cast<float>(width * .5 + vx * focal / vz);
		sy = static_cast<float>(height * .5 - vy * focal / vz);
		return std::isfinite(sx) && std::isfinite(sy);
	}

	const char* storage_label(const std::string& key)
	{
		if (key.find("ender_chest") != std::string::npos) return "Ender Chest";
		if (key.find("trapped_chest") != std::string::npos) return "Trapped Chest";
		if (key.find("shulker_box") != std::string::npos) return "Shulker Box";
		if (key.find("barrel") != std::string::npos) return "Barrel";
		return "Chest";
	}

	bool is_storage_key(const std::string& key)
	{
		return key.find("trapped_chest") != std::string::npos ||
			key.find("ender_chest") != std::string::npos ||
			key.find("shulker_box") != std::string::npos ||
			key.find("barrel") != std::string::npos ||
			key.ends_with(".chest") || key.ends_with("_chest");
	}

	ImU32 storage_color(const std::string& label)
	{
		if (label == "Ender Chest") return IM_COL32(166, 104, 255, 245);
		if (label == "Trapped Chest") return IM_COL32(255, 102, 105, 245);
		if (label == "Shulker Box") return IM_COL32(218, 116, 255, 245);
		if (label == "Barrel") return IM_COL32(235, 174, 92, 245);
		return IM_COL32(255, 194, 92, 245);
	}
}

void modules::storage_esp::run(::cache& cache)
{
    if (!cache.is_valid() || !cache.player || !cache.world || !jni::get_env()) { clear_snapshot(); return; }
    const auto now = std::chrono::steady_clock::now();
    if (now < next_scan) return;
    next_scan = now + std::chrono::milliseconds(50);
    try
    {
        jni::frame frame{96};
        std::uint64_t generation;
        std::size_t cursor;
        {
            std::scoped_lock lock(marker_mutex);
            if (session_.observe({cache.world, cache.player})) { markers.clear(); scan_index = 0; }
            generation = session_.ticket();
            cursor = scan_index;
        }
        services::game::loaded_chunks chunks;
        if (!services::game::capture_loaded_chunks(cache.world, chunks)) { clear_snapshot(); return; }
        std::vector<marker> scanned;
        std::unordered_set<std::uint64_t> refreshed;
        const std::size_t count = chunks.slots.size();
        const std::size_t batch = std::min<std::size_t>(count, 32);
        for (std::size_t i = 0; i < batch; ++i)
        {
            jni::frame chunk_frame{48};
            const auto& slot = chunks.slots[(cursor + i) % count];
            refreshed.insert(services::game::chunk_key(slot.x, slot.z));
            auto chunk = services::game::read_loaded_chunk(chunks, slot);
            if (!chunk) continue;
            auto entities = chunk.getBlockEntities.call();
            auto values = entities ? entities.values.call() : maps::Collection{};
            auto array = values ? values.toArray.call() : jni::array<maps::Object>{nullptr};
            if (platform::jni_context::clear_exception() || !array) { clear_snapshot(); return; }
            const int length = array.get_length();
            if (platform::jni_context::clear_exception()) { clear_snapshot(); return; }
            for (int index = 0; index < length; ++index)
            {
                jni::frame entity_frame{48};
                maps::BlockEntity entity(jni::get_env()->GetObjectArrayElement(
                    static_cast<jobjectArray>(array.object_instance), index), false);
                if (!entity) continue;
                const bool removed = entity.isRemoved.call() == JNI_TRUE;
                auto pos = entity.getPos.call();
                auto state = entity.getCachedState.call();
                auto block = state ? state.getBlock.call() : maps::Block{};
                auto item = block ? block.asItem.call() : maps::Item{};
                auto name = item ? item.getTranslationKey.call() : maps::String{};
                if (platform::jni_context::clear_exception() || removed || !pos || !name) continue;
                const std::string key = name.to_string();
                if (!is_storage_key(key)) continue;
                const int x = pos.getX.call(), y = pos.getY.call(), z = pos.getZ.call();
                if (platform::jni_context::clear_exception()) continue;
                scanned.push_back({double(x), double(y), double(z), storage_label(key)});
            }
        }
        std::scoped_lock lock(marker_mutex);
        if (!session_.accepts(generation)) return;
        markers.erase(std::remove_if(markers.begin(), markers.end(), [&](const marker& old)
        {
            const auto key = services::game::chunk_key(
                static_cast<int>(std::floor(old.x / 16.)), static_cast<int>(std::floor(old.z / 16.)));
            return !chunks.keys.contains(key) || refreshed.contains(key);
        }), markers.end());
        markers.insert(markers.end(), scanned.begin(), scanned.end());
        scan_index = count ? (cursor + batch) % count : 0;
    }
    catch (...) { platform::jni_context::clear_exception(); clear_snapshot(); }
}

void modules::storage_esp::render_overlay()
{
    auto settings_snapshot = settings_.read();
    if (!platform::jni_context::bind_current_thread()) { clear_snapshot(); return; }
    jni::frame render_frame{96};
    services::game::client_context context;
    if (!services::game::capture_client_context(context, true, false)) { clear_snapshot(); return; }
	double cx = 0.0, cy = 0.0, cz = 0.0;
	float yaw = 0.0f, pitch = 0.0f, fov = 70.0f;
	{
		jni::frame frame{ 48 };
		if (!camera_snapshot(context, cx, cy, cz, yaw, pitch, fov)) { clear_snapshot(); return; }
	}
	std::vector<marker> draw_markers;
	{
		std::scoped_lock lock(marker_mutex);
        if (session_.observe({context.world, context.player})) {
            markers.clear(); scan_index = 0; return;
        }
		draw_markers = markers;
	}
	if (draw_markers.empty()) return;

	const ImVec2 display = ImGui::GetIO().DisplaySize;
	ImDrawList* draw = ImGui::GetBackgroundDrawList();
	for (const marker& entry : draw_markers)
	{
		float corners_x[8]{}, corners_y[8]{};
		bool corner_valid[8]{};
		int projected = 0;
		float min_x = 1.0e9f, min_y = 1.0e9f, max_x = -1.0e9f, max_y = -1.0e9f;
		for (int x = 0; x <= 1; ++x) for (int y = 0; y <= 1; ++y) for (int z = 0; z <= 1; ++z)
		{
			const int corner = x * 4 + y * 2 + z;
			float sx = 0.0f, sy = 0.0f;
			if (!project(entry.x + x, entry.y + y, entry.z + z,
				cx, cy, cz, yaw, pitch, display.x, display.y, fov, sx, sy)) continue;
			min_x = std::min(min_x, sx); min_y = std::min(min_y, sy);
			max_x = std::max(max_x, sx); max_y = std::max(max_y, sy);
			corners_x[corner] = sx; corners_y[corner] = sy; corner_valid[corner] = true; ++projected;
		}
		if (projected < 4 || max_x < 0.0f || min_x > display.x || max_y < 0.0f || min_y > display.y)
			continue;

		const ImU32 color = storage_color(entry.label);
		if (settings_snapshot.box_style == 0)
		{
			draw->AddRectFilled(ImVec2(min_x, min_y), ImVec2(max_x, max_y),
				(color & 0x00FFFFFFu) | 0x1E000000u, 2.0f);
			draw->AddRect(ImVec2(min_x, min_y), ImVec2(max_x, max_y), color, 2.0f, 0, 1.4f);
		}
		else
		{
			static constexpr int edges[12][2] = {
				{ 0, 1 }, { 0, 2 }, { 0, 4 }, { 1, 3 }, { 1, 5 }, { 2, 3 },
				{ 2, 6 }, { 3, 7 }, { 4, 5 }, { 4, 6 }, { 5, 7 }, { 6, 7 }
			};
			for (const auto& edge : edges)
			{
				if (!corner_valid[edge[0]] || !corner_valid[edge[1]]) continue;
				draw->AddLine(ImVec2(corners_x[edge[0]], corners_y[edge[0]]),
					ImVec2(corners_x[edge[1]], corners_y[edge[1]]), IM_COL32(0, 0, 0, 190), 3.4f);
				draw->AddLine(ImVec2(corners_x[edge[0]], corners_y[edge[0]]),
					ImVec2(corners_x[edge[1]], corners_y[edge[1]]), color, 1.5f);
			}
		}

		const double dx = entry.x + .5 - cx, dy = entry.y + .5 - cy, dz = entry.z + .5 - cz;
		const int distance = static_cast<int>(std::sqrt(dx * dx + dy * dy + dz * dz));
		char text[96]{};
		if (settings_snapshot.show_labels && settings_snapshot.show_distance) std::snprintf(text, sizeof(text), "%s  %dm", entry.label.c_str(), distance);
		else if (settings_snapshot.show_labels) std::snprintf(text, sizeof(text), "%s", entry.label.c_str());
		else if (settings_snapshot.show_distance) std::snprintf(text, sizeof(text), "%dm", distance);
		if (text[0] == '\0') continue;
		const ImVec2 text_size = ImGui::CalcTextSize(text);
		const float text_x = (min_x + max_x - text_size.x) * 0.5f;
		const float text_y = min_y - text_size.y - 5.0f;
		draw->AddRectFilled(ImVec2(text_x - 5.0f, text_y - 2.0f),
			ImVec2(text_x + text_size.x + 5.0f, text_y + text_size.y + 2.0f),
			IM_COL32(7, 9, 13, 210), 4.0f);
		draw->AddText(ImVec2(text_x, text_y), color, text);
	}
}

void modules::storage_esp::clear_snapshot()
{
    std::scoped_lock lock(marker_mutex);
    if (jni::get_env()) session_.reset(); else session_.invalidate();
    markers.clear(); scan_index = 0;
}

void modules::storage_esp::on_disable(::cache&)
{
    clear_snapshot();
}

void modules::storage_esp::render_options()
{
    gui::settings::render(settings_, module_settings::storage_esp::schema());
}
