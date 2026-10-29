#include "../../ui/settings_renderer.hpp"
#include "../../services/game/anti_bot_service.hpp"
#include "blatant_aim_engine.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <string>

#include <imgui/imgui.h>

#include "../../friends/friends.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"

namespace
{
	using steady_clock = std::chrono::steady_clock;
	std::mt19937_64 random_engine(std::random_device{}());
void deactivate(blatant_aim_engine::runtime& state)
	{
		state.active = false;
		state.last_update = {};
	}

	blatant_aim_engine::aim_point sanitize_point(blatant_aim_engine::aim_point point)
	{
		if (point == blatant_aim_engine::aim_point::body ||
			point == blatant_aim_engine::aim_point::random ||
			point == blatant_aim_engine::aim_point::adaptive)
			return point;

		// Old Head was 0 and old Legs was 2. Head becomes the safer Body mode,
		// while Legs becomes Adaptive instead of silently targeting feet.
		return static_cast<int>(point) == 2
			? blatant_aim_engine::aim_point::adaptive
			: blatant_aim_engine::aim_point::body;
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

	std::string entity_name(const maps::Entity& entity)
	{
		maps::String name = entity.getEntityName.call();
		const std::string value = name && !platform::jni_context::clear_exception()
			? name.to_string()
			: std::string{};
		platform::jni_context::clear_exception();
		return value;
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

	maths::vector3d point_at_ratio(const maps::Box& box, double ratio)
	{
		const double height = box.maxY.get() - box.minY.get();
		return {
			(box.minX.get() + box.maxX.get()) * 0.5,
			box.minY.get() + height * std::clamp(ratio, 0.35, 0.78),
			(box.minZ.get() + box.maxZ.get()) * 0.5
		};
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

	double angular_distance(const maths::vector3d& from, const maths::vector3d& to,
		const maths::angles& view, maths::angles* delta_out = nullptr)
	{
		const maths::angles delta = ((to - from).get_angles() - view).mod360();
		if (delta_out)
			*delta_out = delta;
		return std::hypot(static_cast<double>(delta.yaw), static_cast<double>(delta.pitch));
	}

	bool manual_target(
		::cache& cache,
		blatant_aim_engine::runtime& state,
		bool pressed)
	{
		if (!pressed)
			return false;

		maps::HitResult hit = cache.instance.crosshairTarget.get();
		maps::HitResult$Type entity_type = maps::HitResult$Type{}.ENTITY.get();
		if (!hit || !entity_type || platform::jni_context::clear_exception() ||
			!hit.getType.call().is_same_object(entity_type) || platform::jni_context::clear_exception())
			return false;

		maps::Entity entity = maps::EntityHitResult(hit).entity.get();
		if (!entity || !entity.is_instance_of<maps::PlayerEntity>() ||
			entity.isAlive.call() != JNI_TRUE || platform::jni_context::clear_exception() ||
			friends::is_friend(entity) || services::anti_bot::is_bot(entity))
			return false;

		state.locked_target_id = entity.getId.call();
		state.locked_target_name = entity_name(entity);
		return !platform::jni_context::clear_exception();
	}

	float update_random_ratio(
		blatant_aim_engine::runtime& state,
		int target_id,
		steady_clock::time_point now,
		float delta_time)
	{
		if (state.random_target_id != target_id)
		{
			state.random_target_id = target_id;
			state.random_ratio = std::uniform_real_distribution<float>(0.48f, 0.70f)(random_engine);
			state.random_goal_ratio = state.random_ratio;
			state.random_next_shift = now + std::chrono::milliseconds(
				std::uniform_int_distribution<int>(520, 900)(random_engine));
		}

		if (now >= state.random_next_shift)
		{
			const float offset = std::normal_distribution<float>(0.0f, 0.065f)(random_engine);
			state.random_goal_ratio = std::clamp(state.random_goal_ratio + offset, 0.44f, 0.74f);
			state.random_next_shift = now + std::chrono::milliseconds(
				std::uniform_int_distribution<int>(520, 950)(random_engine));
		}

		// The random point is a slow moving body-zone trajectory, not a new body
		// part every scan. This removes the old vertical head/body/legs snapping.
		const float blend = 1.0f - std::exp(-5.2f * std::clamp(delta_time, 0.001f, 0.05f));
		state.random_ratio += (state.random_goal_ratio - state.random_ratio) * blend;
		return state.random_ratio;
	}

	struct candidate
	{
		maps::Entity entity{};
		maps::Box box{};
		maths::vector3d point{};
		maths::angles delta{};
		std::string name{};
		int id = -1;
		double distance = 0.0;
		double angle = (std::numeric_limits<double>::max)();
		bool valid = false;
	};

	bool choose_adaptive_point(
		const maps::Box& box,
		const maps::ClientWorld& world,
		const maps::ClientPlayerEntity& player,
		const maths::vector3d& eye,
		const maths::angles& view,
		maths::vector3d& point,
		maths::angles& delta,
		double& angle)
	{
		constexpr std::array<double, 3> ratios{ 0.48, 0.59, 0.70 };
		bool found = false;
		double best = (std::numeric_limits<double>::max)();
		for (double ratio : ratios)
		{
			const maths::vector3d sample = point_at_ratio(box, ratio);
			if (!has_line_of_sight(world, player, eye, sample))
				continue;
			maths::angles sample_delta{};
			const double sample_angle = angular_distance(eye, sample, view, &sample_delta);
			if (sample_angle < best)
			{
				best = sample_angle;
				point = sample;
				delta = sample_delta;
				angle = sample_angle;
				found = true;
			}
		}
		return found;
	}
}

void blatant_aim_engine::reset(runtime& state)
{
	state = {};
}

void blatant_aim_engine::run(::cache& cache, config& settings, runtime& state)
{
	const bool left_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	if (!left_down)
	{
		state.left_button_was_down = false;
		deactivate(state);
		return;
	}
	const bool pressed = !state.left_button_was_down;
	state.left_button_was_down = true;

	const HWND minecraft_window = gui::get_window();
	if (gui::draw || !minecraft_window || GetForegroundWindow() != minecraft_window ||
		!cache.is_valid() || !cache.instance || !cache.player || !cache.world ||
		!cache.players || !jni::get_env())
	{
		reset(state);
		return;
	}

	try
	{
		jni::frame frame{ 384 };
		if (cache.instance.currentScreen.get() || platform::jni_context::clear_exception() ||
			(settings.only_weapon && !has_combat_weapon(cache.player)))
		{
			deactivate(state);
			return;
		}

		settings.fov = std::clamp(settings.fov, 10.0f, 180.0f);
		settings.max_distance = std::clamp(settings.max_distance, 1.0f, 6.0f);
		settings.strength = std::clamp(settings.strength, 0.05f, 1.0f);
		settings.point = sanitize_point(settings.point);

		const auto now = steady_clock::now();
		const float delta_time = state.last_update.time_since_epoch().count() == 0
			? 0.005f
			: std::clamp(std::chrono::duration<float>(now - state.last_update).count(), 0.001f, 0.05f);
		state.last_update = now;

		if (!settings.last_target)
		{
			state.locked_target_id = -1;
			state.locked_target_name.clear();
		}
		else
		{
			// A fresh physical click on another player is an explicit user choice and
			// immediately replaces the previous lock.
			manual_target(cache, state, pressed);
		}

		const maths::vector3d eye = eye_position(cache.player);
		const maths::angles view = cache.player.get_angles();
		const double fov_radius = settings.fov * 0.5;
		candidate best{};
		bool locked_target_alive_and_near = state.locked_target_id == -1;

		jni::array<maps::Object> player_array = cache.players.toArray.call();
		if (!player_array || platform::jni_context::clear_exception())
		{
			reset(state);
			return;
		}

		for (const maps::Object& object : player_array.to_vector())
		{
			maps::Entity entity(object.object_instance, false);
			if (!entity || entity.is_same_object(cache.player) ||
				!entity.is_instance_of<maps::PlayerEntity>() ||
				entity.isAlive.call() != JNI_TRUE || platform::jni_context::clear_exception() ||
				entity.isInvisible.call() == JNI_TRUE || platform::jni_context::clear_exception() ||
				friends::is_friend(entity) || services::anti_bot::is_bot(entity))
				continue;

			const int id = entity.getId.call();
			const std::string name = entity_name(entity);
			maps::Box box = entity.get_bounding_box();
			if (id < 0 || !box || platform::jni_context::clear_exception())
				continue;

			const double distance = distance_to_box(eye, box);
			if (platform::jni_context::clear_exception())
				continue;

			const bool lock_match = id == state.locked_target_id &&
				(state.locked_target_name.empty() || name == state.locked_target_name);
			if (lock_match && distance <= 40.0)
				locked_target_alive_and_near = true;
			if (state.locked_target_id != -1 && !lock_match)
				continue;
			if (distance > settings.max_distance)
				continue;

			maths::vector3d point{};
			maths::angles delta{};
			double angle = (std::numeric_limits<double>::max)();
			bool visible = false;

			if (settings.point == aim_point::adaptive)
			{
				visible = choose_adaptive_point(
					box, cache.world, cache.player, eye, view, point, delta, angle);
			}
			else
			{
				const double ratio = settings.point == aim_point::random && state.random_target_id == id
					? state.random_ratio
					: 0.59;
				point = point_at_ratio(box, ratio);
				visible = has_line_of_sight(cache.world, cache.player, eye, point);
				if (visible)
					angle = angular_distance(eye, point, view, &delta);
			}

			// A real circular angular cone replaces the old independent yaw/pitch box.
			if (!visible || angle > fov_radius)
				continue;

			const double score = angle + distance * 0.06;
			const double best_score = best.valid
				? best.angle + best.distance * 0.06
				: (std::numeric_limits<double>::max)();
			if (score < best_score)
			{
				best.entity = entity;
				best.box = box;
				best.point = point;
				best.delta = delta;
				best.name = name;
				best.id = id;
				best.distance = distance;
				best.angle = angle;
				best.valid = true;
			}
		}

		if (state.locked_target_id != -1 && !locked_target_alive_and_near)
		{
			state.locked_target_id = -1;
			state.locked_target_name.clear();
			state.random_target_id = -1;
			deactivate(state);
			return;
		}

		if (!best.valid)
		{
			deactivate(state);
			return;
		}

		if (settings.last_target && state.locked_target_id == -1)
		{
			// The first assisted player becomes the lock even if the initial physical
			// click landed just outside their hitbox.
			state.locked_target_id = best.id;
			state.locked_target_name = best.name;
		}

		if (settings.point == aim_point::random)
		{
			const float ratio = update_random_ratio(state, best.id, now, delta_time);
			best.point = point_at_ratio(best.box, ratio);
			if (!has_line_of_sight(cache.world, cache.player, eye, best.point))
			{
				deactivate(state);
				return;
			}
			best.angle = angular_distance(eye, best.point, view, &best.delta);
			if (best.angle > fov_radius)
			{
				deactivate(state);
				return;
			}
		}
		else
		{
			state.random_target_id = -1;
		}

		if (std::abs(best.delta.yaw) > 0.12f || std::abs(best.delta.pitch) > 0.12f)
		{
			// Time based response keeps the same feel when the worker loop stalls or
			// runs at a slightly different frequency.
			const float response = 8.0f + settings.strength * 38.0f;
			const float blend = 1.0f - std::exp(-response * delta_time);
			const float maximum_yaw = (180.0f + 720.0f * settings.strength) * delta_time;
			const float maximum_pitch = (130.0f + 520.0f * settings.strength) * delta_time;
			const float yaw_step = std::clamp(best.delta.yaw * blend, -maximum_yaw, maximum_yaw);
			const float pitch_step = std::clamp(best.delta.pitch * blend, -maximum_pitch, maximum_pitch);

			cache.player.yaw.set(view.yaw + yaw_step);
			cache.player.pitch.set(std::clamp(view.pitch + pitch_step, -90.0f, 90.0f));
			if (platform::jni_context::clear_exception())
			{
				deactivate(state);
				return;
			}
		}

		state.active = true;
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		reset(state);
	}
}

void blatant_aim_engine::render_options(config& settings, const char* id_suffix)
{
    ImGui::PushID(id_suffix);
    gui::settings::render(settings, schema());
    ImGui::PopID();
}
