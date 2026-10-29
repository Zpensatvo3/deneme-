#include <imgui/imgui.h>
#include "../../cache/cache.hpp"

#include "trigger_engine.hpp"
#include "triggerbot.hpp"
#include "../../services/game/world_bridge.hpp"
#include "../ui_redirect.hpp"
void modules::triggerbot::run(::cache&)
{
	// Trigger execution stays on Minecraft's render/client thread.
}

void modules::triggerbot::render_overlay()
{
	trigger_engine::run_overlay(settings, state, enabled);
}

void modules::triggerbot::render_options()
{
	trigger_engine::render_options(settings, state, "triggerbot_settings");
}

void modules::triggerbot::on_disable(::cache&)
{
	trigger_engine::request_reset(state);
}

void modules::triggerbot::on_enable(::cache&)
{
	trigger_engine::request_reset(state);
}

void modules::triggerbot::cancel_on_client_thread()
{
    services::game::world_bridge::cancel_trigger();
}
