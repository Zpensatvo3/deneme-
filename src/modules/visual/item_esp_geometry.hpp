#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>

namespace item_esp_geometry
{
    struct point { double x, y, z; };
    inline bool finite(point p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
    struct rectangle { double left, top, right, bottom; };
    struct camera
    {
        point position{};
        double yaw = 0, pitch = 0, fov = 70, width = 0, height = 0, x = 0, y = 0;
    };

    // Clip crossing edges at the near plane. No distance cutoff or minimum
    // projected size: distant loaded items remain eligible for labels/boxes.
    inline bool project_box(const camera& c, point low, point high, rectangle& output)
    {
        if (!finite(c.position) || !finite(low) || !finite(high) ||
            !std::isfinite(c.yaw) || !std::isfinite(c.pitch) || !std::isfinite(c.fov) ||
            !std::isfinite(c.width) || !std::isfinite(c.height) || !std::isfinite(c.x) || !std::isfinite(c.y) ||
            c.width <= 0 || c.height <= 0 || c.fov <= 1 || c.fov >= 179 ||
            low.x > high.x || low.y > high.y || low.z > high.z) return false;
        constexpr double pi = 3.14159265358979323846, near_plane = .08;
        const double yaw = c.yaw * pi / 180., pitch = c.pitch * pi / 180.;
        const double sy = std::sin(yaw), cy = std::cos(yaw), sp = std::sin(pitch), cp = std::cos(pitch);
        const double focal = c.height / (2 * std::tan(c.fov * pi / 360.));
        std::array<point, 8> corners{};
        for (int i = 0; i < 8; ++i)
        {
            const double x = ((i & 4) ? high.x : low.x) - c.position.x;
            const double y = ((i & 2) ? high.y : low.y) - c.position.y;
            const double z = ((i & 1) ? high.z : low.z) - c.position.z;
            corners[i] = {-x * cy - z * sy, -x * sy * sp + y * cp + z * cy * sp,
                -x * sy * cp - y * sp + z * cy * cp};
        }
        double left = std::numeric_limits<double>::max(), top = left, right = -left, bottom = -left;
        bool projected = false;
        auto append = [&](point p)
        {
            const double x = c.width * .5 + p.x * focal / p.z;
            const double y = c.height * .5 - p.y * focal / p.z;
            if (!std::isfinite(x) || !std::isfinite(y)) return;
            left = std::min(left, x); right = std::max(right, x);
            top = std::min(top, y); bottom = std::max(bottom, y); projected = true;
        };
        for (point p : corners) if (p.z >= near_plane) append(p);
        for (int i = 0; i < 8; ++i) for (int bit : {1, 2, 4})
        {
            if (i & bit) continue;
            const point a = corners[i], b = corners[i | bit];
            if ((a.z < near_plane) == (b.z < near_plane)) continue;
            const double t = (near_plane - a.z) / (b.z - a.z);
            append({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, near_plane});
        }
        if (!projected || right < 0 || left > c.width || bottom < 0 || top > c.height) return false;
        // Keep a tiny, distant item's marker legible without hiding its label.
        if (right - left < 3) { const double mid = (right + left) * .5; left = mid - 1.5; right = mid + 1.5; }
        if (bottom - top < 3) { const double mid = (bottom + top) * .5; top = mid - 1.5; bottom = mid + 1.5; }
        output = {c.x + std::clamp(left, 0., c.width), c.y + std::clamp(top, 0., c.height),
            c.x + std::clamp(right, 0., c.width), c.y + std::clamp(bottom, 0., c.height)};
        return true;
    }

    inline std::string label(std::string_view name, int count, double distance,
        bool show_name, bool show_count, bool show_distance)
    {
        std::string result = show_name ? std::string(name) : std::string{};
        auto append = [&](const std::string& value) { if (!result.empty()) result += "  "; result += value; };
        if (show_count && count > 0) append("x" + std::to_string(count));
        if (show_distance && std::isfinite(distance) && distance >= 0)
        {
            char text[64]{};
            std::snprintf(text, sizeof(text), "%.1fm", distance);
            append(text);
        }
        return result;
    }
}
