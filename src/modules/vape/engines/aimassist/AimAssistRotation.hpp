#pragma once
#include "../math/RotationUtil.hpp"
#include "../math/Vector3.hpp"
#include <random>
#include <cmath>
#include <algorithm>

namespace Vape::Combat {

    /**
     * Exact C++20 port of Vape V4.21's AimAssistRotationSubModule ("Simple Mode").
     *
     * Complete Physics-Based Human Aim Engine:
     * - Target Velocity Predictor: 1.7x target velocity lead
     * - Proximity & Angle Force Dynamics:
     *     Accelerates faster at medium distance; dampens at < 0.5 blocks
     *     Dynamic boost when target changes direction quickly (> 6 deg/tick)
     * - Strafe Increase (1.6x multiplier):
     *     Detects when player is strafing away from target and increases force to prevent slipping
     * - Inversion & Overshoot Dampening:
     *     Inverts velocity buffer when crosshair crosses target center line
     *     0.7x damping within 5-degree deadzone
     * - Precision Near-Target Divisor:
     *     Scales down adjustment by (10.0 - angleDiff) within 9 degrees
     * - Organic Hand Drift Cycle:
     *     Every 250-300 ticks, introduces subtle resting position drift
     * - Exact Mouse Accumulator & Minecraft Sensitivity Quantizer:
     *     Mouse delta = steps * (sensitivity * 0.6 + 0.2)^3 * 8.0
     */
    class AimAssistRotation {
    public:
        double horizontalSpeed{5.0};
        double verticalSpeed{5.0};
        bool aimVertically{false};
        bool strafeIncrease{true};

        AimAssistRotation() : m_rng(std::random_device{}()) {}

        void reset() {
            m_horizontalMouseAccumulator = 0.0f;
            m_verticalMouseAccumulator = 0.0f;
            m_horizontalVelocity = 0.0f;
            m_verticalVelocity = 0.0f;
            m_horizontalVelocityBuffer = 0.0f;
            m_verticalVelocityBuffer = 0.0f;
            m_driftX = 0;
            m_driftY = 0;
            m_driftTimer = 0.0;
            m_prevTargetX = 0.0;
            m_prevTargetZ = 0.0;
            m_targetInitialized = false;
            m_swapTickCounter = 0;
            m_sampleCounter = 0;
            m_lastAngleDiff = 0;
            m_prevOnLeft = m_prevAbove = false;
            m_randomOffsetX = m_randomOffsetY = 0;
            m_pitchBoost = m_yawBoost = 0;
        }

