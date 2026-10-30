#include "../../ui/settings_renderer.hpp"
#include "../../ui/aim_region_picker.hpp"
#include "../../services/game/anti_bot_service.hpp"
#include "../../services/game/target_service.hpp"
#include "click_aim_engine.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <string>

#include <imgui/imgui.h>

#include "../../friends/friends.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"

namespace
{
	using steady_clock = std::chrono::steady_clock;
	constexpr auto lost_target_grace = std::chrono::milliseconds(250);
	constexpr auto click_release_grace = std::chrono::milliseconds(200);
	void deactivate(click_aim_engine::runtime& state)
	{
		state.active = false;
		state.last_update = {};
	}

	void clear_aim(click_aim_engine::runtime& state)
	{
		state.aim_target.clear_ref();
		state.selected_region = -1;
		state.aim_complete = false;
	}

	void release_target(click_aim_engine::runtime& state)
	{
		state.locked_target.clear_ref();
		state.lock_lost_since = {};
	}

	void lock_target(click_aim_engine::runtime& state, const maps::Entity& entity)
	{
		if (!state.locked_target || !state.locked_target.is_same_object(entity))
		{
			release_target(state);
			state.locked_target = entity;
		}
		state.lock_lost_since = {};
	}

	double distance_to_box(const maths::vector3d& eye, const maps::Box& box)
	{
		const double closest_x = (std::max)(box.minX.get(), (std::min)(eye.x, box.maxX.get()));
		const double closest_y = (std::max)(box.minY.get(), (std::min)(eye.y, box.maxY.get()));
		const double closest_z = (std::max)(box.minZ.get(), (std::min)(eye.z, box.maxZ.get()));
		const double dx = closest_x - eye.x;
		const double dy = closest_y - eye.y;
		const double dz = closest_z - eye.z;
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	template <typename EntityType>
	maths::vector3d eye_position(EntityType& entity)
	{
		const maths::vector3d position = entity.get_position();
		maps::EntityPose pose = entity.getPose.call();
		const float eye_height = pose && !platform::jni_context::clear_exception()
			? entity.getEyeHeight.call(pose)
			: 1.62f;
		platform::jni_context::clear_exception();
		return { position.x, position.y + eye_height, position.z };
	}

	bool has_combat_weapon(const maps::ClientPlayerEntity& player)
	{
		maps::ItemStack stack = player.getMainHandStack.call();
		if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception())
			return false;

		maps::Item item = stack.item.get();
		if (!item || platform::jni_context::clear_exception())
			return false;

		if (item.is_sword() ||
			item.is_instance_of<maps::AxeItem>() ||
			item.is_instance_of<maps::MaceItem>())
			return !platform::jni_context::clear_exception();

		maps::String translation = item.getTranslationKey.call();
		const std::string key = translation && !platform::jni_context::clear_exception()
			? translation.to_string()
			: std::string{};
		platform::jni_context::clear_exception();
		return key.ends_with("_sword") || key.ends_with(".sword") ||
			key.ends_with("_axe") || key.ends_with(".axe") ||
			key.ends_with("_mace") || key.ends_with(".mace");
	}

	bool has_line_of_sight(
		const maps::ClientWorld& world,
		const maps::ClientPlayerEntity& player,
		const maths::vector3d& from,
		const maths::vector3d& to)
	{
		maps::Vec3d start = maps::Vec3d::new_object(
			&maps::Vec3d::constructor, from.x, from.y, from.z);
		maps::Vec3d end = maps::Vec3d::new_object(
			&maps::Vec3d::constructor, to.x, to.y, to.z);
		maps::RaycastContext$ShapeType shape = maps::RaycastContext$ShapeType{}.COLLIDER.get();
		maps::RaycastContext$FluidHandling fluid = maps::RaycastContext$FluidHandling{}.NONE.get();
		if (!start || !end || !shape || !fluid || platform::jni_context::clear_exception())
			return false;

		maps::RaycastContext context = maps::RaycastContext::new_object(
			&maps::RaycastContext::constructor,
			start, end, shape, fluid, maps::Entity(player.object_instance, false));
		maps::BlockHitResult hit = context ? world.raycast.call(context) : maps::BlockHitResult{};
		if (!hit || platform::jni_context::clear_exception())
			return false;

		maps::HitResult$Type type = maps::HitResult(hit.object_instance, false).getType.call();
		maps::HitResult$Type block = maps::HitResult$Type{}.BLOCK.get();
		if (!type || !block || platform::jni_context::clear_exception())
			return false;
		return !type.is_same_object(block);
	}

