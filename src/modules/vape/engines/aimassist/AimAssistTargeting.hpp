#pragma once
#include "../math/Vector3.hpp"
#include "../math/RotationUtil.hpp"
#include <algorithm>
#include <cmath>

namespace Vape::Combat {

    class AimAssistTargeting {
    public:
        enum class TargetArea {
            CENTER,
            CLOSEST
        };

        TargetArea targetArea{TargetArea::CLOSEST};

        void reset() {
            m_initialized = false;
            m_airFactor = 0.0f;
            m_lastGroundEyeY = 0.0;
            m_lastEyeY = 0.0;
            m_verticalVelocity = 0;
            m_rotation = {};
        }

        // Resolves 3D target coordinates including bounding box offset and airborne compensation
        Math::Vector3 resolveTargetPoint(const Math::Vector3& playerEyePos, const Math::BoundingBox& targetBox,
                                         bool isPlayerOnGround, double deltaTime) {
            double targetMinY = targetBox.min.y;
            double targetMaxY = targetBox.max.y;
            double targetHeight = targetMaxY - targetMinY;
            double targetCenterY = targetMinY + targetHeight * 0.65;

            deltaTime = std::clamp(deltaTime, .001, .12);
            const double measured = m_lastEyeY != 0 ? (playerEyePos.y-m_lastEyeY)/deltaTime : 0;
            m_verticalVelocity += (measured-m_verticalVelocity)*.65;
            m_lastEyeY = playerEyePos.y;
            // Recovered Java measures upward height/eye velocity, not distance
            // fallen below the last ground position.
            if (isPlayerOnGround) {
                m_lastGroundEyeY = playerEyePos.y;
                m_airFactor *= 0.35f;
            } else {
                if (m_lastGroundEyeY == 0.0) m_lastGroundEyeY = playerEyePos.y;
                const auto height = std::max(0.0,playerEyePos.y-m_lastGroundEyeY);
                m_airFactor = static_cast<float>(std::max(m_airFactor*.92,
                    std::max(std::min(.7,std::max(0.0,m_verticalVelocity)*.34),std::min(.82,height*.58))));
            }

            double airborneOffset = std::min(0.85, static_cast<double>(m_airFactor));
            double eyeOffset = 0.1 + airborneOffset;
            double eyeAlignedY = std::clamp(playerEyePos.y - eyeOffset, targetMinY + 0.01, targetMaxY - 0.01);
            double centerBlend = std::clamp(airborneOffset / 0.55, 0.0, 1.0);
            double targetY = eyeAlignedY + (targetCenterY - eyeAlignedY) * centerBlend;
            targetY = std::clamp(targetY, targetMinY + 0.01, targetMaxY - 0.01);

            if (targetArea == TargetArea::CLOSEST) {
                Math::Vector3 closest = targetBox.getClosestPoint(playerEyePos);
                double closestY = std::clamp(closest.y - eyeOffset, targetMinY + 0.01, targetMaxY - 0.01);
                closestY += (targetCenterY - closestY) * centerBlend;
                closestY = std::clamp(closestY, targetMinY + 0.01, targetMaxY - 0.01);
                return {closest.x, closestY, closest.z};
            }

            Math::Vector3 center = targetBox.getCenter();
            return {center.x, targetY, center.z};
        }

