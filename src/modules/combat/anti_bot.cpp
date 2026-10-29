#include "anti_bot.hpp"
#include "../../cache/cache.hpp"
#include "../../services/game/anti_bot_service.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../gui/gui.hpp"
#include "../ui_redirect.hpp"

modules::anti_bot::anti_bot() : module("anti bot", "Client", "anti_bot")
{
    enabled = module_settings::anti_bot_default_enabled;
    services::anti_bot::configure(enabled, settings_.read());
}
modules::anti_bot::~anti_bot() { services::anti_bot::configure(false, settings_.read()); }
void modules::anti_bot::run(::cache& cache)
{
    services::anti_bot::configure(enabled, settings_.read());
    if (!cache.is_valid() || !cache.world || !cache.player) services::anti_bot::clear_session();
}
void modules::anti_bot::on_disable(::cache&) { services::anti_bot::configure(false, settings_.read()); }
void modules::anti_bot::render_options()
{
    gui::settings::render(settings_, module_settings::anti_bot::schema());
    ImGui::TextWrapped("%s", "Shared player filter for aim, trigger and visuals. Unknown data is allowed; hidden Tab names alone are not bots.");
}
