#include "../../ui/settings_renderer.hpp"
#include "../../services/game/anti_bot_service.hpp"
#include "legit_aim_engine.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../../services/game/client_context.hpp"
#include "../../services/input/input_service.hpp"
#include "../../friends/friends.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <imgui/imgui.h>

namespace
{
    using steady_clock = std::chrono::steady_clock;
    namespace geometry = legit_aim_math;

    bool has_combat_weapon(const maps::ClientPlayerEntity& player)
    {
        auto stack = player.getMainHandStack.call();
        if (platform::jni_context::clear_exception() || !stack) return false;
        const bool empty = stack.isEmpty.call() == JNI_TRUE;
        if (platform::jni_context::clear_exception() || empty) return false;
        auto item = stack.item.get();
        if (platform::jni_context::clear_exception() || !item) return false;
        const bool sword = item.is_sword();
        const bool axe = item.is_instance_of<maps::AxeItem>();
        const bool mace = item.is_instance_of<maps::MaceItem>();
        if (platform::jni_context::clear_exception()) return false;
        if (sword || axe || mace) return true;
        auto translation = item.getTranslationKey.call();
        if (platform::jni_context::clear_exception() || !translation) return false;
        const auto key = translation.to_string();
        if (platform::jni_context::clear_exception()) return false;
        return key.ends_with("_sword") || key.ends_with(".sword") ||
            key.ends_with("_axe") || key.ends_with(".axe") ||
            key.ends_with("_mace") || key.ends_with(".mace");
    }

    bool read_bounds(const maps::Entity& entity, geometry::bounds& result)
    {
        auto box = entity.get_bounding_box();
        if (platform::jni_context::clear_exception() || !box) return false;
        // Take one numeric snapshot; do not repeatedly query six JNI fields.
        result = {box.minX.get(), box.minY.get(), box.minZ.get(),
            box.maxX.get(), box.maxY.get(), box.maxZ.get()};
        return !platform::jni_context::clear_exception() && geometry::valid(result);
    }

    geometry::point target_point(geometry::bounds box, legit_aim_engine::aim_point point, double distance)
    {
        const double height = box.max_y - box.min_y;
        const double ratio = point == legit_aim_engine::aim_point::head ? .86 :
            point == legit_aim_engine::aim_point::upper_body ? .68 : std::clamp(.58 + distance * .035, .60, .75);
        return {(box.min_x + box.max_x) * .5, box.min_y + height * ratio, (box.min_z + box.max_z) * .5};
    }

    bool has_line_of_sight(const maps::ClientWorld& world, const maps::ClientPlayerEntity& player,
        geometry::point from, geometry::point to)
    {
        if (!geometry::finite(from) || !geometry::finite(to)) return false;
        auto start = maps::Vec3d::new_object(&maps::Vec3d::constructor, from.x, from.y, from.z);
        auto end = maps::Vec3d::new_object(&maps::Vec3d::constructor, to.x, to.y, to.z);
        auto shape = maps::RaycastContext$ShapeType{}.COLLIDER.get();
        auto fluid = maps::RaycastContext$FluidHandling{}.NONE.get();
        if (platform::jni_context::clear_exception() || !start || !end || !shape || !fluid) return false;
        auto context = maps::RaycastContext::new_object(&maps::RaycastContext::constructor,
            start, end, shape, fluid, maps::Entity(player.object_instance, false));
        if (platform::jni_context::clear_exception() || !context) return false;
        auto hit = world.raycast.call(context);
        if (platform::jni_context::clear_exception() || !hit) return false;
        auto type = maps::HitResult(hit.object_instance, false).getType.call();
        auto block = maps::HitResult$Type{}.BLOCK.get();
        if (platform::jni_context::clear_exception() || !type || !block) return false;
        const bool blocked = type.is_same_object(block);
        return !platform::jni_context::clear_exception() && !blocked;
    }

    struct candidate
    {
        maps::Entity entity{};
        geometry::bounds box{};
        geometry::point point{};
        maths::angles delta{};
        double distance = 0, score = (std::numeric_limits<double>::max)();
        bool valid = false;
    };

