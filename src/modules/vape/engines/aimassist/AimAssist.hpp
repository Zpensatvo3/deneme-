#pragma once
#include "AimAssistRotation.hpp"
#include "AimAssistTargeting.hpp"
#include <vector>
#include <algorithm>
#include <memory>

namespace Vape::Combat {

    struct AimTargetCandidate {
        int entityId{0};
        Math::BoundingBox boundingBox{};
        Math::Vector3 velocity{};
        float health{20.0f};
        int armorValue{0};
        int equipmentTier{0};
        bool isFriend{false};
        bool isDead{false};
    };

    /**
     * Independent C++ AimAssist, with source-checked Simple/Adaptive dispatch.
     *
     * Complete Feature Set:
     * - Multi-Mode Target Sorting:
     *     YAW      (Closest to crosshair / FOV)
     *     DISTANCE (Closest in 3D distance)
     *     HEALTH   (Lowest health target)
     *     ARMOR    (Lowest armor value / threat)
     *     THREAT   (Highest equipment / weapon threat)
     * - Dual Engine Architecture:
     *     SIMPLE   (Vape's physics-based mouse accumulator with 1.7x lead and strafe boost)
     *     ADAPTIVE (Continuous relative velocity vector lead & airborne interpolation)
     * - Mining Protection (checkBlockBreak): Pauses aim assistance while mining
     * - Require Mouse Down: Only aims while holding attack button
     * - Weapons Only filter: Only aims while holding swords or axes
     * - Closest vs Center Area targeting
     */
    class AimAssist {
    public:
        enum class EngineMode {
            SIMPLE,
            ADAPTIVE
        };

        enum class TargetSortMode {
            YAW,
            DISTANCE,
            HEALTH,
            ARMOR,
            THREAT
        };

        bool enabled{true};
        EngineMode mode{EngineMode::SIMPLE};
        TargetSortMode targetSortMode{TargetSortMode::YAW};
        double distance{5.0};
        double maxAngle{180.0}; // Full FOV threshold
        bool requireMouseDown{true};
        bool weaponsOnly{true};
        bool checkBlockBreak{true}; // Prevents aiming while breaking blocks

        AimAssist() = default;

        [[nodiscard]] bool isValidTarget(const Math::Vector3& eye,float yaw,const AimTargetCandidate& target) const {
            if(!enabled||target.isDead||target.isFriend)return false;
            const auto point=target.boundingBox.getCenter();
            return eye.distanceTo(point)<=distance&&Math::RotationUtil::getAngleDifference(yaw,
                Math::RotationUtil::getRotationsToCoords(eye,point).yaw)<=maxAngle*.5;
        }

        // Evaluates candidates and picks the single best target according to selected sorting mode
        [[nodiscard]] const AimTargetCandidate* findBestTarget(const Math::Vector3& playerEyePos,
                                                               float playerYaw,
                                                               const std::vector<AimTargetCandidate>& candidates) const {
            if (!enabled) return nullptr;

            std::vector<const AimTargetCandidate*> validTargets;
            for (const auto& candidate : candidates) {
                if (!isValidTarget(playerEyePos,playerYaw,candidate)) continue;

                double dist = playerEyePos.distanceTo(candidate.boundingBox.getCenter());
                if (dist > distance) continue;

                Math::Angle targetAngle = Math::RotationUtil::getRotationsToCoords(playerEyePos, candidate.boundingBox.getCenter());
                float fov = Math::RotationUtil::getAngleDifference(playerYaw, targetAngle.yaw);
                if (fov > maxAngle / 2.0) continue;

                validTargets.push_back(&candidate);
            }

            if (validTargets.empty()) return nullptr;

            // Sort candidates according to targetSortMode
            std::sort(validTargets.begin(), validTargets.end(), [&](const AimTargetCandidate* a, const AimTargetCandidate* b) {
                switch (targetSortMode) {
                    case TargetSortMode::YAW: {
                        float fovA = Math::RotationUtil::getAngleDifference(playerYaw, Math::RotationUtil::getRotationsToCoords(playerEyePos, a->boundingBox.getCenter()).yaw);
                        float fovB = Math::RotationUtil::getAngleDifference(playerYaw, Math::RotationUtil::getRotationsToCoords(playerEyePos, b->boundingBox.getCenter()).yaw);
                        return fovA < fovB;
                    }
                    case TargetSortMode::DISTANCE:
                        return playerEyePos.distanceTo(a->boundingBox.getCenter()) < playerEyePos.distanceTo(b->boundingBox.getCenter());
                    case TargetSortMode::HEALTH:
                        return a->health < b->health;
                    case TargetSortMode::ARMOR:
                        return a->armorValue < b->armorValue;
                    case TargetSortMode::THREAT:
                        return a->equipmentTier > b->equipmentTier;
                }
                return false;
            });

            return validTargets.front();
        }

