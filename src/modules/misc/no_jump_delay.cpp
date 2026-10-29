#include "no_jump_delay.hpp"
#include "../../services/game/hybrid_bridge.hpp"

void modules::no_jump_delay::render_overlay()
{
    if (enabled) services::game::hybrid::tick_no_jump_delay();
}