    candidate evaluate(const maps::Entity& entity, const legit_aim_engine::config& settings,
        geometry::point eye, const maths::angles& view, const maps::ClientWorld& world,
        const maps::ClientPlayerEntity& player, float fov_multiplier)
    {
        // A rejected candidate must not leave a Java exception for the next one.
        struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } clear_exception;
        candidate result;
        if (!entity) return result;
        const bool is_player = entity.is_instance_of<maps::PlayerEntity>();
        const bool self = entity.is_same_object(player);
        const bool alive = entity.isAlive.call() == JNI_TRUE;
        const bool invisible = entity.isInvisible.call() == JNI_TRUE;
        if (platform::jni_context::clear_exception() || !is_player || self || !alive || invisible ||
            friends::is_friend(entity) || services::anti_bot::is_bot(entity) || platform::jni_context::clear_exception()) return result;
        geometry::bounds box;
        if (!read_bounds(entity, box)) return result;
        const double distance = geometry::distance_to_box(eye, box);
        if (!std::isfinite(distance) || distance > settings.max_distance) return result;
        const auto point = target_point(box, settings.point, distance);
        geometry::angles desired;
        if (!geometry::angles_to(eye, point, desired)) return result;
        const maths::angles delta{geometry::wrap(desired.yaw - view.yaw), desired.pitch - view.pitch};
        const double angular_distance = std::hypot(delta.yaw, delta.pitch);
        if (!std::isfinite(angular_distance) || angular_distance > settings.fov * fov_multiplier ||
            !has_line_of_sight(world, player, eye, point)) return result;
        result.entity = entity; result.box = box; result.point = point; result.delta = delta;
        result.distance = distance; result.score = angular_distance + distance * .08; result.valid = true;
        return result;
    }

    void clear_challenger(legit_aim_engine::runtime& state)
    {
        state.challenger.clear_ref(); state.challenger_since = {};
    }
    void assign_target(legit_aim_engine::runtime& state, const maps::Entity& target)
    {
        state.locked_target = target; state.filtered_point_valid = false;
        geometry::reset_motion(state.motion);
        state.assist_blend = std::min(state.assist_blend, .35f); clear_challenger(state);
    }
}

void legit_aim_engine::request_reset(runtime& state)
{
    // Lifecycle hooks can run on the worker. No JNI/runtime mutation there.
    state.reset_generation.fetch_add(1, std::memory_order_release);
}

void legit_aim_engine::reset(runtime& state)
{
    state.locked_target.clear_ref(); clear_challenger(state);
    state.world.clear_ref(); state.player.clear_ref(); state.config_valid = false;
    state.last_update = {}; state.last_click = {}; state.next_scan = {};
    state.manual_override_until = {}; state.filtered_point_valid = false;
    state.expected_view_valid = false; state.assist_blend = 0; state.active = false;
    geometry::reset_motion(state.motion);
}