	struct targeting_view
	{
		maths::vector3d forward{}, right{}, up{};
		double tan_half_vertical = 0.0, tan_half_horizontal = 0.0;

		bool initialize(HWND window, float vertical_fov, const maths::angles& view)
		{
			RECT client{};
			if (!std::isfinite(vertical_fov) || vertical_fov <= 0.f || vertical_fov >= 180.f ||
				!std::isfinite(view.yaw) || !std::isfinite(view.pitch) || !GetClientRect(window, &client))
				return false;
			const double width = static_cast<double>(client.right) - client.left;
			const double height = static_cast<double>(client.bottom) - client.top;
			if (width <= 0.0 || height <= 0.0)
				return false;

			// Minecraft's numeric FOV is vertical. The horizontal limit follows
			// the window aspect ratio; screen corners are not a circular cone.
			constexpr double radians = std::numbers::pi / 180.0;
			tan_half_vertical = std::tan(vertical_fov * radians * 0.5);
			tan_half_horizontal = tan_half_vertical * width / height;
			const double yaw = std::remainder(static_cast<double>(view.yaw), 360.0) * radians;
			const double pitch = view.pitch * radians;
			const double sy = std::sin(yaw), cy = std::cos(yaw);
			const double sp = std::sin(pitch), cp = std::cos(pitch);
			forward = { -sy * cp, -sp, cy * cp };
			right = { cy, 0.0, sy };
			up = { -sy * sp, cp, cy * sp };
			return std::isfinite(tan_half_horizontal) && tan_half_horizontal > 0.0;
		}

		bool contains(const maths::vector3d& eye, const maths::vector3d& point, double& angle) const
		{
			const auto offset = point - eye;
			const double depth = offset.x * forward.x + offset.y * forward.y + offset.z * forward.z;
			const double x = offset.x * right.x + offset.y * right.y + offset.z * right.z;
			const double y = offset.x * up.x + offset.y * up.y + offset.z * up.z;
			if (!std::isfinite(depth) || !std::isfinite(x) || !std::isfinite(y) || depth <= 1.e-6 ||
				std::abs(x) > depth * tan_half_horizontal || std::abs(y) > depth * tan_half_vertical)
				return false;
			// Rank accepted points by their true 3D angle, not yaw/pitch length.
			angle = maths::to_degrees(std::atan2(std::hypot(x, y), depth));
			return std::isfinite(angle);
		}
	};

	maths::angles aim_delta(const maths::vector3d& eye, const maths::vector3d& point,
		const maths::angles& view)
	{
		const auto offset = point - eye;
		const double yaw = maths::to_degrees(std::atan2(-offset.x, offset.z));
		const double pitch = -maths::to_degrees(std::atan2(offset.y, std::hypot(offset.x, offset.z)));
		return { static_cast<float>(std::remainder(yaw - view.yaw, 360.0)),
			static_cast<float>(pitch - view.pitch) };
	}

	bool crosshair_hit(const maths::vector3d& eye, const maths::vector3d& direction,
		const maps::Box& box, double reach, double& hit_distance)
	{
		// Revalidate the vanilla crosshair hint against the current look ray.
		const double origins[]{ eye.x, eye.y, eye.z };
		const double directions[]{ direction.x, direction.y, direction.z };
		const double lower[]{ box.minX.get(), box.minY.get(), box.minZ.get() };
		const double upper[]{ box.maxX.get(), box.maxY.get(), box.maxZ.get() };
		if (platform::jni_context::clear_exception()) return false;
		double ray_near = 0.0, ray_far = reach;
		for (int axis = 0; axis < 3; ++axis)
		{
			if (!std::isfinite(origins[axis]) || !std::isfinite(directions[axis]) ||
				!std::isfinite(lower[axis]) || !std::isfinite(upper[axis]) || lower[axis] > upper[axis])
				return false;
			if (std::abs(directions[axis]) < 1.e-12)
			{
				if (origins[axis] < lower[axis] || origins[axis] > upper[axis]) return false;
				continue;
			}
			double first = (lower[axis] - origins[axis]) / directions[axis];
			double second = (upper[axis] - origins[axis]) / directions[axis];
			if (first > second) std::swap(first, second);
			ray_near = (std::max)(ray_near, first);
			ray_far = (std::min)(ray_far, second);
			if (ray_near > ray_far) return false;
		}
		hit_distance = ray_near;
		return true;
	}

