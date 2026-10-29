#include "../../ui/settings_renderer.hpp"
#include "jump_reset.hpp"
#include "../../services/game/world_bridge.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"
#include <imgui/imgui.h>

void modules::jump_reset::run(::cache&) {}

void modules::jump_reset::request_reset()
{
    std::lock_guard lock(settings_mutex);
    reset_generation.fetch_add(1, std::memory_order_release);
}
void modules::jump_reset::on_enable(::cache&) { request_reset(); }
void modules::jump_reset::on_disable(::cache&) { request_reset(); }
void modules::jump_reset::cancel_on_client_thread()
{
    services::game::world_bridge::cancel_jump();
}
void modules::jump_reset::render_overlay()
{
    // Java owns game reads, damage decisions, timing and binding cleanup.
    // Serialize settings publication with dispatch. Java checks the cancellation
    // generation again immediately before writing the game binding.
    std::lock_guard lock(settings_mutex);
    services::game::world_bridge::tick_jump(settings, enabled, reset_generation);
}
jump_reset_config::config modules::jump_reset::settings_snapshot()
{
    std::lock_guard lock(settings_mutex); return settings;
}
void modules::jump_reset::apply_settings(jump_reset_config::config candidate, bool force_reset)
{
    if (!jump_reset_config::normalize(candidate)) return;
    std::lock_guard lock(settings_mutex);
    if (force_reset || !jump_reset_config::same_config(candidate, settings)) {
        settings = candidate;
        reset_generation.fetch_add(1, std::memory_order_release);
    }
}

void modules::jump_reset::render_options()
{
    ImGui::PushID("jump_reset_settings");
    auto candidate = settings_snapshot();
    gui::option_section("Timing");
    const char* profile_names[] = {
        "Custom",
        "Immediate",
        "Balanced",
        "Controlled"
    };
    int selected = static_cast<int>(jump_reset_config::selected_profile(candidate));
    const float width = ImGui::GetContentRegionAvail().x;
    const float spacing = 6.f;
    const float button_width = (width - 2.f * spacing) / 3.f;
    bool presets_fit = width >= 280.f;
    for (int index = 1; index <= 3; ++index)
        presets_fit = presets_fit && ImGui::CalcTextSize(profile_names[index]).x + 16.f <= button_width;
    if (presets_fit)
    {
        for (int index = 1; index <= 3; ++index)
        {
            ImGui::PushID(index);
            if (gui::action_button(profile_names[index], ImVec2(button_width, 28.f),
                selected == index ? gui::action_tone::accent : gui::action_tone::neutral))
            {
                jump_reset_config::apply_profile(candidate, static_cast<jump_reset_config::profile>(index));
                selected = index;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                const int chances[] = {0, 100, 85, 65}, delays[] = {0, 0, 15, 35};
                ImGui::Text("%d%% activation / %d ms delay",
                    chances[index], delays[index]);
                ImGui::EndTooltip();
            }
            ImGui::PopID();
            if (index < 3) ImGui::SameLine(0, spacing);
        }
    }
    else if (gui::option_combo("Preset", &selected, profile_names, 4))
        jump_reset_config::apply_profile(candidate, static_cast<jump_reset_config::profile>(selected));

    gui::settings::render(candidate, jump_reset_config::schema().subspan(1, 2));
    gui::option_section("Input / conditions");
    gui::settings::render(candidate, jump_reset_config::schema().first(1));
    gui::settings::render(candidate, jump_reset_config::schema().last(1));

    ImGui::Spacing();
    if (gui::action_button("Restore Defaults",
        ImVec2(ImGui::GetContentRegionAvail().x, 28.f)))
        candidate = jump_reset_config::config{};
    if (jump_reset_config::normalize(candidate)) apply_settings(candidate);
    ImGui::PopID();
}