void legit_aim_engine::run(const config& requested, runtime& state, bool enabled, bool require_left_click)
{
    if (!platform::jni_context::bind_current_thread()) { request_reset(state); return; }
    const auto generation = state.reset_generation.load(std::memory_order_acquire);
    if (state.seen_reset != generation || !enabled) {
        reset(state); state.seen_reset = generation;
        if (!enabled) return;
    }
    config settings = requested;
    const HWND window = gui::get_window();
    if (!normalize(settings) || settings.strength <= 0 || gui::draw || !gui::runtime_ready.load(std::memory_order_acquire) ||
        !window || GetForegroundWindow() != window) { reset(state); return; }

    struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } clear_exception;
    try {
        jni::frame frame{512};
        services::game::client_context context;
        if (!services::game::capture_client_context(context, true, false) || context.screen) { reset(state); return; }
        const bool alive = context.player.isAlive.call() == JNI_TRUE;
        const bool using_item = context.player.isUsingItem.call() == JNI_TRUE;
        auto camera = context.instance.getCameraEntity.call();
        if (platform::jni_context::clear_exception() || !alive || using_item || !camera ||
            !camera.is_same_object(context.player) || platform::jni_context::clear_exception() ||
            (settings.weapon_only && !has_combat_weapon(context.player))) { reset(state); return; }
        const bool same_session = state.world && state.player && state.world.is_same_object(context.world) &&
            state.player.is_same_object(context.player);
        if (platform::jni_context::clear_exception()) { reset(state); return; }
        if (!same_session || !state.config_valid || !same_config(settings, state.last_config)) {
            reset(state); state.world = context.world; state.player = context.player;
            state.last_config = settings; state.config_valid = true;
        }

        const auto now = steady_clock::now();
        const bool activated = !require_left_click ||
            services::input::physical_button_down(services::input::mouse_button::left);
        if (!activated) {
            if (state.last_click.time_since_epoch().count() == 0 ||
                now - state.last_click > std::chrono::milliseconds(200)) { reset(state); return; }
            // Keep the target briefly for reacquisition, but never move the
            // camera after the user releases physical left click.
            const float elapsed = state.last_update.time_since_epoch().count() == 0 ? 0.f :
                std::chrono::duration<float>(now - state.last_update).count();
            state.active = false; state.expected_view_valid = false;
            geometry::reset_motion(state.motion);
            state.assist_blend = std::max(0.f, state.assist_blend - std::clamp(elapsed, 0.f, .05f) / .08f);
            state.last_update = now; return;
        }
        state.last_click = now;
        float delta_time = state.last_update.time_since_epoch().count() == 0 ? 0.f :
            std::chrono::duration<float>(now - state.last_update).count();
        state.last_update = now;
        // A stalled frame is not accumulated camera movement to catch up with.
        if (!std::isfinite(delta_time) || delta_time < 0 || delta_time > .10f) { reset(state); return; }
        delta_time = std::clamp(delta_time, 0.f, .05f);

        // Missing JNI methods return numeric zero; never treat that fallback
        // as a real world-space eye position or camera angle.
        if (!context.player.getEyeY.is_valid() || !context.player.yaw.is_valid() || !context.player.pitch.is_valid()) {
            reset(state); return;
        }
        const auto position = context.player.get_position();
        const double eye_y = context.player.getEyeY.call();
        const geometry::point eye{position.x, eye_y, position.z};
        const auto view = context.player.get_angles();
        if (platform::jni_context::clear_exception() || !geometry::finite(eye) ||
            !geometry::finite(geometry::angles{view.yaw, view.pitch})) { reset(state); return; }
        if (now < state.manual_override_until) {
            state.active = false; state.expected_view = view; state.expected_view_valid = true;
            geometry::reset_motion(state.motion);
            state.assist_blend = std::max(0.f, state.assist_blend - delta_time / .08f); return;
        }

        candidate current;
        if (state.locked_target) {
            // Visibility is rechecked every camera update, not only each scan.
            current = evaluate(state.locked_target, settings, eye, view, context.world, context.player, 1.22f);
            if (!current.valid) {
                state.locked_target.clear_ref(); state.filtered_point_valid = false;
                state.expected_view_valid = false; state.assist_blend = 0;
                geometry::reset_motion(state.motion);
                state.next_scan = now; clear_challenger(state);
            }
        }
        if (now >= state.next_scan) {
            state.next_scan = now + std::chrono::milliseconds(40);
            candidate best;
            auto players = platform::adapter::get().get_players(context.world);
            if (platform::jni_context::clear_exception() || !players) { reset(state); return; }
            auto player_array = players.toArray.call();
            if (platform::jni_context::clear_exception() || !player_array) { reset(state); return; }
            for (const auto& object : player_array.to_vector()) {
                // Release temporary ray/box references per player, rather than
                // accumulating them across a large lobby in the outer frame.
                jni::frame candidate_frame{48};
                auto entity = maps::Entity(object.object_instance, false);
                auto tested = evaluate(entity, settings, eye, view, context.world, context.player, 1.f);
                if (tested.valid && tested.score < best.score) best = tested;
            }
            if (platform::jni_context::clear_exception()) { reset(state); return; }
            if (!current.valid) {
                clear_challenger(state);
                if (best.valid) { assign_target(state, best.entity); current = best; }
            } else {
                const bool better = best.valid && !best.entity.is_same_object(current.entity) && best.score < current.score - 2.5;
                if (platform::jni_context::clear_exception()) { reset(state); return; }
                if (better) {
                    const bool same_challenger = state.challenger && state.challenger.is_same_object(best.entity);
                    if (platform::jni_context::clear_exception()) { reset(state); return; }
                    if (!same_challenger) { state.challenger = best.entity; state.challenger_since = now; }
                    else if (now - state.challenger_since >= std::chrono::milliseconds(80)) {
                        assign_target(state, best.entity); current = best;
                    }
                } else clear_challenger(state);
            }
        }
        if (!current.valid) {
            state.active = false; state.expected_view_valid = false;
            geometry::reset_motion(state.motion);
            state.assist_blend = std::max(0.f, state.assist_blend - delta_time / .08f); return;
        }
        if (!state.filtered_point_valid) {
            state.filtered_point = current.point; state.filtered_point_valid = true;
        } else state.filtered_point = geometry::filtered(state.filtered_point, current.point, current.box, delta_time);

        geometry::angles observed_target;
        if (!geometry::angles_to(eye, current.point, observed_target)) { reset(state); return; }
        // Absolute target bearing, not target_delta: our own camera movement
        // and manual mouse rotation must not masquerade as target velocity.
        geometry::observe_target(state.motion, observed_target, delta_time);

        if (geometry::ray_intersects(eye, geometry::direction({view.yaw, view.pitch}), current.box, settings.max_distance)) {
            geometry::stop_motion(state.motion);
            state.active = true; state.expected_view = view; state.expected_view_valid = true; return;
        }
        // The smoothed aim point can be occluded even if the current centre is
        // visible. Do not rotate toward that stale/blocked filtered point.
        if (!has_line_of_sight(context.world, context.player, eye, state.filtered_point)) {
            state.active = false; state.expected_view_valid = false; state.filtered_point_valid = false;
            geometry::reset_motion(state.motion);
            state.assist_blend = 0; return;
        }
        geometry::angles desired;
        if (!geometry::angles_to(eye, state.filtered_point, desired)) { reset(state); return; }
        const geometry::angles target_delta{geometry::wrap(desired.yaw - view.yaw), desired.pitch - view.pitch};
        float manual_scale = 1;
        if (state.expected_view_valid) {
            const auto control = geometry::manual_input({view.yaw, view.pitch},
                {state.expected_view.yaw, state.expected_view.pitch}, target_delta, delta_time);
            manual_scale = control.influence;
            if (control.release_target) {
                state.locked_target.clear_ref(); clear_challenger(state); state.filtered_point_valid = false;
                state.next_scan = now; state.manual_override_until = now + std::chrono::milliseconds(180);
                geometry::reset_motion(state.motion);
                state.expected_view_valid = false; state.assist_blend = 0; state.active = false; return;
            }
        }
        state.assist_blend = std::min(1.f, state.assist_blend + delta_time / .09f);
        const auto step = geometry::motion(state.motion, target_delta, settings.strength, delta_time, state.assist_blend, manual_scale);
        const maths::angles output{view.yaw + step.yaw, std::clamp(view.pitch + step.pitch, -90.f, 90.f)};
        if (!geometry::finite(geometry::angles{output.yaw, output.pitch}) ||
            state.reset_generation.load(std::memory_order_acquire) != generation) { reset(state); return; }
        if (std::abs(step.yaw) > .00001f || std::abs(step.pitch) > .00001f) {
            context.player.yaw.set(output.yaw); context.player.pitch.set(output.pitch);
            if (platform::jni_context::clear_exception()) { reset(state); return; }
        }
        state.expected_view = output; state.expected_view_valid = true; state.active = true;
    } catch (...) {
        platform::jni_context::clear_exception(); reset(state);
    }
}

void legit_aim_engine::render_options(config& settings, const char* id_suffix)
{
    ImGui::PushID(id_suffix);
    gui::settings::render(settings, schema());
    ImGui::PopID();
}
