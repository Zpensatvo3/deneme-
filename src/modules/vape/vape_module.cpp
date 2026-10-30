#include "vape_module.hpp"
#include "../../services/game/anti_bot_service.hpp"
#include "game_bindings.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../../services/game/client_context.hpp"
#include "../../services/game/attack_coordinator.hpp"
#include "../../services/input/input_service.hpp"
#include "../core/module_registry.hpp"
#include "../../friends/friends.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

// Independent user-supplied engines, corrected against the recovered Java source.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "engines/aimassist/AimAssist.hpp"
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace vape_bridge
{
    using vector = Vape::Math::Vector3;
    using clock = std::chrono::steady_clock;
    struct runtime
    {
        maps::ClientWorld world{nullptr, true};
        maps::ClientPlayerEntity player{nullptr, true};
        Vape::Combat::AimAssist aim;
        int last_tick = -1, target_id = -1;
        clock::time_point aim_time{};
        clock::time_point target_time{};
    };

    bool finite(vector point)
    {
        return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
    }
    bool finite(const Vape::Math::BoundingBox& box)
    {
        return finite(box.min) && finite(box.max) &&
            box.min.x <= box.max.x && box.min.y <= box.max.y && box.min.z <= box.max.z;
    }
    template <typename Entity>
    vector position(const Entity& entity)
    {
        const auto p = entity.get_position(); return {p.x, p.y, p.z};
    }
    template <typename Entity>
    vector velocity(const Entity& entity)
    {
        auto value = entity.getVelocity.call();
        if (!value || platform::jni_context::clear_exception())
            return {(std::numeric_limits<double>::quiet_NaN)(),0,0};
        return {value.x.get(), value.y.get(), value.z.get()};
    }
    std::string item_key(const maps::ItemStack& stack)
    {
        if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception()) return {};
        auto item = stack.item.get();
        if (!item || platform::jni_context::clear_exception()) return {};
        auto text = item.getTranslationKey.call();
        if (!text || platform::jni_context::clear_exception()) return {};
        return text.to_string();
    }
    bool weapon(const std::string& key)
    {
        return key.ends_with("_sword") || key.ends_with("_axe") || key.ends_with(".mace");
    }
    int equipment_damage(const std::string& key)
    {
        if (key.ends_with(".mace")) return 6;
        if (key.ends_with("_sword")) {
            if (key.find("netherite") != std::string::npos) return 8;
            if (key.find("diamond") != std::string::npos) return 7;
            if (key.find("iron") != std::string::npos) return 6;
            if (key.find("stone") != std::string::npos) return 5;
            return 4;
        }
        if (key.ends_with("_axe")) {
            if (key.find("wooden") != std::string::npos || key.find("golden") != std::string::npos) return 7;
            return 9;
        }
        return 1;
    }
    bool usable(const maps::ItemStack& stack, bool blocks_only)
    {
        if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception()) return false;
        auto item = stack.item.get();
        if (!item || platform::jni_context::clear_exception()) return false;
        return !blocks_only || item.is_instance_of<maps::BlockItem>();
    }
    bool pressed(const maps::KeyBinding& key)
    {
        return key && key.isPressed.is_valid() && key.isPressed.call() == JNI_TRUE;
    }
    bool block_target(const maps::MinecraftClient& instance)
    {
        auto hit = instance.crosshairTarget.get();
        if (!hit || platform::jni_context::clear_exception()) return false;
        auto type = hit.getType.call();
        auto block = maps::HitResult$Type{}.BLOCK.get();
        return type && block && type.is_same_object(block);
    }
    bool sensitivity(const maps::GameOptions& options, float& output)
    {
        auto setting = options.getMouseSensitivity.call();
        if (!setting || platform::jni_context::clear_exception()) return false;
        auto object = setting.getValue.call();
        if (!object || platform::jni_context::clear_exception()) return false;
        maps::Double value(object.object_instance, false);
        if (!value.doubleValue.is_valid()) return false;
        const auto raw = value.doubleValue.call();
        if (platform::jni_context::clear_exception() || !std::isfinite(raw) || raw < 0 || raw > 1) return false;
        output = static_cast<float>(raw); return true;
    }
    bool target_allowed(const maps::Entity& entity, const maps::ClientPlayerEntity& player)
    {
        return entity && entity.is_instance_of<maps::PlayerEntity>() && !entity.is_same_object(player) &&
            entity.isAlive.call() == JNI_TRUE && entity.isInvisible.call() != JNI_TRUE &&
            !friends::is_friend(entity) && !services::anti_bot::is_bot(entity);
    }
    struct target_snapshot
    {
        Vape::Combat::AimTargetCandidate aim;
    };
    std::vector<target_snapshot> targets(const services::game::client_context& context)
    {
        std::vector<target_snapshot> result;
        auto players = platform::adapter::get().get_players(context.world);
        if (!players || platform::jni_context::clear_exception()) return result;
        auto array = players.toArray.call();
        if (!array || platform::jni_context::clear_exception()) return result;
        vape_bindings::Player local(context.player.object_instance, false);
        if (!local.canSee.is_valid()) return result;
        for (const auto& object : array.to_vector()) {
            jni::frame frame{48};
            maps::Entity entity(object.object_instance, false);
            if (!target_allowed(entity, context.player) || platform::jni_context::clear_exception()) continue;
            if (local.canSee.call(entity) != JNI_TRUE || platform::jni_context::clear_exception()) continue;
            auto bounds = entity.get_bounding_box();
            if (!bounds || !entity.getEyeY.is_valid() || platform::jni_context::clear_exception()) continue;
            target_snapshot sample;
            sample.aim.entityId = entity.getId.call();
            sample.aim.boundingBox = {{bounds.minX.get(),bounds.minY.get(),bounds.minZ.get()},
                {bounds.maxX.get(),bounds.maxY.get(),bounds.maxZ.get()}};
            sample.aim.velocity = velocity(entity);
            vape_bindings::Player target(entity.object_instance, false);
            if (!target.getArmor.is_valid() || !target.getHealth.is_valid()) continue;
            sample.aim.health = target.getHealth.call();
            sample.aim.armorValue = target.getArmor.call();
            sample.aim.equipmentTier = equipment_damage(item_key(target.getMainHandStack.call()));
            if (platform::jni_context::clear_exception() || !finite(sample.aim.boundingBox) ||
                !finite(sample.aim.velocity) ||
                !std::isfinite(sample.aim.health)) continue;
            result.push_back(sample);
        }
        return result;
    }

    // Common IO arbitration only; no Wolf target selection, timing or aim math.
    bool active(const char* id)
    {
        const auto* module = modules::find_module_by_id(id);
        return module && module->enabled;
    }
    const char* name(modules::vape_kind kind)
    {
        switch (kind) {
        case modules::vape_kind::aim: return "vape aim assist";
        }
        return "vape";
    }
    const char* id(modules::vape_kind kind)
    {
        switch (kind) {
        case modules::vape_kind::aim: return "vape_aim_assist";
        }
        return "vape";
    }
}

