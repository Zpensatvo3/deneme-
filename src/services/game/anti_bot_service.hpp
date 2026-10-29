#pragma once
#include "../../sdk/minecraft/common/entities.hpp"
#include "../../modules/combat/settings/anti_bot_settings.hpp"

namespace services::anti_bot
{
    void configure(bool enabled, const module_settings::anti_bot& settings);
    void clear_session();
    // Read-only common filter. Unknown/unreliable data is never a bot verdict.
    bool is_bot(const maps::Entity& entity);
}