        // Computes dynamic predictive lead coordinates based on relative velocity
        Math::Vector3 calculatePredictiveAim(const Math::Vector3& resolvedTargetPos,
                                            const Math::Vector3& targetVelocity,
                                            const Math::Vector3& playerVelocity,
                                            double targetDistance, double deltaTime) {
            Math::Vector3 relativeMotion = targetVelocity - playerVelocity;
            Math::Vector3 targetLead = targetVelocity + Math::Vector3{
                relativeMotion.x * 0.08,
                relativeMotion.y * 0.1,
                relativeMotion.z * 0.08
            };

            if (!m_initialized) {
                m_aimPos = resolvedTargetPos;
                m_lead = targetLead;
                m_initialized = true;
            }

            double targetSpeed = std::sqrt(targetVelocity.x * targetVelocity.x + targetVelocity.z * targetVelocity.z);
            double relSpeed = std::sqrt(relativeMotion.x * relativeMotion.x + relativeMotion.z * relativeMotion.z);

            double baseSmoothing = targetArea == TargetArea::CLOSEST ? 0.3 : 0.28;
            double aimSmoothing = std::clamp(baseSmoothing + std::min(0.35, relSpeed * 0.5), 0.08, 0.75);
            double leadScale = targetSpeed + relSpeed * 0.25;
            double leadSmoothing = std::clamp(0.18 + std::min(0.42, leadScale * 0.72), 0.12, 0.68);

            double frameScale = std::clamp(deltaTime * 60.0, 0.45, 2.4);
            double aimBlend = 1.0 - std::pow(1.0 - aimSmoothing, frameScale);
            double leadBlend = 1.0 - std::pow(1.0 - leadSmoothing, frameScale);

            m_aimPos = m_aimPos + (resolvedTargetPos - m_aimPos) * aimBlend;
            m_lead = m_lead + (targetLead - m_lead) * leadBlend;

            const auto lag = resolvedTargetPos-m_aimPos;
            if (std::hypot(lag.x,lag.z) > .3 + targetSpeed*1.3 + relSpeed*.4) {
                m_aimPos=resolvedTargetPos;m_lead=targetLead;
            }

            double leadAmount = std::clamp(0.35 + targetDistance * 0.045 + targetSpeed * 0.95 + std::min(0.18, relSpeed * 0.12), 0.1, 1.05);
            double distScale = std::clamp((targetDistance - 0.8) / 2.5, 0.0, 1.0);
            leadAmount *= (0.3 + 0.7 * distScale);
            leadAmount = std::max(0.05, leadAmount);

            auto output=m_aimPos+m_lead*leadAmount;
            const double verticalLimit=.16+targetDistance*.055;
            output.y=m_aimPos.y+std::clamp(m_lead.y*std::min(leadAmount,.7+targetDistance*.04)*.8,-verticalLimit,verticalLimit);
            return output;
        }