        // Call on PreTick to calculate physics acceleration towards target
        void updateTick(const Math::Vector3& playerPos,
                        float playerYaw,
                        float playerPitch,
                        float playerStrafeInput, // -1.0 (left), 0, +1.0 (right)
                        const Math::Vector3& targetPos,
                        double targetDistance) {
            // The Java worker swaps the previous 11 samples BEFORE producing
            // the next sample. This solver is driven at 1 ms, not at 20 Hz.
            if (++m_swapTickCounter > 10) {
                m_verticalVelocityBuffer = m_verticalVelocity;
                m_horizontalVelocity = m_horizontalVelocityBuffer;
                m_horizontalVelocityBuffer = 0.0f;
                m_verticalVelocity = 0.0f;
                m_swapTickCounter = 0;
            }
            updateDrift();

            if (!m_targetInitialized) {
                m_prevTargetX = targetPos.x;
                m_prevTargetZ = targetPos.z;
                m_targetInitialized = true;
            }

            double targetMotionX = targetPos.x - m_prevTargetX;
            double targetMotionZ = targetPos.z - m_prevTargetZ;
            m_prevTargetX = targetPos.x;
            m_prevTargetZ = targetPos.z;

            // Target lead prediction (1.7 factor)
            constexpr double predictionFactor = 1.7;
            double predictedX = targetPos.x + targetMotionX * predictionFactor;
            double predictedZ = targetPos.z + targetMotionZ * predictionFactor;

            // Calculate horizontal angle difference and relative direction
            Math::Angle targetAngle = Math::RotationUtil::getRotationsToCoords(playerPos, {predictedX, targetPos.y, predictedZ});
            float rawYawDiff = Math::RotationUtil::wrapDegrees(targetAngle.yaw - playerYaw);
            double horizontalAngleDiff = std::abs(rawYawDiff);
            bool targetOnLeft = (rawYawDiff < 0.0f);

            // Vertical difference
            float rawPitchDiff = targetAngle.pitch - playerPitch;
            int verticalAngleDiff = static_cast<int>(std::abs(rawPitchDiff));
            bool targetAbove = (rawPitchDiff < 0.0f);
            int verticalDeadZone = verticalAngleDiff - 10;

            // Calculate dynamic forces
            std::uniform_real_distribution<double> rDist(0.0, 2.0);
            float horizontalForce = static_cast<float>(1.0 + rDist(m_rng) + horizontalAngleDiff / 50.0);
            float verticalForce = static_cast<float>(1.0 + rDist(m_rng) + std::abs(verticalDeadZone) / 50.0);

            // Quick direction change boost
            if (std::abs(horizontalAngleDiff - m_lastAngleDiff) > 6.0) {
                horizontalForce += static_cast<float>(horizontalAngleDiff / 35.0);
            }

            // Proximity acceleration
            double proximityBoost = std::max(0.0, (9.0 - targetDistance) / 2.5 - 2.0);
            horizontalForce += static_cast<float>(proximityBoost);

            // Strafe Increase: 1.6x multiplier when strafing away from target
            bool strafingAway = targetOnLeft ? (playerStrafeInput < 0.0f) : (playerStrafeInput > 0.0f);
            if (strafeIncrease && strafingAway) {
                horizontalForce *= 1.6f;
            }

            if (targetDistance < 0.5) {
                horizontalForce /= 5.0f;
            }

            // Calculate acceleration
            float horizontalAccel = (horizontalForce / 90.0f) * (targetOnLeft ? -1.0f : 1.0f);
            float verticalAccel = (verticalForce / 90.0f) * (targetAbove ? 1.0f : -1.0f);

            // Deadzone dampening (< 5 degrees)
            if (horizontalAngleDiff < 5.0) {
                horizontalAccel = 0.0f;
                m_horizontalVelocity *= 0.7f;
                bool strafingToward = targetOnLeft ? (playerStrafeInput > 0.0f) : (playerStrafeInput < 0.0f);
                if (strafingToward) {
                    m_horizontalVelocity *= 0.5f;
                }
            }

            // Target crossed centerline: invert velocity to prevent overshoot
            if (targetOnLeft != m_prevOnLeft) {
                m_horizontalVelocity = -m_horizontalVelocity;
                m_horizontalVelocityBuffer = -m_horizontalVelocityBuffer;
                m_horizontalMouseAccumulator = 0.0f;
            }
            if (targetAbove != m_prevAbove) {
                m_verticalVelocity = -m_verticalVelocity;
                m_verticalVelocityBuffer = -m_verticalVelocityBuffer;
                m_verticalMouseAccumulator = 0.0f;
            }

            if (verticalDeadZone < 5) {
                verticalAccel = 0.0f;
                m_verticalVelocityBuffer *= 0.7f;
            }

            m_horizontalVelocityBuffer += horizontalAccel;
            m_verticalVelocity += verticalAccel;

            float smoothedHVel = m_horizontalVelocity;
            float smoothedVVel = m_verticalVelocityBuffer;

            if (std::abs(smoothedHVel) > 10.0f) {
                m_horizontalVelocityBuffer = 0.0f;
                m_horizontalVelocity = 0.0f;
                return;
            }

            // Near-target fine divisor adjustment
            float hAdjustment = smoothedHVel * 0.15f;
            if (horizontalAngleDiff <= 9.0) {
                hAdjustment = static_cast<float>(hAdjustment / (10.0 - horizontalAngleDiff));
            }

            if (!std::isnan(hAdjustment)) {
                queueHorizontalAdjustment(hAdjustment, static_cast<float>(horizontalAngleDiff));
            }

            if (aimVertically) {
                float vAdjustment = smoothedVVel * 0.15f;
                if (!std::isnan(vAdjustment)) {
                    queueVerticalAdjustment(vAdjustment, static_cast<float>(verticalAngleDiff));
                }
            }

            m_prevOnLeft = targetOnLeft;
            m_prevAbove = targetAbove;

            if (++m_sampleCounter > 10) {
                m_lastAngleDiff = horizontalAngleDiff;
                m_sampleCounter = 0;
            }
        }

