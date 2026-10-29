#pragma once
#include "settings/china_hat_settings.hpp"
#include "settings/wings_settings.hpp"
#include "settings/cape_settings.hpp"
#include "settings/trails_settings.hpp"
#include "settings/hit_particles_settings.hpp"
#include "settings/jump_circles_settings.hpp"
#include <memory>

namespace cosmetics
{
    // Runtime/Java identity and native physics are render-thread owned. Worker
    // module callbacks only request a reset through the module's generation.
    class cape_motion
    {
    public:
        cape_motion();
        ~cape_motion();
        void clear();
    private:
        struct runtime;
        std::unique_ptr<runtime> state_;
        friend void draw_cape(const module_settings::cape&,cape_motion&);
        friend void draw_wings(const module_settings::wings&,cape_motion&);
    };
    void draw_hat(const module_settings::china_hat& config);
    void draw_wings(const module_settings::wings& config,cape_motion& motion);
    void draw_cape(const module_settings::cape& config, cape_motion& motion);
    class trail_state
    {
    public:
        trail_state();
        ~trail_state();
        void clear();
    private:
        struct runtime;
        std::unique_ptr<runtime> state_;
        friend void draw_trails(const module_settings::trails&,trail_state&);
    };
    void draw_trails(const module_settings::trails& config,trail_state& state);
    class visual_effect_state
    {
    public:
        visual_effect_state();
        ~visual_effect_state();
        void clear();
    private:
        struct runtime;
        std::unique_ptr<runtime> state_;
        friend void draw_hit_particles(const module_settings::hit_particles&, visual_effect_state&);
        friend void draw_jump_circles(const module_settings::jump_circles&, visual_effect_state&);
    };
    void draw_hit_particles(const module_settings::hit_particles&, visual_effect_state&);
    void draw_jump_circles(const module_settings::jump_circles&, visual_effect_state&);
}
