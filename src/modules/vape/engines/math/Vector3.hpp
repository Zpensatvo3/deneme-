#pragma once
#include <cmath>
#include <algorithm>

namespace Vape::Math {

    struct Vector3 {
        double x{0.0}, y{0.0}, z{0.0};

        constexpr Vector3() = default;
        constexpr Vector3(double x, double y, double z) : x(x), y(y), z(z) {}

        constexpr Vector3 operator+(const Vector3& o) const { return {x + o.x, y + o.y, z + o.z}; }
        constexpr Vector3 operator-(const Vector3& o) const { return {x - o.x, y - o.y, z - o.z}; }
        constexpr Vector3 operator*(double s) const { return {x * s, y * s, z * s}; }

        [[nodiscard]] double distanceTo(const Vector3& o) const {
            double dx = x - o.x;
            double dy = y - o.y;
            double dz = z - o.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }
    };

    struct BoundingBox {
        Vector3 min;
        Vector3 max;

        constexpr BoundingBox() = default;
        constexpr BoundingBox(const Vector3& min, const Vector3& max) : min(min), max(max) {}

        [[nodiscard]] Vector3 getCenter() const {
            return {(min.x + max.x) * 0.5, (min.y + max.y) * 0.5, (min.z + max.z) * 0.5};
        }

        [[nodiscard]] Vector3 getClosestPoint(const Vector3& eyePos) const {
            return {
                std::clamp(eyePos.x, min.x, max.x),
                std::clamp(eyePos.y, min.y, max.y),
                std::clamp(eyePos.z, min.z, max.z)
            };
        }
    };

    struct Angle {
        float yaw{0.0f};
        float pitch{0.0f};

        constexpr Angle() = default;
        constexpr Angle(float yaw, float pitch) : yaw(yaw), pitch(pitch) {}
    };
}
