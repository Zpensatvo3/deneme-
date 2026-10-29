#include "trigger_config.hpp"
#include "../../ui/settings_renderer.hpp"
#include "trigger_adapter.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"
#include <imgui/imgui.h>

void trigger_engine::render_options(config& settings, adapter_runtime& state, const char* id_suffix)
{
    ImGui::PushID(id_suffix);
    config candidate = settings;
    gui::settings::render(candidate, schema());
    if (normalize(candidate) && !same_config(candidate, settings)) {
        settings = candidate; ++state.settings_revision; request_reset(state);
    }
    ImGui::PopID();
}
