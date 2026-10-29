#pragma once
#include <algorithm>
#include <cmath>

// Native-free geometry and motion primitives; no JNI, input or wall clock.
namespace legit_aim_math
{
    constexpr double pi = 3.14159265358979323846;
    struct point { double x = 0, y = 0, z = 0; };
    struct angles { float yaw = 0, pitch = 0; };
    struct bounds { double min_x, min_y, min_z, max_x, max_y, max_z; };
    inline bool finite(point p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
    inline bool finite(angles a) { return std::isfinite(a.yaw) && std::isfinite(a.pitch); }
    inline bool valid(bounds b)
    {
        return finite(point{b.min_x, b.min_y, b.min_z}) && finite(point{b.max_x, b.max_y, b.max_z}) &&
            b.min_x < b.max_x && b.min_y < b.max_y && b.min_z < b.max_z;
    }
    inline float wrap(float angle) { return std::isfinite(angle) ? std::remainder(angle, 360.f) : 0.f; }
    inline bool angles_to(point eye, point target, angles& result)
    {
        if (!finite(eye) || !finite(target)) return false;
        const double x = target.x - eye.x, y = target.y - eye.y, z = target.z - eye.z;
        const double horizontal = std::hypot(x, z);
        if (!std::isfinite(horizontal) || !std::isfinite(y) || std::hypot(horizontal, y) < 1e-9) return false;
        result = {static_cast<float>(std::atan2(-x, z) * 180. / pi),
            static_cast<float>(std::atan2(-y, horizontal) * 180. / pi)};
        return finite(result);
    }
    inline point clamp_point(point p, bounds b)
    {
        return {std::clamp(p.x, b.min_x, b.max_x), std::clamp(p.y, b.min_y, b.max_y),
            std::clamp(p.z, b.min_z, b.max_z)};
    }
    inline double distance_to_box(point eye, bounds b)
    {
        const auto closest = clamp_point(eye, b);
        return std::hypot(std::hypot(closest.x - eye.x, closest.z - eye.z), closest.y - eye.y);
    }
    inline point filtered(point previous, point target, bounds b, float seconds)
    {
        if (!finite(previous) || !finite(target) || !valid(b)) return target;
        if (!std::isfinite(seconds) || seconds < 0) return clamp_point(target, b);
        const double blend = 1. - std::exp(-30. * std::clamp(static_cast<double>(seconds), 0., .05));
        // Never chase a trailing point that has left a fast-moving target's box.
        return clamp_point({previous.x + (target.x - previous.x) * blend,
            previous.y + (target.y - previous.y) * blend, previous.z + (target.z - previous.z) * blend}, b);
    }
    inline point direction(angles view)
    {
        const double yaw = view.yaw * pi / 180., pitch = view.pitch * pi / 180.;
        return {-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
    }
    inline bool ray_intersects(point eye, point ray, bounds box, double distance)
    {
        if (!finite(eye) || !finite(ray) || !valid(box) || !std::isfinite(distance) || distance <= 0) return false;
        double minimum = 0, maximum = distance;
        auto axis = [&](double origin, double component, double low, double high) {
            if (std::abs(component) < 1e-7) return origin >= low && origin <= high;
            double a = (low - origin) / component, b = (high - origin) / component;
            if (a > b) std::swap(a, b);
            minimum = std::max(minimum, a); maximum = std::min(maximum, b);
            return minimum <= maximum;
        };
        return axis(eye.x, ray.x, box.min_x, box.max_x) && axis(eye.y, ray.y, box.min_y, box.max_y) &&
            axis(eye.z, ray.z, box.min_z, box.max_z);
    }
    struct manual_control { float influence = 1; bool release_target = false; };
    inline manual_control manual_input(angles actual, angles expected, angles target_delta, float seconds)
    {
        if (!finite(actual) || !finite(expected) || !finite(target_delta) || !std::isfinite(seconds) || seconds <= 0) return {};
        const float yaw = wrap(actual.yaw - expected.yaw), pitch = actual.pitch - expected.pitch;
        const float speed = std::hypot(yaw, pitch) / seconds;
        const bool opposing = yaw * target_delta.yaw + pitch * target_delta.pitch < 0;
        if (!opposing || speed <= 44.f) return {};
        return {std::clamp(1.f - speed / 440.f, 0.f, 1.f), speed > 440.f};
    }
    struct motion_state
    {
        angles velocity{}, target_velocity{}, last_target{};
        float observation_elapsed = 0;
        bool target_valid = false;
    };
    inline void reset_motion(motion_state& state) { state = {}; }
    inline void stop_motion(motion_state& state) { state.velocity = {}; }
    inline void observe_target(motion_state& state, angles target, float seconds)
    {
        if (!finite(target) || !std::isfinite(seconds) || seconds < 0 || seconds > .10f) {
            reset_motion(state); return;
        }
        if (!state.target_valid) {
            state.last_target = target; state.target_valid = true;
            state.target_velocity = {}; state.observation_elapsed = 0; return;
        }
        state.observation_elapsed += seconds;
        const float yaw = wrap(target.yaw - state.last_target.yaw);
        const float pitch = target.pitch - state.last_target.pitch;
        const bool changed = std::hypot(yaw, pitch) >= .001f;
        const float elapsed = state.observation_elapsed;
        if (changed && elapsed >= .002f) {
            const angles measured{yaw / elapsed, pitch / elapsed};
            // Do not turn teleports/discontinuous point jumps into momentum.
            if (!finite(measured) || std::hypot(yaw, pitch) > 20.f || std::hypot(measured.yaw, measured.pitch) > 720.f) {
                stop_motion(state); state.target_velocity = {};
            } else {
                const float alpha = 1.f - std::exp(-elapsed / .025f);
                // A reversal should brake the old estimate before chasing it.
                if (measured.yaw * state.target_velocity.yaw < 0) state.target_velocity.yaw = 0;
                if (measured.pitch * state.target_velocity.pitch < 0) state.target_velocity.pitch = 0;
                state.target_velocity.yaw += (measured.yaw - state.target_velocity.yaw) * alpha;
                state.target_velocity.pitch += (measured.pitch - state.target_velocity.pitch) * alpha;
            }
            state.last_target = target; state.observation_elapsed = 0;
        } else if (!changed && elapsed > .075f) {
            // Position updates may arrive at tick rate. Keep velocity between
            // updates briefly, but let a stopped target's estimate fade out.
            const float decay = std::exp(-seconds / .025f);
            state.target_velocity.yaw *= decay; state.target_velocity.pitch *= decay;
        }
    }
    inline float maximum_speed(float strength)
    {
        if (!std::isfinite(strength) || strength <= 0) return 0;
        return 120.f + std::sqrt(std::clamp(strength / 100.f, 0.f, 1.f)) * 360.f;
    }
    inline float response_rate(float strength)
    {
        if (!std::isfinite(strength) || strength <= 0) return 0;
        return 10.f + std::sqrt(std::clamp(strength / 100.f, 0.f, 1.f)) * 28.f;
    }
    inline float damped_axis(float error, float& velocity, float rate, float seconds, float speed_limit,
        float target_speed = 0, float follow = 0)
    {
        // Critically damped response relative to observed target motion. Velocity is
        // carried between frames, rather than restarting an easing curve each
        // update. The final step is bounded by CURRENT error: this does not aim
        // at an extrapolated point or extend the configured range.
        const double decay = std::exp(-static_cast<double>(rate) * seconds);
        velocity = std::clamp(velocity, -speed_limit, speed_limit);
        target_speed = std::clamp(target_speed, -speed_limit, speed_limit);
        // The observed bearing is at the END of this elapsed frame. Reconstruct
        // its start before integrating, otherwise low FPS adds a second frame
        // of target movement and produces different tracking strength.
        const double lag_bias = rate > 0 ? 2. * (1. - std::clamp(follow, 0.f, 1.f)) * target_speed / rate : 0;
        const double start_error = static_cast<double>(error) - static_cast<double>(target_speed) * seconds - lag_bias;
        const double offset = -start_error;
        const double relative_velocity = static_cast<double>(velocity) - target_speed;
        const double correction = (relative_velocity + rate * offset) * seconds;
        const double requested = start_error + (offset + correction) * decay + static_cast<double>(target_speed) * seconds;
        const double next_velocity = (relative_velocity - rate * correction) * decay + target_speed;
        const double limit = std::min(std::abs(static_cast<double>(error)), static_cast<double>(speed_limit) * seconds);
        const float step = static_cast<float>(error >= 0 ? std::clamp(requested, 0., limit) : std::clamp(requested, -limit, 0.));
        velocity = static_cast<float>(std::clamp(next_velocity, -static_cast<double>(speed_limit), static_cast<double>(speed_limit)));
        // Arriving at the point must not leave momentum that overshoots it.
        if (step == error) velocity = 0;
        return step;
    }
    inline angles motion(motion_state& state, angles delta, float strength, float seconds, float blend, float manual_scale)
    {
        if (!finite(delta) || !std::isfinite(strength) || !std::isfinite(seconds) ||
            !finite(state.velocity) || !finite(state.target_velocity) || !std::isfinite(blend) || !std::isfinite(manual_scale) ||
            strength <= 0 || seconds < 0) { reset_motion(state); return {}; }
        if (blend <= 0 || manual_scale <= 0) { stop_motion(state); return {}; }
        if (seconds == 0) return {};
        seconds = std::clamp(seconds, 0.f, .05f);
        const float engagement = std::clamp(blend, 0.f, 1.f);
        const float eased = engagement * engagement * (3.f - 2.f * engagement);
        const float influence = eased * std::clamp(manual_scale, 0.f, 1.f);
        const float moving = std::clamp(std::hypot(state.target_velocity.yaw, state.target_velocity.pitch) / 180.f, 0.f, 1.f);
        const float rate = response_rate(strength) * std::sqrt(influence) * (1.f + .15f * moving);
        // Reducing assistance must also limit carried momentum, so manual aim
        // can override it immediately rather than fighting yesterday's speed.
        const float speed = maximum_speed(strength) * std::clamp(manual_scale, 0.f, 1.f);
        const float power = std::sqrt(std::clamp(strength / 100.f, 0.f, 1.f));
        const float follow = (.35f + .65f * power) * influence;
        return {damped_axis(wrap(delta.yaw), state.velocity.yaw, rate, seconds, speed, state.target_velocity.yaw, follow),
            damped_axis(delta.pitch, state.velocity.pitch, rate * .85f, seconds, speed * .72f, state.target_velocity.pitch, follow)};
    }
}