        // Call on PreTick
        void updateTick(const Math::Vector3& playerPos,
                        float playerYaw,
                        float playerPitch,
                        float playerStrafeInput,
                        bool isAttackKeyDown,
                        bool isHoldingWeapon,
                        bool isBreakingBlocks,
                        const AimTargetCandidate* target,
                        const Math::Vector3& playerVelocity = {},
                        bool isOnGround = true,
                        double deltaTime = 0.05) {
            if (!enabled || !target) {
                reset();
                return;
            }

            if (requireMouseDown && !isAttackKeyDown) {
                reset();
                return;
            }

            if (weaponsOnly && !isHoldingWeapon) {
                reset();
                return;
            }

            if (checkBlockBreak && isBreakingBlocks) {
                reset();
                return;
            }

            double dist = playerPos.distanceTo(target->boundingBox.getCenter());
            if (dist > distance) {
                reset();
                return;
            }

            Math::Vector3 targetCenter = m_adaptiveTargeting.targetArea==AimAssistTargeting::TargetArea::CLOSEST ?
                target->boundingBox.getClosestPoint(playerPos):target->boundingBox.getCenter();
            if (mode == EngineMode::ADAPTIVE) {
                const auto resolved = m_adaptiveTargeting.resolveTargetPoint(playerPos, target->boundingBox, isOnGround, deltaTime);
                targetCenter = m_adaptiveTargeting.calculatePredictiveAim(resolved, target->velocity, playerVelocity, dist, deltaTime);
                m_adaptiveTargeting.updateRotation(playerPos,targetCenter,playerYaw,playerPitch,playerStrafeInput,dist,
                    m_simpleRotation.horizontalSpeed,m_simpleRotation.verticalSpeed,m_simpleRotation.aimVertically,
                    m_simpleRotation.strafeIncrease,deltaTime);
            } else {
                m_simpleRotation.updateTick(playerPos, playerYaw, playerPitch, playerStrafeInput, targetCenter, dist);
            }
        }

        // Fixed 1 ms numeric solver on the client thread replaces Java's unsafe
        // background JNI worker. A frame hitch discards excess time instead of
        // replaying an unbounded backlog. Adaptive is a distinct time-based loop.
        void updateFrame(const Math::Vector3& eye,float yaw,float pitch,float strafe,
                         bool attackKey,bool weapon,bool mining,const AimTargetCandidate* target,
                         const Math::Vector3& motion,bool grounded,double elapsed) {
            if(!std::isfinite(elapsed)||elapsed<=0)return;
            if(!target){reset();return;}
            if(mode==EngineMode::ADAPTIVE) {
                updateTick(eye,yaw,pitch,strafe,attackKey,weapon,mining,target,motion,grounded,std::min(elapsed,.05));
                return;
            }
            m_solverTime=std::min(.05,m_solverTime+elapsed);
            const int steps=static_cast<int>((m_solverTime+1e-9)/.001);
            m_solverTime-=steps*.001;
            for(int i=0;i<steps;++i)
                updateTick(eye,yaw,pitch,strafe,attackKey,weapon,mining,target,motion,grounded,.001);
        }

        // Call on PreRenderTick (Frame) to apply smooth mouse delta to camera
        void applyRenderTick(float& playerYaw, float& playerPitch, float mouseSensitivity) {
            if (!enabled) return;
            if(mode==EngineMode::ADAPTIVE)m_adaptiveTargeting.applyRenderTick(playerYaw,playerPitch,mouseSensitivity);
            else m_simpleRotation.applyRenderTick(playerYaw, playerPitch, mouseSensitivity);
        }

        void reset() {
            m_simpleRotation.reset();
            m_adaptiveTargeting.reset();
            m_solverTime=0;
        }

        AimAssistRotation& getSimpleRotation() { return m_simpleRotation; }
        AimAssistTargeting& getAdaptiveTargeting() { return m_adaptiveTargeting; }

    private:
        AimAssistRotation m_simpleRotation;
        AimAssistTargeting m_adaptiveTargeting;
        double m_solverTime=0;
    };
}
