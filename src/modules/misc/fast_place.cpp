#include "fast_place.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/utility_bridge.hpp"
#include "../../services/input/input_service.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
#include <algorithm>

void modules::fast_place::run(::cache&) {}

void modules::fast_place::render_overlay()
{
    if (!enabled || !platform::jni_context::bind_current_thread()) return;
    const auto settings = settings_.read();
    services::game::utility::update(services::game::utility::feature::fast_place,
        enabled, std::clamp(settings.delay, 0, 4),
        services::input::physical_button_down(services::input::mouse_button::right));
}

void modules::fast_place::render_options()
{
    gui::settings::render(settings_, module_settings::fast_place::schema());
}
