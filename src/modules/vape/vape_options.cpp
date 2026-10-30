#include "vape_module.hpp"
#include "../../ui/settings_renderer.hpp"

float modules::vape_module::get_options_height()
{
    switch (kind) {
    case vape_kind::aim: return 470.f;
    }
    return 45.f;
}

void modules::vape_module::render_options()
{
    if (gui::settings::render(settings, vape_settings::schema(kind))) {
        reset_requested.store(true);
    }
    if (enabled) if (const auto* reason = conflict()) ImGui::TextWrapped("%s", reason);
    if (enabled && !runtime_issue.empty()) ImGui::TextWrapped("%s", runtime_issue.c_str());
}
