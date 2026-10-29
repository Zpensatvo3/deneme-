#include "../../platform/jni/jni_context.hpp"

#include "sprint.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
#include "../../services/game/utility_bridge.hpp"

void modules::sprint::run(::cache&) {}

void modules::sprint::on_disable(::cache&)
{
    reset_requested_.store(true, std::memory_order_release);
}

void modules::sprint::render_overlay()
{
    if (!platform::jni_context::bind_current_thread()) return;
    if (reset_requested_.exchange(false, std::memory_order_acq_rel) &&
        !services::game::utility::release(services::game::utility::feature::sprint))
    {
        reset_requested_.store(true, std::memory_order_release);
        return;
    }
    const auto settings = settings_.read();
    services::game::utility::update(services::game::utility::feature::sprint,
        enabled, settings.stop_while_using_item ? 1 : 0);
}

void modules::sprint::on_render_shutdown()
{
    services::game::utility::release(services::game::utility::feature::sprint);
}

void modules::sprint::render_options()
{
	gui::settings::render(settings_, module_settings::sprint::schema());
}