        // Call on PreRenderTick to apply mouse delta to camera
        void applyRenderTick(float& playerYaw, float& playerPitch, float mouseSensitivity) {
            m_horizontalMouseAccumulator += static_cast<float>(m_driftX);
            m_verticalMouseAccumulator += static_cast<float>(m_driftY);

            int hSteps = static_cast<int>(m_horizontalMouseAccumulator);
            int vSteps = static_cast<int>(m_verticalMouseAccumulator);

            float remH = m_horizontalMouseAccumulator - static_cast<float>(hSteps);
            float remV = m_verticalMouseAccumulator - static_cast<float>(vSteps);

            float sensBase = mouseSensitivity * 0.6f + 0.2f;
            float mouseScale = sensBase * sensBase * sensBase * 8.0f;

            // PlayerMouseRotationApplier multiplies raw mouse deltas by .15.
            // This port writes degrees directly, so it must include that step.
            float hDelta = static_cast<float>(hSteps) * mouseScale * .15f;
            float vDelta = static_cast<float>(vSteps) * mouseScale * .15f;

            playerYaw = Math::RotationUtil::wrapDegrees(playerYaw + hDelta);
            playerPitch = std::clamp(playerPitch - vDelta, -90.0f, 90.0f);

            m_horizontalMouseAccumulator = remH;
            m_verticalMouseAccumulator = remV;
            m_driftX = 0;
            m_driftY = 0;
        }

    private:
        void updateDrift() {
            m_driftTimer += 1.0;
            std::uniform_int_distribution<int> tDist(250, 299);
            if (m_driftTimer >= tDist(m_rng)) {
                std::uniform_int_distribution<int> dDist(-100, -50);
                std::uniform_int_distribution<int> oDist(-1, 1);
                m_driftTimer = dDist(m_rng);
                m_randomOffsetX = oDist(m_rng);
                m_randomOffsetY = oDist(m_rng);
            }

            int hStep = m_randomOffsetX;
            int vStep = m_randomOffsetY;

            if (m_driftTimer < 0.0) {
                hStep = 0;
                vStep = 0;
            }

            std::uniform_int_distribution<int> r20(0, 19);
            if (r20(m_rng) == 1) {
                m_driftX += hStep;
                m_driftY += vStep;
            }

            if ((m_horizontalMouseAccumulator > 0.0f && m_driftX < 0) ||
                (m_horizontalMouseAccumulator < 0.0f && m_driftX > 0)) {
                m_driftX = 0;
            }
        }

        void queueHorizontalAdjustment(float adjustment, float angleDiff) {
            if (adjustment != 0.0f) {
                adjustment *= 5.0f;
                float speed = static_cast<float>(horizontalSpeed);
                if (angleDiff <= 10.0f) {
                    m_pitchBoost = speed;
                }
                if (m_pitchBoost > 0.0f) {
                    speed -= m_pitchBoost / 3.0f;
                    m_pitchBoost -= angleDiff / 200.0f;
                }
                m_horizontalMouseAccumulator += speed * adjustment;
            } else {
                m_horizontalMouseAccumulator = 0.0f;
            }
        }

        void queueVerticalAdjustment(float adjustment, float angleDiff) {
            if (adjustment != 0.0f) {
                adjustment *= 5.0f;
                float speed = static_cast<float>(verticalSpeed);
                if (angleDiff <= 10.0f) {
                    m_yawBoost = speed;
                }
                if (m_yawBoost > 0.0f) {
                    speed -= m_yawBoost / 3.0f;
                    m_yawBoost -= angleDiff / 200.0f;
                }
                m_verticalMouseAccumulator += speed * adjustment;
            } else {
                m_verticalMouseAccumulator = 0.0f;
            }
        }

        float m_horizontalMouseAccumulator{0.0f};
        float m_verticalMouseAccumulator{0.0f};
        float m_horizontalVelocity{0.0f};
        float m_verticalVelocity{0.0f};
        float m_horizontalVelocityBuffer{0.0f};
        float m_verticalVelocityBuffer{0.0f};

        int m_swapTickCounter{0};
        int m_sampleCounter{0};
        double m_lastAngleDiff{0.0};

        bool m_prevOnLeft{false};
        bool m_prevAbove{false};

        double m_prevTargetX{0.0};
        double m_prevTargetZ{0.0};
        bool m_targetInitialized{false};

        double m_driftTimer{0.0};
        int m_randomOffsetX{0};
        int m_randomOffsetY{0};
        int m_driftX{0};
        int m_driftY{0};

        float m_pitchBoost{0.0f};
        float m_yawBoost{0.0f};

        std::mt19937 m_rng;
    };
}