	struct candidate
	{
		maps::Entity entity{};
		maths::vector3d point{};
		maths::angles delta{};
		double distance = 0.0;
		double angle = (std::numeric_limits<double>::max)();
		int region = -1;
		bool complete = false;
		bool valid = false;
	};

	bool choose_region(const maps::Entity& entity, const aim_regions::model& model,
		const click_aim_engine::config& settings, const click_aim_engine::runtime& state,
		const maths::vector3d& eye, const maths::angles& view, const targeting_view& frustum,
		double reach, candidate& result)
	{
		const bool same_target = state.aim_target && entity.is_same_object(state.aim_target);
		if (platform::jni_context::clear_exception()) return false;
		const int cached = same_target ? state.selected_region : -1;
		const auto selected = [&](int id) {
			return std::find(settings.regions.begin(), settings.regions.end(), id) != settings.regions.end();
		};
		const auto assign = [&](int id, const maths::vector3d& requested, bool complete) {
			const auto offset = requested - eye;
			const double length = offset.length();
			if (std::isfinite(length) && length <= 1.e-6 && complete) {
				result.point = requested; result.angle = 0; result.region = id;
				result.complete = true; result.valid = true; result.delta = {};
				return true;
			}
			maths::vector3d point{};
			if (!std::isfinite(length) || length <= 1.e-6 ||
				!model.hit(id, eye, offset * (1.0 / length), reach, point)) return false;
			double angle = 0;
			if (!frustum.contains(eye, point, angle) || !has_line_of_sight(
				maps::ClientWorld(state.world.object_instance, false),
				maps::ClientPlayerEntity(state.player.object_instance, false), eye, point)) return false;
			result.point = point; result.angle = complete ? 0.0 : angle; result.region = id;
			result.complete = complete; result.valid = true;
			result.delta = complete ? maths::angles{} : aim_delta(eye, point, view);
			return true;
		};

		// Once aligned, this target gets no second attempt until a new click.
		// Only the cached point's safety is revalidated; no nearest-region search.
		if (same_target && state.aim_complete && selected(cached))
			return assign(cached, model.center(cached), true);

		// Check membership first. Being anywhere inside a selected region is
		// sufficient: do not pull an already valid aim toward the cell center.
		for (int id : settings.regions) {
			maths::vector3d hit{};
			if (model.hit(id, eye, frustum.forward, reach, hit) && assign(id, hit, true)) return true;
		}
		if (selected(cached)) return assign(cached, model.center(cached), false);

		// True central angle is monotonic with perspective pixel distance from
		// the crosshair, so this chooses the closest projected region center.
		candidate nearest{};
		for (int id : settings.regions) {
			const auto point = model.center(id);
			double angle = 0;
			if (!frustum.contains(eye, point, angle) || angle >= nearest.angle) continue;
			if (!assign(id, point, false)) continue;
			if (result.angle < nearest.angle) nearest = result;
		}
		result = nearest;
		return result.valid;
	}
}

void click_aim_engine::reset(runtime& state)
{
	release_target(state);
	clear_aim(state);
	state.world.clear_ref();
	state.player.clear_ref();
	state.left_button_was_down = false;
	state.released_since = {};
	deactivate(state);
}

