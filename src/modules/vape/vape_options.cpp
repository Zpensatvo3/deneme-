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
    std::lock_guard lock(settings_mutex);
    auto candidate = settings;
    gui::settings::render(candidate, vape_settings::schema(kind));
    if (!(candidate == settings) && vape_settings::normalize(kind, candidate)) {
        settings = candidate;
        reset_requested.store(true);
    }
    if (enabled) if (const auto* reason = conflict()) ImGui::TextWrapped("%s", reason);
    if (enabled && !runtime_issue.empty()) ImGui::TextWrapped("%s", runtime_issue.c_str());
}

void modules::vape_module::visit_config(config_visitor& visitor)
{
    std::lock_guard lock(settings_mutex);
    if (vape_settings::visit(visitor, kind, settings)) reset_requested.store(true);
}
