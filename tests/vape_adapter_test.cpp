#include "../src/modules/vape/engines/aimassist/AimAssist.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>

// Small, offline decision check. No game process, injection, server or inputs.
int main()
{
    using namespace Vape;
    const Math::Vector3 eye{0,1.62,0};
    const Math::BoundingBox bounds{{1.7,0,2.7},{2.3,1.8,3.3}};
    Combat::AimAssist aim;
    aim.requireMouseDown=false;aim.weaponsOnly=false;
    const std::vector<Combat::AimTargetCandidate> targets{{7,bounds}};
    float yaw=0,pitch=0;
    const auto* selected=aim.findBestTarget(eye,yaw,targets);
    assert(selected&&selected->entityId==7);
    for(int frame=0;frame<120;++frame) {
        aim.updateFrame(eye,yaw,pitch,0,false,false,false,selected,{},true,1./120);
        aim.applyRenderTick(yaw,pitch,.5f);
    }
    assert(yaw < -1&&std::isfinite(yaw));
    aim.reset();yaw=0;
    aim.updateTick(eye,yaw,pitch,0,false,false,true,selected);
    aim.applyRenderTick(yaw,pitch,.5f);
    assert(yaw==0); // Actual mining still pauses the user's aim engine.
    aim.reset();yaw=0;
    aim.mode=Combat::AimAssist::EngineMode::ADAPTIVE;
    for(int frame=0;frame<120;++frame) {
        aim.updateFrame(eye,yaw,pitch,0,false,false,false,selected,{},true,1./120);
        aim.applyRenderTick(yaw,pitch,.5f);
    }
    assert(yaw < -1&&std::isfinite(yaw));
    aim.reset();yaw=0;
    aim.applyRenderTick(yaw,pitch,.5f);assert(yaw==0);

    std::cout << "Vape aim engine checks passed\n";
}