        // Normal-speed adaptive controller recovered from the main Java
        // targeting submodule. It has its OWN rate/bias/history state; routing
        // this point back through Simple destroys the adaptive behavior.
        void updateRotation(const Math::Vector3& eye,const Math::Vector3& point,
                            float yaw,float pitch,float strafe,double distance,
                            double horizontalSpeed,double verticalSpeed,bool vertical,
                            bool strafeIncrease,double deltaTime) {
            auto& r=m_rotation;
            const float dt=static_cast<float>(std::clamp(deltaTime,.001,.12));
            auto target=Math::RotationUtil::getRotationsToCoords(eye,point);
            const float yawError=Math::RotationUtil::wrapDegrees(target.yaw-yaw);
            const float pitchError=vertical?target.pitch-pitch:0;
            const float absYaw=std::abs(yawError),absPitch=std::abs(pitchError);
            auto smooth=[](float a,float b,float v){const auto n=std::clamp((v-a)/(b-a),0.f,1.f);return n*n*(3-2*n);};
            auto lerp=[](float t,float a,float b){return a+t*(b-a);};
            float desired=1-smooth(1.5f,8.f,std::hypot(absYaw,absPitch));
            if(r.initialized)desired=std::clamp(desired+std::clamp(-(absYaw-std::abs(r.lastYawError))/dt/20.f,-.3f,.3f),0.f,1.f);
            r.strength+=(desired-r.strength)*(desired>r.strength?std::clamp(dt*3,.01f,.25f):std::clamp(dt*20,.05f,.8f));
            float targetYawRate=0,targetPitchRate=0,playerYawRate=0,playerPitchRate=0;
            if(r.initialized) {
                const float blend=std::clamp(dt*12,.05f,.45f);
                r.yawAccel+=(Math::RotationUtil::wrapDegrees(target.yaw-r.lastTargetYaw)/dt-r.yawAccel)*blend;
                r.pitchAccel+=((target.pitch-r.lastTargetPitch)/dt-r.pitchAccel)*blend;
                targetYawRate=r.yawAccel;targetPitchRate=r.pitchAccel;
                playerYawRate=Math::RotationUtil::wrapDegrees(yaw-r.lastPlayerYaw)/dt;
                playerPitchRate=(pitch-r.lastPlayerPitch)/dt;
            }
            const float hs=static_cast<float>(horizontalSpeed*.75),vs=static_cast<float>(verticalSpeed*.75);
            const float hf=std::clamp((hs-10)/90,0.f,1.f),vf=std::clamp((vs-10)/90,0.f,1.f);
            if(yawError*r.lastYawError<0)r.yawBias*=.3f;
            if(pitchError*r.lastPitchError<0)r.pitchBias*=.3f;
            r.yawBias+=yawError*dt*r.strength*r.strength*(1-hf);
            r.pitchBias+=pitchError*dt*r.strength*r.strength*(1-vf);
            const float retention=1-(1-r.strength)*std::clamp(dt*5,0.f,.5f);
            r.yawBias=std::clamp(r.yawBias*retention,-15*(1-hf*.9f),15*(1-hf*.9f));
            r.pitchBias=std::clamp(r.pitchBias*retention,-10*(1-vf*.9f),10*(1-vf*.9f));
            r.yawVelocity=r.yawVelocity*.85f+(r.initialized?(yawError-r.lastYawError)/dt:0)*.15f;
            r.pitchVelocity=r.pitchVelocity*.85f+(r.initialized?(pitchError-r.lastPitchError)/dt:0)*.15f;
            const float h=std::min(hs,10.f),v=std::min(vs,10.f);
            const float hg=.15f+.85f*std::clamp(std::pow((h-1)/9,2.f),0.f,1.f);
            const float vg=.15f+.85f*std::clamp(std::pow((v-1)/9,2.f),0.f,1.f);
            float yawRate=lerp(r.strength,8,2.5f+h*.15f)*hg*yawError+
                lerp(r.strength,.15f,.8f+h*.04f)*hg*r.yawBias+
                lerp(r.strength,.08f,.25f)*hg*r.yawVelocity+
                (.85f+h*.015f)*hg*targetYawRate-lerp(r.strength,.1f,.3f)*playerYawRate;
            float pitchRate=lerp(r.strength,7,2.2f+v*.13f)*vg*pitchError+
                lerp(r.strength,.12f,.65f+v*.035f)*vg*r.pitchBias+
                lerp(r.strength,.06f,.2f)*vg*r.pitchVelocity+
                (.82f+v*.013f)*vg*targetPitchRate-lerp(r.strength,.08f,.25f)*playerPitchRate;
            if(strafeIncrease&&((yawError>0&&strafe<0)||(yawError<=0&&strafe>0)))yawRate*=1.15f;
            const float wide=1+smooth(8,40,absYaw)*2.5f;
            const float yawLimit=std::min(400*wide,std::max((22+h*15)*hg*wide,(std::abs(targetYawRate)*.4f+18)*(.35f+hg*.65f)*wide));
            const float pitchLimit=std::min(300.f,std::max((18+v*13)*vg,(std::abs(targetPitchRate)*.38f+14)*(.35f+vg*.65f)));
            const float dist=smooth(.5f,3,static_cast<float>(distance));
            const float wideBase=lerp(smooth(8,40,absYaw),.15f,.65f);
            yawRate=std::clamp(yawRate,-yawLimit,yawLimit)*lerp(r.strength,wideBase+(1-wideBase)*dist,.4f+.6f*dist);
            pitchRate=std::clamp(pitchRate,-pitchLimit,pitchLimit)*lerp(r.strength,.2f+.8f*dist,.45f+.55f*dist);
            r.pendingYaw+=yawRate*dt;
            if(vertical)r.pendingPitch+=pitchRate*dt;
            else {r.pendingPitch=r.pitchBias=r.pitchVelocity=0;}
            r.lastYawError=yawError;r.lastPitchError=pitchError;
            r.lastTargetYaw=target.yaw;r.lastTargetPitch=target.pitch;
            r.lastPlayerYaw=yaw;r.lastPlayerPitch=pitch;r.initialized=true;
        }

        void applyRenderTick(float& yaw,float& pitch,float sensitivity) {
            auto& r=m_rotation;
            const float base=sensitivity*.6f+.2f;
            const float unit=base*base*base*8*.15f;
            const float dy=std::trunc(r.pendingYaw/unit)*unit;
            const float dp=std::trunc(r.pendingPitch/unit)*unit;
            yaw=Math::RotationUtil::wrapDegrees(yaw+dy);
            pitch=std::clamp(pitch+dp,-90.f,90.f);
            r.pendingYaw-=dy;r.pendingPitch-=dp;
        }

    private:
        bool m_initialized{false};
        float m_airFactor{0.0f};
        double m_lastGroundEyeY{0.0};
        double m_lastEyeY{0.0},m_verticalVelocity{0.0};
        struct RotationState {
            bool initialized=false;
            float strength=0,yawBias=0,pitchBias=0,yawVelocity=0,pitchVelocity=0,yawAccel=0,pitchAccel=0;
            float pendingYaw=0,pendingPitch=0,lastYawError=0,lastPitchError=0;
            float lastTargetYaw=0,lastTargetPitch=0,lastPlayerYaw=0,lastPlayerPitch=0;
        } m_rotation;
        Math::Vector3 m_aimPos{};
        Math::Vector3 m_lead{};
    };
}
