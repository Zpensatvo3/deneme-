#include "auto_totem.hpp"
#include "../../services/game/world_bridge.hpp"
#include "../../ui/settings_renderer.hpp"

void modules::auto_totem::render_overlay()
{
    if (lifecycle_reset_.exchange(false, std::memory_order_acq_rel)) cancel_on_client_thread();
    const auto expected = generation_.load(std::memory_order_acquire);
    const auto config = settings_.read();
    services::game::world_bridge::tick_totem(config, enabled, generation_, expected);
}
void modules::auto_totem::cancel_on_client_thread()
{
    services::game::world_bridge::cancel_totem();
}
void modules::auto_totem::render_options()
{
    if (gui::settings::render(settings_, module_settings::auto_totem::schema()))
        generation_.fetch_add(1, std::memory_order_release);
}
