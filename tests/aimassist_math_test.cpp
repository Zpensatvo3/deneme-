#include "../src/modules/combat/legit_aim_math.hpp"
#include <cassert>
#include <limits>
#include <cstdio>
#include <initializer_list>

using namespace legit_aim_math;
int main()
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    angles desired;
    assert(angles_to({}, {0, 0, -4}, desired) && std::abs(std::abs(desired.yaw) - 180.f) < .001f);
    assert(angles_to({}, {0, 4, 0}, desired) && std::abs(desired.pitch + 90.f) < .001f);
    assert(angles_to({}, {4, 0, 0}, desired) && std::abs(desired.yaw + 90.f) < .001f);
    assert(!angles_to({}, {}, desired));
    assert(!angles_to({}, {infinity, 0, 0}, desired));
    assert(wrap(181.f) == -179.f && wrap(-181.f) == 179.f);
    assert(std::isfinite(wrap(infinity)));

    const auto single_step = [](angles delta, float strength, float seconds, float blend, float manual) {
        motion_state state;
        return motion(state, delta, strength, seconds, blend, manual);
    };
    const auto zero = single_step({40, 20}, 0, .016f, 1, 1);
    assert(zero.yaw == 0 && zero.pitch == 0);
    assert(single_step({40, 20}, 100, 0, 1, 1).yaw == 0);
    assert(single_step({nan, 0}, 50, .016f, 1, 1).yaw == 0);
    assert(single_step({40, 0}, infinity, .016f, 1, 1).yaw == 0);
    assert(single_step({40, 0}, 50, .016f, 1, 0).yaw == 0);
    for (float seconds : {.001f, .016f, .05f, 10.f}) {
        const auto step = single_step({90, 80}, 42, seconds, 1, 1);
        const float limit = maximum_speed(42) * std::min(seconds, .05f);
        assert(std::abs(step.yaw) <= limit + .0001f && std::abs(step.pitch) <= limit * .72f + .0001f);
    }

    // The exact damped response for a small stationary error is independent of FPS
    // when the rate limiter and engagement ramp are not active.
    float reference = 0;
    for (int fps : {50, 100, 240}) {
        float yaw = 0;
        motion_state state;
        for (int frame = 0; frame < fps / 5; ++frame) yaw += motion(state, {1.f - yaw, 0}, 42, 1.f / fps, 1, 1).yaw;
        if (reference == 0) reference = yaw;
        assert(std::abs(yaw - reference) < .00002f && yaw <= 1.f);
    }

    // Same 42% setting: softer first frame and stronger follow-up than the old calculation.
    motion_state strong;
    const float old_first = 10.f * (1.f - std::exp(-(3.f + .42f * 11.f) * .016f));
    const auto first = motion(strong, {10, 0}, 42, .016f, 1, 1);
    const auto second = motion(strong, {10.f - first.yaw, 0}, 42, .016f, 1, 1);
    assert(first.yaw > 0 && first.yaw < old_first && second.yaw > first.yaw);
    reset_motion(strong);
    float stronger_yaw = 0;
    for (int frame = 0; frame < 15; ++frame)
        stronger_yaw += motion(strong, {10.f - stronger_yaw, 0}, 42, .01f, 1, 1).yaw;
    const float old_follow = 10.f * (1.f - std::exp(-(3.f + .42f * 11.f) * .15f));
    assert(stronger_yaw > old_follow && stronger_yaw <= 10.f);
    const auto softer_start = single_step({10, 0}, 42, .016f, .1f, 1);
    assert(softer_start.yaw < first.yaw);

    // No carried velocity after cancellation, manual override or zero strength.
    strong.velocity = {200, 100};
    assert(motion(strong, {10, 5}, 0, .016f, 1, 1).yaw == 0);
    assert(strong.velocity.yaw == 0 && strong.velocity.pitch == 0);
    strong.velocity = {200, 100};
    assert(motion(strong, {10, 5}, 42, .016f, 1, 0).yaw == 0);
    assert(strong.velocity.yaw == 0 && strong.velocity.pitch == 0);
    strong.velocity = {200, 100};
    const auto reversed = motion(strong, {-10, -5}, 42, .016f, 1, 1);
    assert(reversed.yaw <= 0 && reversed.pitch <= 0);
    strong.velocity = {200, 100};
    const auto near = motion(strong, {.01f, .01f}, 42, .016f, 1, 1);
    assert(near.yaw <= .01f && near.pitch <= .01f);
    strong.velocity = {200, 100};
    const auto manual = motion(strong, {10, 5}, 42, .016f, 1, .1f);
    assert(manual.yaw <= maximum_speed(42) * .1f * .016f + .00001f);
    assert(std::abs(strong.velocity.yaw) <= maximum_speed(42) * .1f + .00001f);
    reset_motion(strong); assert(strong.velocity.yaw == 0 && strong.velocity.pitch == 0);

    // Same strength and same target: observed-motion compensation reduces
    // steady tracking lag without aiming past the target's current bearing.
    float previous_lag = -1;
    for (int fps : {40, 60, 240}) {
        motion_state tracking, position_only;
        const float dt = 1.f / fps;
        float assisted_yaw = 0, baseline_yaw = 0;
        observe_target(tracking, {10, 0}, 0);
        for (int frame = 1; frame <= fps / 2; ++frame) {
            const float target = 10.f + 50.f * frame * dt;
            observe_target(tracking, {target, 0}, dt);
            assisted_yaw += motion(tracking, {target - assisted_yaw, 0}, 42, dt, 1, 1).yaw;
            baseline_yaw += motion(position_only, {target - baseline_yaw, 0}, 42, dt, 1, 1).yaw;
            assert(assisted_yaw <= target + .0001f);
        }
        const float lag = 35.f - assisted_yaw;
        assert(lag >= -.0001f && lag < (35.f - baseline_yaw) * .6f);
        if (previous_lag >= 0) assert(std::abs(lag - previous_lag) < .05f);
        previous_lag = lag;
        const auto estimate = tracking.target_velocity;
        stop_motion(tracking);
        assert(tracking.velocity.yaw == 0 && tracking.target_velocity.yaw == estimate.yaw);
        reset_motion(tracking);
        assert(!tracking.target_valid && tracking.target_velocity.yaw == 0);
    }

    // Absolute bearings, yaw wrap and tick-spaced updates. Camera corrections
    // must not be fed back into the target-speed estimate.
    motion_state observation;
    observe_target(observation, {179, 0}, 0);
    observe_target(observation, {-179, 0}, .05f);
    assert(observation.target_velocity.yaw > 0 && observation.target_velocity.yaw < 40.f);
    const float old_velocity = observation.target_velocity.yaw;
    observe_target(observation, {-179, 0}, .02f);
    assert(observation.target_velocity.yaw == old_velocity);
    observe_target(observation, {-178, 0}, .03f);
    assert(observation.target_velocity.yaw > 0);
    for (int frame = 0; frame < 60; ++frame) observe_target(observation, {-178, 0}, .01f);
    assert(std::abs(observation.target_velocity.yaw) < .001f);
    observe_target(observation, {-100, 0}, .01f); // Discontinuous bearing: no fake momentum.
    assert(observation.target_velocity.yaw == 0 && observation.velocity.yaw == 0);
    observe_target(observation, {nan, 0}, .01f);
    assert(!observation.target_valid && observation.target_velocity.yaw == 0);
    for (int fps : {40, 60, 240}) {
        const float dt = 1.f / fps;
        const auto control = manual_input({-220.f * dt, 0}, {}, {10, 0}, dt);
        assert(!control.release_target && std::abs(control.influence - .5f) < .0001f);
        assert(manual_input({-500.f * dt, 0}, {}, {10, 0}, dt).release_target);
        assert(!manual_input({500.f * dt, 0}, {}, {10, 0}, dt).release_target);
    }

    const bounds box{-1, 0, 3, 1, 2, 4};
    assert(valid(box) && ray_intersects({0, 1, 0}, direction({0, 0}), box, 4));
    assert(!ray_intersects({0, 1, 0}, direction({180, 0}), box, 4));
    assert(!ray_intersects({0, 1, 0}, direction({0, 0}), box, 2.9));
    assert(!valid({1, 0, 0, -1, 2, 3}));
    assert(!valid({0, 0, 0, infinity, 2, 3}));
    const bounds moved{2, 0, 3, 3, 2, 4};
    const auto smoothed = filtered({0, 1, 3.5}, {2.5, 1, 3.5}, moved, .005f);
    assert(smoothed.x >= moved.min_x && smoothed.x <= moved.max_x);
    assert(std::abs(distance_to_box({0, 1, 0}, box) - 3.) < .00001);
    // World-space eye Y must not be replaced by height-above-feet or JNI's
    // zero on a missing method. A close player at Y=64 is still in range.
    const point elevated_eye{0, 65.62, 0};
    const point elevated_target{1, 65.3, 3.3};
    const bounds elevated_box{.7, 64, 3, 1.3, 65.8, 3.6};
    assert(distance_to_box(elevated_eye, elevated_box) < 4.2);
    assert(distance_to_box({0, 0, 0}, elevated_box) > 4.2);
    assert(distance_to_box({0, 1.62, 0}, elevated_box) > 4.2);
    assert(angles_to(elevated_eye, elevated_target, desired));
    assert(std::hypot(desired.yaw, desired.pitch) < 60);
    motion_state elevated_motion;
    const auto elevated_step = motion(elevated_motion, desired, 42, .016f, 1, 1);
    assert(std::abs(elevated_step.yaw) > .001f);
    std::puts("Aim same-strength tracking, observed angular velocity, damping, resets, FPS and geometry passed.");
}
