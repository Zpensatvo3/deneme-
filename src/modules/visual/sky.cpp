#include "sky.hpp"
#include "sky_bindings.hpp"
#include "sky_panorama.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/ui_state.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/overlay_context.hpp"
#include "../../ui/settings_renderer.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    int blend_rgb(int original,ImVec4 target,float strength)
    {
        const auto channel=[&](int shift,float value) {
            value=std::isfinite(value) ? std::clamp(value,0.f,1.f) : 0.f;
            return std::clamp(static_cast<int>(std::lround(((original>>shift)&255)*(1-strength)+value*255*strength)),0,255);
        };
        return channel(16,target.x)<<16 | channel(8,target.y)<<8 | channel(0,target.z);
    }
}

struct modules::sky::runtime
{
    services::game::overlay_session session;
    sky_panorama::renderer panorama;
    unsigned generation=0;
    double next_update=0,next_capture=0;

    bool reset()
    {
        bool complete=true;
        if (complete) { session.reset(); next_update=next_capture=0; }
        return complete;
    }
};

modules::sky::sky() : module("sky","Visual","sky"),state_(std::make_unique<runtime>()) {}
modules::sky::~sky() = default;

void modules::sky::on_render_shutdown()
{
    state_->panorama.shutdown();
    if (!platform::jni_context::bind_current_thread()) return;
    try { jni::frame frame{32}; state_->reset(); }
    catch (...) { platform::jni_context::clear_exception(); }
}

void modules::sky::render_overlay()
{
    if (!platform::jni_context::bind_current_thread()) return;
    struct cleanup { ~cleanup() { platform::jni_context::clear_exception(); } } clear_on_exit;
    jni::frame frame{64};
    auto& state=*state_;
    const auto config=settings_.read();
    const bool reduced=gui::ui_state::get_preferences().reduce_motion;
    bool panorama_ready=false,colors_ready=false;
    int sky_rgb=0,fog_rgb=0;
    struct publish_panorama
    {
        sky_panorama::renderer& renderer;
        const module_settings::sky& config;
        const bool& ready;
        const bool& colors_ready;
        const int& sky_rgb;
        const int& fog_rgb;
        bool reduced;
        ~publish_panorama()
        {
            renderer.configure(config,ready,reduced);
            renderer.configure_colors(sky_rgb,fog_rgb,std::clamp(config.strength,0.f,1.f),config.match_fog,colors_ready);
        }
    } publish{state.panorama,config,panorama_ready,colors_ready,sky_rgb,fog_rgb,reduced};
    try
    {
        const auto generation=reset_generation_.load(std::memory_order_acquire);
        if (generation!=state.generation)
        {
            if (!state.reset()) return;
            state.generation=generation;
        }
        if (!enabled || !gui::runtime_ready.load(std::memory_order_acquire) || gui::should_unload) { state.reset(); return; }
        services::game::client_context context;
        if (!services::game::capture_client_context(context,true,false)) { state.reset(); return; }
        // The same biome registry is reused across dimensions and camera media.
        // Restore our air/sky colors before vanilla draws water, lava or a skyless world.
        sky_bindings::World world(context.world.object_instance,false);
        auto dimension=world.getDimension.call();
        if (platform::jni_context::clear_exception() || !dimension) { state.reset(); return; }
        const bool skylight=dimension.hasSkyLight.call()==JNI_TRUE;
        if (platform::jni_context::clear_exception() || !skylight) { state.reset(); return; }
        auto renderer=context.instance.gameRenderer.get();
        if (platform::jni_context::clear_exception() || !renderer) { state.reset(); return; }
        auto camera=renderer.getCamera.call();
        if (platform::jni_context::clear_exception() || !camera) { state.reset(); return; }
        sky_bindings::Camera view(camera.object_instance,false);
        auto medium=view.getSubmersionType.call();
        if (platform::jni_context::clear_exception() || !medium) { state.reset(); return; }
        auto air=sky_bindings::Submersion{}.NONE.get();
        if (platform::jni_context::clear_exception() || !air) { state.reset(); return; }
        const bool clear_air=air.is_same_object(medium);
        if (platform::jni_context::clear_exception() || !clear_air) { state.reset(); return; }
        const services::game::overlay_identity identity(context.world,context.player);
        if (!state.session.matches(identity))
        {
            if (!state.reset()) return;
            state.session.observe(identity);
        }
        if (!std::isfinite(config.strength) || config.strength<=0) { state.reset(); return; }
        colors_ready=state.panorama.connect();
        panorama_ready=config.mode==1 && colors_ready;
        ImVec4 color;
        if (config.mode==1)
        {
            switch (config.panorama)
            {
            case 1: color={.14f,.13f,.20f,1.f}; break;
            case 2: color={.74f,.56f,.43f,1.f}; break;
            case 3: color={.82f,.43f,.65f,1.f}; break;
            case 4: color={.31f,.20f,.55f,1.f}; break;
            case 5: color={.32f,.40f,.66f,1.f}; break;
            default: color={.08f,.12f,.22f,1.f}; break;
            }
        }
        else switch (config.style)
        {
        case 1: color={1.f,.43f,.32f,1.f}; break;
        case 2: color={.46f,.24f,.78f,1.f}; break;
        case 3: color={.29f,.76f,1.f,1.f}; break;
        case 4: color={config.color[0],config.color[1],config.color[2],1.f}; break;
        default: color=gui::get_dynamic_color().Value; break;
        }
        auto fog=color;
        if (config.mode!=1 || config.panorama<3)
        {
            fog.x=fog.x*.67f+.28f; fog.y=fog.y*.67f+.28f; fog.z=fog.z*.67f+.28f;
        }
        const float strength=std::clamp(config.strength,0.f,1.f);
        sky_rgb=blend_rgb(0,color,1.f);
        fog_rgb=blend_rgb(0,fog,1.f);
    }
    catch (...) { platform::jni_context::clear_exception(); panorama_ready=colors_ready=false; state.reset(); }
}

void modules::sky::render_options()
{
    gui::settings::render(settings_,module_settings::sky::schema());
    if (settings_.read().mode==1)
    {
        const char* error=state_->panorama.error();
        if (error && *error) ImGui::TextWrapped("%s",error);
    }
}
