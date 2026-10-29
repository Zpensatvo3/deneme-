#define ONESTEP_UI_NO_REDIRECT
#include "modules.hpp"
#undef ONESTEP_UI_NO_REDIRECT
#include "module_catalog.hpp"

namespace
{
    using module_factory = modules::module* (*)();

    template <typename Module>
    modules::module* create_module()
    {
        return new Module();
    }


    struct descriptor
    {
        const char* stable_id;
        module_factory create;
    };

    const descriptor default_modules[] = {
        { "friends_manager", &create_module<modules::friend_manager> },
        { "click_gui", &create_module<modules::click_gui> },
        { "panic", &create_module<modules::panic> },
        { "anti_bot", &create_module<modules::anti_bot> },
        { "legit_click_aim_assist", &create_module<modules::legit_click_aim_assist> },
        { "blatant_click_aim_assist", &create_module<modules::blatant_click_aim_assist> },
        { "aim_assist", &create_module<modules::aim_assist> },
        { "triggerbot", &create_module<modules::triggerbot> },
        { "watermark", &create_module<modules::watermark> },
        { "arraylist", &create_module<modules::arraylist> },
        { "chams", &create_module<modules::chams> },
        { "china_hat", &create_module<modules::china_hat> },
        { "wings", &create_module<modules::wings> },
        { "cape", &create_module<modules::cape> },
        { "sky", &create_module<modules::sky> },
        { "trails", &create_module<modules::trails> },
        { "hit_particles", &create_module<modules::hit_particles> },
        { "jump_circles", &create_module<modules::jump_circles> },
        { "player_esp", &create_module<modules::player_esp> },
        { "item_esp", &create_module<modules::item_esp> },
        { "storage_esp", &create_module<modules::storage_esp> },
        { "block_esp", &create_module<modules::block_esp> },
        { "radar", &create_module<modules::radar> },
        { "spotify_hud", &create_module<modules::spotify_hud> },
        { "full_bright", &create_module<modules::full_bright> },
        { "jump_reset", &create_module<modules::jump_reset> },
        { "shield_breaker", &create_module<modules::shield_breaker> },
        { "auto_totem", &create_module<modules::auto_totem> },
        { "pearl_macro", &create_module<modules::pearl_macro> },
        { "sprint", &create_module<modules::sprint> },
        { "no_jump_delay", &create_module<modules::no_jump_delay> },
        { "fast_place", &create_module<modules::fast_place> },
        { "fast_exp", &create_module<modules::fast_exp> },
        { "pot_refill", &create_module<modules::pot_refill> },
        { "mouse_tweaks", &create_module<modules::mouse_tweaks> },
        { "vape_aim_assist", &modules::create_vape_aim },
    };
}

void modules::register_default_modules()
{
    for (const descriptor& entry : default_modules)
    {
        module* instance = entry.create();
        if (!instance || std::string_view(instance->get_id()) != entry.stable_id)
        {
            delete instance;
            continue;
        }
        register_module(instance);
    }
}