using namespace vape_bridge;

modules::vape_module::vape_module(vape_kind selected)
    : module(vape_bridge::name(selected), "Vape", vape_bridge::id(selected)),
      settings(vape_bridge::id(selected), vape_settings::schema(selected)), kind(selected)
{
}
modules::vape_module::~vape_module() = default;
void modules::vape_module::on_enable(::cache&) { reset_requested.store(true); }
void modules::vape_module::on_disable(::cache&) { reset_requested.store(true); }

const char* modules::vape_module::conflict() const
{
    using vape_bridge::active;
    if (kind == vape_kind::aim) {
        if (active("aim_assist") || active("click_aim_assist"))
            return "Pause: Wolf Aim Assist is enabled";
    }
    return nullptr;
}

void modules::vape_module::render_overlay()
{
    if (!platform::jni_context::bind_current_thread()) { runtime_issue="Vape: render-thread JNI unavailable."; reset_requested.store(true); return; }
    if (reset_requested.exchange(false) || !enabled) state.reset();
    const auto window = gui::get_window();
    if (!enabled || gui::draw || !gui::runtime_ready.load(std::memory_order_acquire) || gui::should_unload || !window ||
        GetForegroundWindow() != window || conflict()) { state.reset(); if(!enabled)runtime_issue.clear(); return; }
    runtime_issue.clear();
    struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } cleanup;
    try {
        jni::frame frame{256};
        services::game::client_context context;
        if (!services::game::capture_client_context(context)) { runtime_issue="Vape: client/player snapshot unavailable."; state.reset(); return; }
        if (context.screen ||
            context.player.isAlive.call() != JNI_TRUE || platform::jni_context::clear_exception()) { state.reset(); return; }
        auto camera = context.instance.getCameraEntity.call();
        if (!camera || !camera.is_same_object(context.player) || platform::jni_context::clear_exception()) { state.reset(); return; }
        if (state && (!state->world.is_same_object(context.world) || !state->player.is_same_object(context.player) ||
            platform::jni_context::clear_exception())) state.reset();
        const auto config = settings.read();
        if (!state) {
            state = std::make_unique<runtime>(); state->world = context.world; state->player = context.player;
        }
        auto& r = *state;
        const auto age = context.player.age.get();
        const bool tick = r.last_tick != age;
        r.last_tick = age;
        auto options = context.instance.options.get();
        auto main = context.player.getMainHandStack.call();
        const std::string key = item_key(main);
        const bool holding_weapon = weapon(key);
        const bool left = services::input::physical_button_down(services::input::mouse_button::left);
        const bool using_item = context.player.isUsingItem.call() == JNI_TRUE;
        const bool looking_at_block = block_target(context.instance);
        if (!options || platform::jni_context::clear_exception()) { state.reset(); return; }
        if (!context.player.yaw.is_valid() || !context.player.pitch.is_valid()) { state.reset(); return; }
        float yaw = context.player.yaw.get(), pitch = context.player.pitch.get();
        const float original_yaw = yaw, original_pitch = pitch;
        float mouse_sensitivity = 0;
        if (!std::isfinite(yaw) || !std::isfinite(pitch) || platform::jni_context::clear_exception()) { state.reset(); return; }
        if (!sensitivity(options, mouse_sensitivity)) {
            runtime_issue="Vape: mouse sensitivity binding unavailable."; state.reset(); return;
        }

        if (kind == vape_kind::aim) {
            if (!context.player.getEyeY.is_valid()) { state.reset(); return; }
            auto eye = position(context.player); eye.y = context.player.getEyeY.call();
            if (!finite(eye) || platform::jni_context::clear_exception()) { state.reset(); return; }
            if (kind == vape_kind::aim) {
                vape_bindings::Manager manager(context.interaction.object_instance, false);
                if (config.protect_blocks && !manager.breaking.is_valid()) { state.reset(); return; }
                const bool breaking_blocks = left && looking_at_block && manager.breaking.call() == JNI_TRUE;
                const bool activated = !using_item && (!config.require_mouse || left) &&
                    (!config.weapons_only || holding_weapon) && (!config.protect_blocks || !breaking_blocks);
                if (!activated) { r.aim.reset(); r.target_id = -1; r.aim_time={}; return; }
                r.aim.distance = config.range; r.aim.maxAngle = config.fov;
                r.aim.requireMouseDown = config.require_mouse; r.aim.weaponsOnly = config.weapons_only;
                r.aim.checkBlockBreak = config.protect_blocks;
                r.aim.targetSortMode = static_cast<Vape::Combat::AimAssist::TargetSortMode>(config.sort);
                r.aim.mode = static_cast<Vape::Combat::AimAssist::EngineMode>(config.aim_mode);
                r.aim.getAdaptiveTargeting().targetArea = static_cast<Vape::Combat::AimAssistTargeting::TargetArea>(config.target_area);
                auto& simple = r.aim.getSimpleRotation();
                simple.horizontalSpeed = config.horizontal; simple.verticalSpeed = config.vertical;
                simple.aimVertically = config.aim_vertical; simple.strafeIncrease = config.strafe_increase;
                {
                    std::vector<Vape::Combat::AimTargetCandidate> candidates;
                    for (const auto& sample : targets(context)) candidates.push_back(sample.aim);
                    const Vape::Combat::AimTargetCandidate* target = nullptr;
                    const auto now=clock::now();
                    // A held activation retains a valid target instead of
                    // resetting all velocity buffers whenever two players cross.
                    if (config.require_mouse || now-r.target_time<std::chrono::milliseconds(700)) for (const auto& candidate : candidates)
                        if (candidate.entityId==r.target_id && r.aim.isValidTarget(eye,yaw,candidate)) { target=&candidate; break; }
                    if (!target) {target=r.aim.findBestTarget(eye,yaw,candidates);r.target_time=now;}
                    const int next_id = target ? target->entityId : -1;
                    if (r.target_id != next_id) { r.aim.reset(); r.aim_time={}; }
                    r.target_id = next_id;
                    const float strafe = (pressed(options.rightKey.get()) ? 1.f : 0.f) -
                        (pressed(options.leftKey.get()) ? 1.f : 0.f);
                    if (platform::jni_context::clear_exception()) { state.reset(); return; }
                    const auto motion = velocity(context.player);
                    if (!finite(motion)) { state.reset(); return; }
                    const double elapsed=r.aim_time.time_since_epoch().count()?
                        std::chrono::duration<double>(now-r.aim_time).count():.001;
                    r.aim_time=now;
                    r.aim.updateFrame(eye, yaw, pitch, strafe, left, holding_weapon, breaking_blocks, target,
                        motion,context.player.isOnGround.call()==JNI_TRUE,elapsed);
                }
                r.aim.applyRenderTick(yaw, pitch, mouse_sensitivity);
            }
        }
        if (platform::jni_context::clear_exception() || !enabled || reset_requested.load() ||
            !std::isfinite(yaw) || !std::isfinite(pitch) || pitch < -90 || pitch > 90) { state.reset(); return; }
        if (yaw != original_yaw || pitch != original_pitch) {
            if(kind==vape_kind::aim) {
                const float delta=Vape::Math::RotationUtil::wrapDegrees(yaw-original_yaw);
                // Java's mouse applier adjusts interpolation history too. Keep
                // yaw continuous across +/-180 instead of rendering a 360 turn.
                yaw=original_yaw+delta;
                const bool history=context.player.prevYaw.is_valid()&&context.player.prevPitch.is_valid();
                if(history) {
                    const float previousYaw=context.player.prevYaw.get(),previousPitch=context.player.prevPitch.get();
                    if(platform::jni_context::clear_exception()) {state.reset();return;}
                    context.player.prevYaw.set(previousYaw+delta);
                    context.player.prevPitch.set(previousPitch+pitch-original_pitch);
                }
            }
            context.player.yaw.set(yaw); context.player.pitch.set(pitch);
            if (platform::jni_context::clear_exception()) state.reset();
        }
    } catch (...) { platform::jni_context::clear_exception(); state.reset(); }
}

modules::module* modules::create_vape_aim() { return new vape_module(vape_kind::aim); }
