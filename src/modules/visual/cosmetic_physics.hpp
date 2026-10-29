#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace cosmetic_physics {
    struct vector { double x=0,y=0,z=0; };
    inline vector operator+(vector a,vector b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
    inline vector operator-(vector a,vector b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
    inline vector operator*(vector a,double b) { return {a.x*b,a.y*b,a.z*b}; }
    inline double square(vector a) { return a.x*a.x+a.y*a.y+a.z*a.z; }
    inline bool finite(vector a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z); }
    struct sample { vector position; double time=0,yaw=0; int entity=-1,age=0; };
    struct signal {
        vector velocity{},acceleration{};
        double sideways=0,forward=0,vertical=0,turn=0,phase=0,accumulator=0;
        sample previous{};
        bool initialized=false;
        static constexpr double step=1./120.;
        void clear() { *this={}; }
        bool observe(sample next,bool animate,double speed=1) {
            if (!finite(next.position)||!std::isfinite(next.time)||!std::isfinite(next.yaw)) { clear(); return false; }
            const double elapsed=next.time-previous.time;
            const bool reset=!initialized || next.entity!=previous.entity || next.age<previous.age ||
                elapsed<0 || elapsed>.25 || square(next.position-previous.position)>16;
            if (reset) { clear(); initialized=true; previous=next; return true; }
            if (!animate) { clear(); initialized=true; previous=next; return true; }
            if (elapsed>1e-6) {
                const double dt=std::min(elapsed,.05),blend=1.-std::exp(-dt*12.);
                auto measured=(next.position-previous.position)*(1./elapsed);
                const auto bounded=[](double value) { return std::clamp(value,-16.,16.); };
                measured={bounded(measured.x),bounded(measured.y),bounded(measured.z)};
                const auto old=velocity; velocity=velocity+(measured-velocity)*blend;
                auto a=(velocity-old)*(1./elapsed);
                const auto clamp_acceleration=[](double value) { return std::clamp(value,-35.,35.); };
                acceleration={clamp_acceleration(a.x),clamp_acceleration(a.y),clamp_acceleration(a.z)};
                const double c=std::cos(next.yaw),s=std::sin(next.yaw);
                sideways=velocity.x*c+velocity.z*s;
                forward=-velocity.x*s+velocity.z*c; vertical=velocity.y;
                const double rotation=std::clamp(std::remainder(next.yaw-previous.yaw,6.283185307179586)/elapsed,-6.,6.);
                turn+=(rotation-turn)*blend;
                phase=std::fmod(phase+dt*(2.6*std::clamp(speed,.25,2.)+std::abs(forward)*.10),6.283185307179586);
                accumulator=std::min(accumulator+dt,6*step);
            }
            previous=next; return false;
        }
        int steps() {
            const int count=std::min(6,static_cast<int>((accumulator+1e-9)/step));
            accumulator=std::max(0.,accumulator-count*step); return count;
        }
    };
    struct spring {
        double position=0,velocity=0;
        void tick(double target,double dt,double stiffness=80,double damping=15) {
            velocity+=(stiffness*(target-position)-damping*velocity)*dt;
            position+=velocity*dt;
        }
    };
    struct wing_rig {
        std::array<spring,2> shoulder{},tip{};
        void reset() { for (auto& joint:shoulder) joint={.38,0}; for (auto& joint:tip) joint={.38,0}; }
        void advance(const signal& input,int steps,double strength) {
            strength=std::clamp(strength,0.,1.5);
            for (int n=0;n<steps;++n) for (int i=0;i<2;++i) {
                const double side=i ? 1. : -1.;
                const double flap=std::sin(input.phase-(steps-1-n)*signal::step*2.6)*.22;
                const double load=std::clamp(std::abs(input.forward)*.026-input.vertical*.022,0.,.24);
                const double twist=side*(input.turn*.024+input.sideways*.009)*strength;
                const double target=std::clamp(.38+flap+load*strength+twist,.04,1.05);
                shoulder[i].tick(target,signal::step,90,15);
                tip[i].tick(shoulder[i].position+std::sin(input.phase-.45)*.08*strength,signal::step,45,10);
                shoulder[i].position=std::clamp(shoulder[i].position,-.1,1.15);
                tip[i].position=std::clamp(tip[i].position,-.2,1.25);
            }
        }
    };
    struct cloth {
        static constexpr int columns=9,rows=13;
        std::array<vector,columns*rows> position{},previous{};
        double width=0,length=0;
        bool initialized=false;
        int index(int x,int y) const { return y*columns+x; }
        vector anchor(int x) const { return {(double(x)/(columns-1)-.5)*width,1.46,-.255}; }
        void reset(double w,double l) {
            width=std::clamp(w,.45,.95); length=std::clamp(l,.65,1.30); initialized=true;
            for (int y=0;y<rows;++y) for (int x=0;x<columns;++x) {
                const double v=double(y)/(rows-1); auto p=anchor(x); p.y-=length*v; p.z-=.10*v;
                position[index(x,y)]=previous[index(x,y)]=p;
            }
        }
        void constrain(int a,int b,double rest,bool fixed_a,bool fixed_b) {
            const auto delta=position[b]-position[a]; const double distance=std::sqrt(square(delta));
            if (distance<1e-9) return;
            const auto correction=delta*((distance-rest)/distance);
            if (fixed_a) position[b]=position[b]-correction;
            else if (fixed_b) position[a]=position[a]+correction;
            else { position[a]=position[a]+correction*.5; position[b]=position[b]-correction*.5; }
        }
        void advance(const signal& input,int steps,double strength) {
            strength=std::clamp(strength,0.,1.75);
            const double c=std::cos(input.previous.yaw),s=std::sin(input.previous.yaw);
            const double side_acceleration=input.acceleration.x*c+input.acceleration.z*s;
            const double forward_acceleration=-input.acceleration.x*s+input.acceleration.z*c;
            for (int n=0;n<steps;++n) {
                for (int y=1;y<rows;++y) for (int x=0;x<columns;++x) {
                    const int i=index(x,y); const double v=double(y)/(rows-1),u=double(x)/(columns-1);
                    const auto p=position[i]; const auto inertia=(p-previous[i])*.985;
                    const vector force{
                        (-input.sideways*.8-side_acceleration*.23-input.turn*1.1)*strength,
                        -9.8-input.acceleration.y*.10*strength,
                        (-input.forward*2.5-forward_acceleration*.20+std::sin(input.phase+v*5+u*3)*.35)*strength};
                    position[i]=p+inertia+force*(signal::step*signal::step); previous[i]=p;
                }
                const double dx=width/(columns-1),dy=length/(rows-1),diagonal=std::hypot(dx,dy);
                for (int iteration=0;iteration<6;++iteration) {
                    for (int x=0;x<columns;++x) position[index(x,0)]=anchor(x);
                    for (int y=0;y<rows;++y) for (int x=0;x<columns;++x) {
                        if (x+1<columns) constrain(index(x,y),index(x+1,y),dx,y==0,y==0);
                        if (y+1<rows) constrain(index(x,y),index(x,y+1),dy,y==0,false);
                        if (x+1<columns && y+1<rows) {
                            constrain(index(x,y),index(x+1,y+1),diagonal,y==0,false);
                            constrain(index(x+1,y),index(x,y+1),diagonal,y==0,false);
                        }
                    }
                    // Render-only torso/leg collision; no world/player writes.
                    for (int y=1;y<rows;++y) for (int x=0;x<columns;++x) {
                        auto& p=position[index(x,y)];
                        if (std::abs(p.x)<.34) p.z=std::min(p.z,p.y>.70 ? -.245 : -.20);
                        p.y=std::clamp(p.y,-.05,1.46);
                    }
                }
                for (int x=0;x<columns;++x) position[index(x,0)]=anchor(x);
            }
        }
        vector at(double u,double v) const {
            const double fx=std::clamp(u,0.,1.)*(columns-1),fy=std::clamp(v,0.,1.)*(rows-1);
            const int x=std::min(int(fx),columns-2),y=std::min(int(fy),rows-2);
            const double a=fx-x,b=fy-y;
            return (position[index(x,y)]*(1-a)+position[index(x+1,y)]*a)*(1-b)+
                (position[index(x,y+1)]*(1-a)+position[index(x+1,y+1)]*a)*b;
        }
    };
}
