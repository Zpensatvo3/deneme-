#pragma once
#include "../../maths/maths.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace aim_regions
{
    inline constexpr int count = 60, maximum_selected = 10;
    struct rectangle { double left, top, right, bottom; };
    // The same six front-facing parts as the Player ESP preview.
    inline constexpr std::array<rectangle, 6> parts{{
        {100,101,160,161}, {94,165,166,261}, {70,165,94,261},
        {166,165,190,261}, {98,261,128,328}, {132,261,162,328}
    }};
    inline constexpr const char* names[]{"Head", "Body", "Right arm", "Left arm", "Right leg", "Left leg"};
    constexpr rectangle region(int id)
    {
        const auto part = parts[id / 10];
        const int column = id % 2, row = id % 10 / 2;
        const double width = (part.right - part.left) / 2.0, height = (part.bottom - part.top) / 5.0;
        return {part.left + column * width, part.top + row * height,
                part.left + (column + 1) * width, part.top + (row + 1) * height};
    }
    struct bounds { maths::vector3d minimum, maximum; };
    inline bool intersect(const maths::vector3d& origin, const maths::vector3d& direction,
        const bounds& box, double& ray_near, double& ray_far)
    {
        const double starts[]{origin.x, origin.y, origin.z}, steps[]{direction.x, direction.y, direction.z};
        const double lower[]{box.minimum.x, box.minimum.y, box.minimum.z};
        const double upper[]{box.maximum.x, box.maximum.y, box.maximum.z};
        for (int axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(starts[axis]) || !std::isfinite(steps[axis]) ||
                !std::isfinite(lower[axis]) || !std::isfinite(upper[axis]) || lower[axis] > upper[axis]) return false;
            if (std::abs(steps[axis]) < 1.e-12) {
                if (starts[axis] < lower[axis] || starts[axis] > upper[axis]) return false;
            } else {
                double first = (lower[axis] - starts[axis]) / steps[axis];
                double second = (upper[axis] - starts[axis]) / steps[axis];
                if (first > second) std::swap(first, second);
                ray_near = (std::max)(ray_near, first); ray_far = (std::min)(ray_far, second);
                if (ray_near > ray_far) return false;
            }
        }
        return true;
    }
    struct model
    {
        bounds hitbox{};
        maths::vector3d origin{}, side{}, front{};
        double width = 0, height = 0, depth = 0;
        bool initialize(const bounds& box, double yaw)
        {
            hitbox = box;
            const auto size = box.maximum - box.minimum;
            if (!std::isfinite(size.x) || !std::isfinite(size.y) || !std::isfinite(size.z) ||
                !std::isfinite(yaw) || size.x <= 0 || size.y <= 0 || size.z <= 0) return false;
            const double radians = std::remainder(yaw, 360.0) * std::numbers::pi / 180.0;
            side = {std::cos(radians), 0, std::sin(radians)};
            front = {-std::sin(radians), 0, std::cos(radians)};
            origin = {(box.minimum.x + box.maximum.x) * .5, box.minimum.y,
                      (box.minimum.z + box.maximum.z) * .5};
            width = (std::min)(size.x, size.z); height = size.y;
            depth = (std::abs(front.x) * size.x + std::abs(front.z) * size.z) * .5;
            return true;
        }
        maths::vector3d local(const maths::vector3d& vector) const
        { return {vector.x * side.x + vector.z * side.z, vector.y, vector.x * front.x + vector.z * front.z}; }
        bounds cell(int id) const
        {
            const auto rect = region(id);
            return {{(rect.left - 130.) / 120. * width, (328. - rect.bottom) / 227. * height, -depth},
                    {(rect.right - 130.) / 120. * width, (328. - rect.top) / 227. * height, depth}};
        }
        maths::vector3d center(int id) const
        {
            const auto box = cell(id);
            const double x = (box.minimum.x + box.maximum.x) * .5;
            return origin + side * x + maths::vector3d{0, (box.minimum.y + box.maximum.y) * .5, 0};
        }
        bool hit(int id, const maths::vector3d& eye, const maths::vector3d& direction,
            double reach, maths::vector3d& point) const
        {
            double ray_near = 0, ray_far = reach;
            if (!intersect(eye, direction, hitbox, ray_near, ray_far) ||
                !intersect(local(eye - origin), local(direction), cell(id), ray_near, ray_far)) return false;
            const double distance = ray_near > 1.e-6 ? ray_near : (std::min)(ray_far, 1.e-4);
            point = eye + direction * distance;
            return true;
        }
    };
}
