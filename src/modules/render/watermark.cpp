#include "../../ui/settings_renderer.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "watermark.hpp"
#include "../../gui/watermark_draw.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"
#include "../../friends/friends.hpp"

#include <Windows.h>
#include <imgui/imgui.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>

void modules::watermark::run(::cache& cache)
{
    const auto now = std::chrono::steady_clock::now();
    if (now < next_status_refresh) return;
    next_status_refresh = now + std::chrono::milliseconds(500);

    std::string current_name = friends::local_player_name();
    int current_latency = 0;
    {
        std::lock_guard lock(status_mutex);
        current_latency = latency_ms;
    }

    bool latency_resolved = false;
    try
    {
        if (cache.is_valid() && cache.player)
        {
            maps::GameProfile local_profile = cache.player.getGameProfile.call();
            maps::String local_name = local_profile ? local_profile.getName.call() : maps::String{};
            if (local_name && !platform::jni_context::clear_exception()) current_name = local_name.to_string();

            maps::ClientPlayNetworkHandler handler = cache.instance.getNetworkHandler.call();
            maps::PlayerListEntry local_entry =
                (handler && local_name) ? handler.getPlayerListEntryByName.call(local_name) : maps::PlayerListEntry{};

            if (!platform::jni_context::clear_exception() && local_entry)
            {
                const int measured_latency = static_cast<int>(local_entry.getLatency.call());
                if (!platform::jni_context::clear_exception())
                {
                    current_latency = std::clamp(measured_latency, 0, 9999);
                    latency_resolved = true;
                }
            }

            // Some proxy/mod combinations briefly return no direct entry while the
            // player list is being rebuilt. Keep the collection lookup as a safe
            // fallback, but never replace the last valid ping on a failed refresh.
            if (!latency_resolved && handler)
            {
                maps::Collection players = handler.getPlayerList.call();
                jni::array<maps::Object> entries = players ? players.toArray.call() : jni::array<maps::Object>{ nullptr };
                if (entries && !platform::jni_context::clear_exception())
                {
                    for (const maps::Object& object : entries.to_vector())
                    {
                        maps::PlayerListEntry entry(object.object_instance, false);
                        maps::GameProfile profile = entry ? entry.getProfile.call() : maps::GameProfile{};
                        maps::String name = profile ? profile.getName.call() : maps::String{};
                        if (!name || platform::jni_context::clear_exception() || name.to_string() != current_name) continue;

                        const int measured_latency = static_cast<int>(entry.getLatency.call());
                        if (!platform::jni_context::clear_exception())
                        {
                            current_latency = std::clamp(measured_latency, 0, 9999);
                            latency_resolved = true;
                        }
                        break;
                    }
                }
            }
        }
    }
    catch (...)
    {
        platform::jni_context::clear_exception();
    }

    if (current_name.empty()) current_name = "Player";
    std::lock_guard lock(status_mutex);
    player_name = std::move(current_name);
    latency_ms = current_latency;
}

void modules::watermark::render_overlay()
{
	if (gui::hud_editor_active) return;
	const ImVec2 size = gui::watermark_draw::size();
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	const float x = std::clamp(std::round(gui::hud_pos_x), 8.f, std::max(8.f, display.x - size.x - 8.f));
	const float y = std::clamp(std::round(gui::hud_pos_y), 8.f, std::max(8.f, display.y - size.y - 8.f));
	gui::watermark_draw::draw(ImGui::GetForegroundDrawList(), x, y);
}

void modules::watermark::render_options()
{
    ImGui::TextDisabled("%s", "Displays onestep logo watermark");
    ImGui::Spacing();
    gui::settings::render(gui::watermark_settings, module_settings::watermark::schema());
}

std::pair<std::string, int> modules::watermark::status_snapshot() const
{
	std::lock_guard lock(status_mutex);
	return { player_name, latency_ms };
}
