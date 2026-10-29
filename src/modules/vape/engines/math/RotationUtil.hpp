#pragma once
#include "Vector3.hpp"
#include <cmath>
#include <numbers>
#include <algorithm>

namespace Vape::Math {

    class RotationUtil {
    public:
        // Normalizes an angle into [-180, 180]
        static float wrapDegrees(float angle) {
            angle = std::fmod(angle, 360.0f);
            if (angle >= 180.0f) angle -= 360.0f;
            if (angle < -180.0f) angle += 360.0f;
            return angle;
        }

        static float getAngleDifference(float a, float b) {
            return std::abs(wrapDegrees(a - b));
        }

        // Calculates look yaw and pitch from camera eye to target position
        static Angle getRotationsToCoords(const Vector3& eyePos, const Vector3& target) {
            double dx = target.x - eyePos.x;
            double dy = target.y - eyePos.y;
            double dz = target.z - eyePos.z;
            double dist = std::sqrt(dx * dx + dz * dz);

            float yaw = static_cast<float>(std::atan2(dz, dx) * (180.0 / std::numbers::pi)) - 90.0f;
            float pitch = static_cast<float>(-(std::atan2(dy, dist) * (180.0 / std::numbers::pi)));

            return {wrapDegrees(yaw), std::clamp(pitch, -90.0f, 90.0f)};
        }

        // Minecraft's mouse sensitivity GCD quantizer (Anti-cheat bypass: prevents impossible angle deltas)
        static float applySensitivityGCD(float currentAngle, float targetAngle, float sensitivity) {
            float f = sensitivity * 0.6f + 0.2f;
            float gcd = f * f * f * 1.2f;
            float delta = targetAngle - currentAngle;
            delta = std::round(delta / gcd) * gcd;
            return currentAngle + delta;
        }
    };
}