void click_aim_engine::run(::cache& cache, config& settings, runtime& state)
{
	const HWND minecraft_window = gui::get_window();
	if (gui::draw || !minecraft_window || GetForegroundWindow() != minecraft_window ||
		!cache.is_valid() || !cache.instance || !cache.player || !cache.world ||
		!cache.players || !jni::get_env())
	{
		reset(state);
		return;
	}

	struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } clear_exception;
	try
	{
		jni::frame frame{ 384 };
		if (cache.instance.currentScreen.get() || platform::jni_context::clear_exception() ||
			cache.player.isAlive.call() != JNI_TRUE || platform::jni_context::clear_exception())
		{
			reset(state);
			return;
		}

		// A respawn/world change must never reuse a lock, even if entity IDs repeat.
		const bool same_session = state.world && state.player &&
			state.world.is_same_object(cache.world) && state.player.is_same_object(cache.player);
		if (platform::jni_context::clear_exception()) { reset(state); return; }
		if (!same_session)
		{
			reset(state);
			state.world = cache.world;
			state.player = cache.player;
			if (platform::jni_context::clear_exception() || !state.world || !state.player)
			{
				reset(state);
				return;
			}
		}

		const auto now = steady_clock::now();
		const bool left_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
		if (!left_down)
		{
			state.left_button_was_down = false;
			if (state.released_since.time_since_epoch().count() == 0) state.released_since = now;
			if (!settings.last_target || now - state.released_since >= click_release_grace) {
				release_target(state); clear_aim(state);
			}
			deactivate(state);
			return;
		}
		const bool pressed = !state.left_button_was_down;
		state.left_button_was_down = true;
		if (state.released_since.time_since_epoch().count() != 0 &&
			now - state.released_since >= click_release_grace) { release_target(state); clear_aim(state); }
		state.released_since = {};
		if (pressed) clear_aim(state);

		if (settings.only_weapon && !has_combat_weapon(cache.player))
		{
			reset(state);
			return;
		}

		settings.fov = std::clamp(settings.fov, modules::catalog::click_aim_assist_fov_minimum,
			modules::catalog::click_aim_assist_fov_maximum);
		settings.strength = std::clamp(settings.strength, 0.05f, 1.0f);
		if (!modules::settings::normalize(settings, schema()) || settings.regions.empty()) {
			release_target(state); deactivate(state); return;
		}

		// Follow vanilla entity reach; never extend it while retaining a target.
		const bool reach_available = cache.player.getEntityInteractionRange.is_valid();
		if (platform::jni_context::clear_exception() || !reach_available) { reset(state); return; }
		const double entity_reach = cache.player.getEntityInteractionRange.call();
		if (platform::jni_context::clear_exception() || !std::isfinite(entity_reach) || entity_reach <= 0.0)
		{
			reset(state);
			return;
		}

		const float delta_time = state.last_update.time_since_epoch().count() == 0
			? 0.005f
			: std::clamp(std::chrono::duration<float>(now - state.last_update).count(), 0.001f, 0.05f);
		state.last_update = now;
		if (!settings.last_target && state.locked_target) release_target(state);

		const maths::vector3d eye = eye_position(cache.player);
		const maths::angles view = cache.player.get_angles();
		targeting_view frustum{};
		if (!frustum.initialize(minecraft_window, settings.fov, view)) { reset(state); return; }

		candidate best{}, locked{}, clicked{};
		bool lock_eligible = false;
		double best_score = (std::numeric_limits<double>::max)();
		int best_priority = -1;
		maps::Entity click_hint{};
		if (settings.last_target && pressed)
			click_hint = services::target::crosshair_entity(cache.instance);
		jni::array<maps::Object> player_array = cache.players.toArray.call();
		if (!player_array || platform::jni_context::clear_exception()) { reset(state); return; }

		// Keep alternatives in the same scan, allowing immediate handoff when a
		// lock becomes invalid or its visibility/FOV grace expires.
		for (const maps::Object& object : player_array.to_vector())
		{
			struct clear_candidate_exception {
				~clear_candidate_exception() { platform::jni_context::clear_exception(); }
			} clear_candidate;
			maps::Entity entity(object.object_instance, false);
			if (!entity || entity.is_same_object(cache.player) ||
				!entity.is_instance_of<maps::PlayerEntity>() ||
				entity.isAlive.call() != JNI_TRUE || platform::jni_context::clear_exception() ||
				entity.isInvisible.call() == JNI_TRUE || platform::jni_context::clear_exception() ||
				friends::is_friend(entity) || services::anti_bot::is_bot(entity) ||
				platform::jni_context::clear_exception())
				continue;

			const int id = entity.getId.call();
			maps::Box box = entity.get_bounding_box();
			if (id < 0 || !box || platform::jni_context::clear_exception()) continue;
			const double distance = distance_to_box(eye, box);
			if (platform::jni_context::clear_exception() || !std::isfinite(distance) || distance > entity_reach)
				continue;
			const bool lock_match = state.locked_target && entity.is_same_object(state.locked_target);
			if (platform::jni_context::clear_exception()) continue;
			if (lock_match) lock_eligible = true;

			if (!entity.yaw.is_valid()) continue;
			const double target_yaw = entity.yaw.get();
			const aim_regions::bounds bounds{{box.minX.get(), box.minY.get(), box.minZ.get()},
				{box.maxX.get(), box.maxY.get(), box.maxZ.get()}};
			aim_regions::model model{};
			if (platform::jni_context::clear_exception() || !model.initialize(bounds, target_yaw)) continue;
			candidate evaluated{};
			if (!choose_region(entity, model, settings, state, eye, view, frustum, entity_reach, evaluated) ||
				platform::jni_context::clear_exception()) continue;
			evaluated.entity = entity;
			evaluated.distance = distance;
			const double score = evaluated.angle + distance * 0.06;
			const int priority = evaluated.complete ? (state.aim_target && entity.is_same_object(state.aim_target) ? 2 : 1) : 0;
			if (priority > best_priority || (priority == best_priority && score < best_score)) {
				best = evaluated; best_score = score; best_priority = priority;
			}
			if (lock_match) locked = evaluated;

			if (click_hint && entity.is_same_object(click_hint) && !platform::jni_context::clear_exception())
			{
				double hit_distance = 0.0;
				if (crosshair_hit(eye, frustum.forward, box, entity_reach, hit_distance) &&
					has_line_of_sight(cache.world, cache.player, eye, eye + frustum.forward * hit_distance))
				{
					clicked = evaluated;
				}
			}
		}

		if (settings.last_target)
		{
			if (clicked.valid)
			{
				// Explicit user selection overrides an existing lock or grace.
				best = clicked;
				lock_target(state, best.entity);
			}
			else if (state.locked_target)
			{
				if (locked.valid)
				{
					best = locked;
					state.lock_lost_since = {};
				}
				else if (lock_eligible)
				{
					// Remember brief FOV/visibility losses, without turning
					// toward the invalid point or another player during grace.
					if (state.lock_lost_since.time_since_epoch().count() == 0) state.lock_lost_since = now;
					if (now - state.lock_lost_since < lost_target_grace) { deactivate(state); return; }
					release_target(state);
				}
				else
				{
					// Dead, absent, friendly, invisible, bot or beyond reach.
					release_target(state);
				}
			}
			if (best.valid && !state.locked_target) lock_target(state, best.entity);
		}

		if (!best.valid) { deactivate(state); return; }
		if (platform::jni_context::clear_exception() || (settings.last_target && !state.locked_target))
		{
			reset(state);
			return;
		}

		if (!state.aim_target || !state.aim_target.is_same_object(best.entity)) state.aim_target = best.entity;
		state.selected_region = best.region;
		state.aim_complete = best.complete;
		if (platform::jni_context::clear_exception() || !state.aim_target) { reset(state); return; }

		if (!best.complete && (std::abs(best.delta.yaw) > 0.001f || std::abs(best.delta.pitch) > 0.001f))
		{
			const float response = 8.0f + settings.strength * 38.0f;
			const float blend = 1.0f - std::exp(-response * delta_time);
			const float maximum_yaw = (180.0f + 720.0f * settings.strength) * delta_time;
			const float maximum_pitch = (130.0f + 520.0f * settings.strength) * delta_time;
			const float yaw_step = std::clamp(best.delta.yaw * blend, -maximum_yaw, maximum_yaw);
			const float pitch_step = std::clamp(best.delta.pitch * blend, -maximum_pitch, maximum_pitch);
			cache.player.yaw.set(view.yaw + yaw_step);
			cache.player.pitch.set(std::clamp(view.pitch + pitch_step, -90.0f, 90.0f));
			if (platform::jni_context::clear_exception()) { reset(state); return; }
		}
		state.active = true;
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		reset(state);
	}
}

void click_aim_engine::render_options(config& settings, const char* id_suffix)
{
    ImGui::PushID(id_suffix);
    gui::settings::render(settings, schema());
    gui::aim_region_picker::render(settings.regions);
    ImGui::PopID();
}
