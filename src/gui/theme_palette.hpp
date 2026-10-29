#pragma once

#include <algorithm>
#include <array>
#include <cmath>

// Shared, UI-independent palette data and interrupted-transition handling.
namespace gui::theme
{
    struct color { float r, g, b; };
    struct palette { const char* name; std::array<color, 3> stops; };
    inline constexpr std::array<palette, 8> palettes{{
        { "onestep", {{{.60f, .32f, 1.f}, {.88f, .44f, .92f}, {.38f, .62f, 1.f}}} },
        { "Crimson", {{{.94f, .25f, .36f}, {1.f, .46f, .55f}, {.93f, .30f, .65f}}} },
        { "Emerald", {{{.16f, .76f, .44f}, {.38f, .94f, .64f}, {.18f, .80f, .74f}}} },
        { "Ocean", {{{.22f, .52f, .96f}, {.37f, .77f, 1.f}, {.34f, .45f, .96f}}} },
        { "Rose", {{{.91f, .34f, .61f}, {1.f, .58f, .78f}, {.81f, .41f, .89f}}} },
        { "Cyber", {{{.10f, .77f, .83f}, {.24f, .96f, .85f}, {.27f, .62f, 1.f}}} },
        { "Sunset", {{{1.f, .46f, .20f}, {1.f, .76f, .34f}, {.98f, .38f, .49f}}} },
        { "Mono", {{{.70f, .73f, .79f}, {.94f, .95f, .98f}, {.80f, .84f, .91f}}} }
    }};
    inline constexpr int count = static_cast<int>(palettes.size());
    inline constexpr double cycle_seconds = 36.0;
    inline constexpr double transition_seconds = .65;

    inline float smooth(float value)
    {
        value = std::clamp(value, 0.f, 1.f);
        return value * value * value * (value * (value * 6.f - 15.f) + 10.f);
    }

    inline color mix(color a, color b, float t)
    {
        return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
    }

    inline color sample(int index, double phase)
    {
        index = std::clamp(index, 0, count - 1);
        phase = std::isfinite(phase) ? phase - std::floor(phase) : 0.0;
        const double scaled = phase * 3.0;
        const int first = std::min(2, static_cast<int>(scaled));
        return mix(palettes[index].stops[first], palettes[index].stops[(first + 1) % 3],
            smooth(static_cast<float>(scaled - first)));
    }

    class transition
    {
        std::array<float, palettes.size()> source{};
        int target = 0;
        double started = 0.0;
        double last_time = 0.0;
        bool initialized = false;

        std::array<float, palettes.size()> weights(double now) const
        {
            const float t = smooth(static_cast<float>((now - started) / transition_seconds));
            auto result = source;
            for (float& weight : result) weight *= 1.f - t;
            result[target] += t;
            return result;
        }

    public:
        color evaluate(int requested, double now, int offset = 0, bool reduce_motion = false)
        {
            requested = std::clamp(requested, 0, count - 1);
            if (!std::isfinite(now)) now = last_time;
            if (!initialized || now < last_time)
            {
                source = {};
                source[requested] = 1.f;
                target = requested;
                started = now;
                initialized = true;
            }
            if (requested != target)
            {
                // Preserve the currently displayed blend, even mid-transition.
                source = weights(now);
                target = requested;
                started = now;
            }
            last_time = now;
            if (reduce_motion)
            {
                source = {};
                source[target] = 1.f;
                return sample(target, static_cast<double>(offset) * .0035);
            }
            const double phase = std::fmod(now, cycle_seconds) / cycle_seconds + static_cast<double>(offset) * .0035;
            const auto blend = weights(now);
            color result{};
            for (int index = 0; index < count; ++index)
            {
                if (blend[index] == 0.f) continue;
                const color c = sample(index, phase);
                result.r += c.r * blend[index];
                result.g += c.g * blend[index];
                result.b += c.b * blend[index];
            }
            return result;
        }
    };
}
