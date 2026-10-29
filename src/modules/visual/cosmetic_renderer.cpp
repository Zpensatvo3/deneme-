#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "cosmetic_renderer.hpp"
#include "chams_model.hpp"
#include "visual_effects_logic.hpp"
#include "cape_design.hpp"
#include "cosmetic_physics.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/ui_state.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/client_context.hpp"
#include "../../services/game/overlay_context.hpp"
#include "../../services/game/target_service.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <initializer_list>
#include <span>
#include <vector>

namespace
{
    constexpr double pi = 3.14159265358979323846;
    struct point { double x = 0, y = 0, z = 0; };
    point operator+(point a, point b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
    point operator-(point a, point b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
    point operator*(point a, double s) { return {a.x*s, a.y*s, a.z*s}; }
    double dot(point a, point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
    point cross(point a, point b) { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
    bool finite(point p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
    point from(maths::vector3d p) { return {p.x,p.y,p.z}; }
    struct basis
    {
        point right, up, forward;
        point apply(point p) const { return right*p.x + up*p.y + forward*p.z; }
    };
    basis player_basis(double yaw, double pitch = 0)
    {
        yaw *= pi/180.; pitch *= pi/180.;
        const point right{std::cos(yaw),0,std::sin(yaw)}, forward{-std::sin(yaw),0,std::cos(yaw)};
        return {right, point{0,std::cos(pitch),0}+forward*std::sin(pitch),
            point{0,-std::sin(pitch),0}+forward*std::cos(pitch)};
    }
    struct plane
    {
        point normal;
        double offset;
        double distance(point p) const { return dot(normal,p)+offset; }
    };
    using polygon = std::vector<point>;
    using shadow = std::vector<plane>;
    struct triangle {
        std::array<point,3> vertices; ImVec4 color;
        ImTextureID texture=nullptr; std::array<ImVec2,3> uv{};
    };
    struct edge { point a,b; ImVec4 color; };
    struct mesh { std::vector<triangle> faces; std::vector<edge> edges; };
    struct scene
    {
        services::game::client_context context;
        point position, head_center, camera;
        basis body, head, view;
        bool crouching = false;
        double height = 1.8, focal = 1, tangent_x = 1, tangent_y = 1;
        ImVec2 origin, size;
        std::vector<shadow> occluders;
        point body_point(point p) const
        {
            // Crouching bends around the waist, with a small downward root shift.
            return position + point{0,.85-(crouching ? .125 : 0.),0} + body.apply(p-point{0,.85,0});
        }
        point to_view(point p) const
        {
            p = p-camera;
            return {dot(p,view.right),dot(p,view.up),dot(p,view.forward)};
        }
        ImVec2 project(point p) const
        {
            return {static_cast<float>(origin.x+size.x*.5+p.x*focal/p.z),
                static_cast<float>(origin.y+size.y*.5-p.y*focal/p.z)};
        }
    };

    polygon clip(const polygon& input, plane boundary, bool inside = true)
    {
        polygon output;
        if (input.empty()) return output;
        output.reserve(input.size()+1);
        point a = input.back();
        double da = boundary.distance(a)*(inside ? 1. : -1.);
        for (point b : input)
        {
            const double db = boundary.distance(b)*(inside ? 1. : -1.);
            if ((da >= 0) != (db >= 0)) output.push_back(a+(b-a)*(da/(da-db)));
            if (db >= 0) output.push_back(b);
            a=b; da=db;
        }
        return output;
    }

    struct texture_vertex { point position; ImVec2 uv; };
    using texture_polygon=std::vector<texture_vertex>;
    texture_polygon clip_texture(const texture_polygon& input,plane boundary,bool inside=true)
    {
        texture_polygon output; if (input.empty()) return output;
        output.reserve(input.size()+1); auto a=input.back();
        double da=boundary.distance(a.position)*(inside ? 1. : -1.);
        for (const auto& b:input) {
            const double db=boundary.distance(b.position)*(inside ? 1. : -1.);
            if ((da>=0)!=(db>=0)) {
                const double t=da/(da-db);
                output.push_back({a.position+(b.position-a.position)*t,
                    {float(a.uv.x+(b.uv.x-a.uv.x)*t),float(a.uv.y+(b.uv.y-a.uv.y)*t)}});
            }
            if (db>=0) output.push_back(b);
            a=b; da=db;
        }
        return output;
    }
    std::vector<texture_polygon> unoccluded_texture(texture_polygon input,const scene& s)
    {
        std::vector<texture_polygon> pieces{std::move(input)};
        for (const auto& volume:s.occluders) {
            std::vector<texture_polygon> next;
            for (auto& p:pieces) {
                bool separate=false;
                for (const auto& plane:volume)
                    if (std::all_of(p.begin(),p.end(),[&](const auto& v) { return plane.distance(v.position)<0; })) { separate=true; break; }
                if (separate) { next.push_back(std::move(p)); continue; }
                for (const auto& plane:volume) {
                    auto outside=clip_texture(p,plane,false);
                    if (outside.size()>=3) next.push_back(std::move(outside));
                    p=clip_texture(p,plane); if (p.size()<3) break;
                }
            }
            pieces=std::move(next); if (pieces.empty() || pieces.size()>256) break;
        }
        return pieces.size()>256 ? std::vector<texture_polygon>{} : pieces;
    }

    // A convex box casts a convex shadow from the camera. Subtracting these
    // volumes keeps cosmetics behind the character/solid blocks without altering GL state.
    void append_box(scene& s, point center, basis axes, point half)
    {
        std::array<point,8> corners;
        const point local[] = {{-half.x,-half.y,-half.z},{half.x,-half.y,-half.z},
            {half.x,half.y,-half.z},{-half.x,half.y,-half.z},{-half.x,-half.y,half.z},
            {half.x,-half.y,half.z},{half.x,half.y,half.z},{-half.x,half.y,half.z}};
        for (int i=0;i<8;++i) corners[i]=center+axes.apply(local[i]);
        constexpr int sides[6][4]{{0,3,2,1},{4,5,6,7},{0,1,5,4},{1,2,6,5},{3,7,6,2},{0,4,7,3}};
        std::array<bool,6> visible{};
        shadow volume;
        for (int i=0;i<6;++i)
        {
            const auto& side=sides[i];
            point normal=cross(corners[side[1]]-corners[side[0]],corners[side[2]]-corners[side[0]]);
            const double length=std::sqrt(dot(normal,normal));
            if (length<1e-9) return;
            normal=normal*(1./length);
            visible[i]=dot(normal,s.camera-corners[side[0]])>1e-7;
            if (visible[i]) volume.push_back({normal*-1.,dot(normal,corners[side[0]])});
        }
        if (volume.empty()) return;
        constexpr int edges[12][4]{{0,1,0,2},{1,2,0,3},{2,3,0,4},{3,0,0,5},
            {4,5,1,2},{5,6,1,3},{6,7,1,4},{7,4,1,5},
            {0,4,2,5},{1,5,2,3},{2,6,3,4},{3,7,4,5}};
        for (const auto& e:edges)
        {
            if (visible[e[2]]==visible[e[3]]) continue;
            point normal=cross(corners[e[0]]-s.camera,corners[e[1]]-s.camera);
            const double length=std::sqrt(dot(normal,normal));
            if (length<1e-9) continue;
            normal=normal*(1./length);
            if (dot(normal,center-s.camera)<0) normal=normal*-1.;
            volume.push_back({normal,-dot(normal,s.camera)});
        }
        s.occluders.push_back(std::move(volume));
    }

    std::vector<polygon> unoccluded(polygon input, const scene& s)
    {
        std::vector<polygon> pieces{std::move(input)};
        for (const auto& volume:s.occluders)
        {
            std::vector<polygon> next;
            for (auto& p:pieces)
            {
                bool separate=false;
                for (const auto& plane:volume)
                    if (std::all_of(p.begin(),p.end(),[&](point v) { return plane.distance(v)<0; })) { separate=true; break; }
                if (separate) { next.push_back(std::move(p)); continue; }
                for (const auto& plane:volume)
                {
                    auto outside=clip(p,plane,false);
                    if (outside.size()>=3) next.push_back(std::move(outside));
                    p=clip(p,plane);
                    if (p.size()<3) break;
                }
            }
            pieces=std::move(next);
            if (pieces.empty() || pieces.size()>256) break;
        }
        return pieces.size()>256 ? std::vector<polygon>{} : pieces;
    }

    std::array<plane,5> frustum(const scene& s)
    {
        return {{{{0,0,1},-.05},{{1,0,s.tangent_x},0},{{-1,0,s.tangent_x},0},
            {{0,1,s.tangent_y},0},{{0,-1,s.tangent_y},0}}};
    }
    bool clip_segment(point& a, point& b, const auto& planes)
    {
        for (const auto& plane:planes)
        {
            const double da=plane.distance(a), db=plane.distance(b);
            if (da<0 && db<0) return false;
            if ((da<0)!=(db<0))
            {
                const point intersection=a+(b-a)*(da/(da-db));
                if (da<0) a=intersection; else b=intersection;
            }
        }
        return true;
    }

    std::vector<std::pair<point,point>> unoccluded_edge(point a, point b, const scene& s)
    {
        std::vector<std::pair<point,point>> pieces{{a,b}};
        for (const auto& volume:s.occluders)
        {
            std::vector<std::pair<point,point>> next;
            for (const auto& [start,end]:pieces)
            {
                double enter=0, leave=1;
                bool intersects=true;
                for (const auto& plane:volume)
                {
                    const double da=plane.distance(start), db=plane.distance(end);
                    if (da<0 && db<0) { intersects=false; break; }
                    if ((da<0)!=(db<0))
                    {
                        const double t=da/(da-db);
                        if (da<0) enter=std::max(enter,t); else leave=std::min(leave,t);
                    }
                    if (enter>leave) { intersects=false; break; }
                }
                if (!intersects) next.push_back({start,end});
                else
                {
                    if (enter>1e-5) next.push_back({start,start+(end-start)*enter});
                    if (leave<1.-1e-5) next.push_back({start+(end-start)*leave,end});
                }
            }
            pieces=std::move(next);
            if (pieces.empty() || pieces.size()>128) break;
        }
        return pieces.size()>128 ? std::vector<std::pair<point,point>>{} : pieces;
    }

    bool capture(scene& s,bool only_third_person=true,bool cosmetic_pose=true)
    {
        using platform::jni_context::clear_exception;
        if (gui::draw || !gui::runtime_ready.load(std::memory_order_acquire) || gui::hud_editor_active || gui::should_unload ||
            !services::game::capture_client_context(s.context,true,false) || s.context.screen) return false;
        auto renderer=s.context.instance.gameRenderer.get();
        if (clear_exception() || !renderer) return false;
        auto camera=renderer.getCamera.call();
        if (clear_exception() || !camera) return false;
        const bool third_person=camera.isThirdPerson.call()==JNI_TRUE;
        if (clear_exception() || (only_third_person && !third_person)) return false;
        const auto& player=s.context.player;
        if (!player.isAlive.call() || player.isSpectator.call() || clear_exception()) return false;
        if (cosmetic_pose && (player.isInvisible.call() || player.isSleeping.call() ||
            player.isFallFlying.call() || player.isSwimming.call() || player.hasVehicle.call() || clear_exception())) return false;
        auto& adapter=platform::adapter::get();
        const float delta=std::clamp(adapter.get_tick_delta(s.context.instance),0.f,1.f);
        if (!std::isfinite(delta) || clear_exception()) return false;
        const auto data=adapter.get_camera_data(s.context.instance,player,delta);
        if (clear_exception() || !data.valid || !finite(from(data.pos)) || !std::isfinite(data.yaw) ||
            !std::isfinite(data.pitch) || !std::isfinite(data.fov) || data.fov<=1 || data.fov>=179) return false;
        auto position=player.getLerpedPos.call(delta);
        if (clear_exception() || !position) return false;
        s.position=from(position.to_vector3d());
        auto box=player.boundingBox.get();
        if (clear_exception() || !box || !finite(s.position)) return false;
        s.height=box.maxY.get()-box.minY.get();
        chams_model::Living pose(player.object_instance,false);
        const float body_previous=pose.prevBodyYaw.get(), body_current=pose.bodyYaw.get();
        const float head_previous=pose.prevHeadYaw.get(), head_current=pose.headYaw.get();
        const float pitch_previous=player.prevPitch.get(), pitch_current=player.pitch.get();
        s.crouching=pose.isInSneakingPose.call()==JNI_TRUE;
        if (clear_exception() || !std::isfinite(s.height) || s.height<1 || s.height>3 ||
            !std::isfinite(body_previous) || !std::isfinite(body_current) || !std::isfinite(head_previous) ||
            !std::isfinite(head_current) || !std::isfinite(pitch_previous) || !std::isfinite(pitch_current)) return false;
        const double body=body_previous+std::remainder(body_current-body_previous,360.f)*delta;
        const double head=head_previous+std::remainder(head_current-head_previous,360.f)*delta;
        s.body=player_basis(body,s.crouching ? 24. : 0.);
        s.head=player_basis(head,pitch_previous+(pitch_current-pitch_previous)*delta);
        s.head_center=s.position+point{0,s.height-.25,0};
        s.camera=from(data.pos);
        const double yaw=data.yaw*pi/180., pitch=data.pitch*pi/180.;
        s.view={{-std::cos(yaw),0,-std::sin(yaw)},
            {-std::sin(yaw)*std::sin(pitch),std::cos(pitch),std::cos(yaw)*std::sin(pitch)},
            {-std::sin(yaw)*std::cos(pitch),-std::sin(pitch),std::cos(yaw)*std::cos(pitch)}};
        const auto* viewport=ImGui::GetMainViewport();
        s.origin=viewport->Pos; s.size=viewport->Size;
        if (s.size.x<=0 || s.size.y<=0) return false;
        s.tangent_y=std::tan(data.fov*pi/360.); s.tangent_x=s.tangent_y*s.size.x/s.size.y;
        s.focal=s.size.y/(2*s.tangent_y);
        if (third_person)
        {
        append_box(s,s.head_center,s.head,{.27,.27,.27});
        append_box(s,s.body_point({0,1.05,0}),s.body,{.25,.36,.15});
        for (int side:{-1,1})
        {
            append_box(s,s.body_point({side*.375,1.05,0}),s.body,{.125,.35,.15});
            append_box(s,s.position+player_basis(body).apply({side*.13,.36,0}),player_basis(body),{.12,.36,.13});
        }
        }
        return true;
    }

    // Bound the read cost to the cosmetic's nearby blocks; all JNI references
    // stay on the render thread and inside the caller's local frame.
    bool block_shadows(scene& s, const mesh& m)
    {
        point low=s.camera, high=s.camera;
        for (const auto& f:m.faces) for (const auto& p:f.vertices)
        {
            low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
            high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};
        }
        if (!finite(low) || !finite(high)) return false;
        if (std::abs(low.x)>30000064 || std::abs(high.x)>30000064 || std::abs(low.z)>30000064 ||
            std::abs(high.z)>30000064 || std::abs(low.y)>30000064 || std::abs(high.y)>30000064) return false;
        if (high.x-low.x>16 || high.y-low.y>16 || high.z-low.z>16) return false;
        const int min_x=static_cast<int>(std::floor(low.x)), max_x=static_cast<int>(std::floor(high.x));
        const int min_y=static_cast<int>(std::floor(low.y)), max_y=static_cast<int>(std::floor(high.y));
        const int min_z=static_cast<int>(std::floor(low.z)), max_z=static_cast<int>(std::floor(high.z));
        if ((max_x-min_x+1)*(max_y-min_y+1)*(max_z-min_z+1)>512) return false;
        const basis axes{{1,0,0},{0,1,0},{0,0,1}};
        for (int x=min_x;x<=max_x;++x) for (int y=min_y;y<=max_y;++y) for (int z=min_z;z<=max_z;++z)
        {
            jni::frame block_frame{16};
            auto position=maps::BlockPos::new_object(&maps::BlockPos::constructor,
                static_cast<jint>(x),static_cast<jint>(y),static_cast<jint>(z));
            if (platform::jni_context::clear_exception() || !position) return false;
            auto block=s.context.world.getBlockState.call(position);
            if (platform::jni_context::clear_exception() || !block) return false;
            const bool opaque=block.isOpaque.call()==JNI_TRUE;
            if (platform::jni_context::clear_exception()) return false;
            if (opaque) append_box(s,{x+.5,y+.5,z+.5},axes,{.5,.5,.5});
        }
        return true;
    }

    ImVec4 tint(bool theme, const float* custom, float opacity, int offset=0)
    {
        ImVec4 c=theme ? gui::get_dynamic_color(offset).Value : ImVec4(custom[0],custom[1],custom[2],custom[3]);
        c.w*=opacity;
        const auto component=[](float value) { return std::isfinite(value) ? std::clamp(value,0.f,1.f) : 0.f; };
        c.x=component(c.x); c.y=component(c.y); c.z=component(c.z); c.w=component(c.w);
        return c;
    }
    void render(const scene& s, const mesh& m, bool glow)
    {
        struct face {
            std::vector<ImVec2> vertices; ImU32 color; double depth;
            ImTextureID texture=nullptr; std::vector<ImVec2> uv;
        };
        struct line { ImVec2 a,b; ImVec4 color; };
        std::vector<face> faces;
        std::vector<line> lines;
        faces.reserve(m.faces.size()*2); lines.reserve(m.edges.size()*2);
        const auto planes=frustum(s);
        for (const auto& triangle:m.faces) {
            if (triangle.texture) {
                texture_polygon source; source.reserve(3);
                for (int i=0;i<3;++i) source.push_back({triangle.vertices[i],triangle.uv[i]});
                for (auto polygon:unoccluded_texture(std::move(source),s)) {
                    for (auto& p:polygon) p.position=s.to_view(p.position);
                    for (const auto& plane:planes) polygon=clip_texture(polygon,plane);
                    if (polygon.size()<3) continue;
                    face f{{},ImGui::ColorConvertFloat4ToU32(triangle.color),0,triangle.texture,{}};
                    f.vertices.reserve(polygon.size()); f.uv.reserve(polygon.size());
                    for (const auto& p:polygon) { f.vertices.push_back(s.project(p.position)); f.uv.push_back(p.uv); f.depth+=p.position.z; }
                    f.depth/=polygon.size(); faces.push_back(std::move(f));
                }
                continue;
            }
            for (auto polygon:unoccluded({triangle.vertices.begin(),triangle.vertices.end()},s))
            {
                for (auto& p:polygon) p=s.to_view(p);
                for (const auto& plane:planes) polygon=clip(polygon,plane);
                if (polygon.size()<3) continue;
                face f{{},ImGui::ColorConvertFloat4ToU32(triangle.color),0};
                for (const auto& p:polygon) { f.vertices.push_back(s.project(p)); f.depth+=p.z; }
                f.depth/=polygon.size();
                double area=0;
                for (std::size_t i=0;i<f.vertices.size();++i)
                {
                    const auto a=f.vertices[i], b=f.vertices[(i+1)%f.vertices.size()];
                    area+=static_cast<double>(a.x)*b.y-static_cast<double>(b.x)*a.y;
                }
                if (std::abs(area)<.01) continue;
                if (area<0) std::reverse(f.vertices.begin(),f.vertices.end());
                faces.push_back(std::move(f));
            }
        }
        for (const auto& edge:m.edges)
            for (auto [a,b]:unoccluded_edge(edge.a,edge.b,s))
            {
                a=s.to_view(a); b=s.to_view(b);
                if (clip_segment(a,b,planes)) lines.push_back({s.project(a),s.project(b),edge.color});
            }
        std::sort(faces.begin(),faces.end(),[](const auto& a,const auto& b) { return a.depth>b.depth; });
        auto* draw=ImGui::GetBackgroundDrawList();
        draw->PushClipRect(s.origin,ImVec2(s.origin.x+s.size.x,s.origin.y+s.size.y),true);
        const auto image_face=[&](const face& f,ImVec2 offset,ImU32 color) {
            draw->PushTextureID(f.texture);
            const int count=static_cast<int>(f.vertices.size()-2)*3; draw->PrimReserve(count,count);
            for (std::size_t i=1;i+1<f.vertices.size();++i) for (const auto index:{std::size_t(0),i,i+1}) {
                const auto p=f.vertices[index]; draw->PrimVtx({p.x+offset.x,p.y+offset.y},f.uv[index],color);
            }
            draw->PopTextureID();
        };
        if (glow) for (int layer=2;layer>0;--layer) for (const auto& f:faces) if (f.texture) {
            auto c=ImGui::ColorConvertU32ToFloat4(f.color); c.w*=.045f;
            const auto color=ImGui::ColorConvertFloat4ToU32(c); const float distance=layer*1.5f;
            for (const auto offset:{ImVec2(-distance,0),ImVec2(distance,0),ImVec2(0,-distance),ImVec2(0,distance)}) image_face(f,offset,color);
        }
        if (glow) for (int layer=3;layer>0;--layer) for (const auto& line:lines)
        {
            ImVec4 c=line.color; c.w*=.06f;
            draw->AddLine(line.a,line.b,ImGui::ColorConvertFloat4ToU32(c),1.f+layer*1.5f);
        }
        for (const auto& face:faces) {
            if (face.texture) image_face(face,{0,0},face.color);
            else draw->AddConvexPolyFilled(face.vertices.data(),static_cast<int>(face.vertices.size()),face.color);
        }
        for (const auto& line:lines) draw->AddLine(line.a,line.b,ImGui::ColorConvertFloat4ToU32(line.color),1.25f);
        draw->PopClipRect();
    }

    template<class Generate>
    bool draw_cosmetic(Generate generate, bool glow)
    {
        if (!platform::jni_context::bind_current_thread()) return false;
        struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } cleanup;
        jni::frame frame{128};
        try
        {
            scene s;
            if (!capture(s)) return false;
            mesh m; generate(s,m);
            if (m.faces.empty() || !block_shadows(s,m)) return false;
            render(s,m,glow);
            return true;
        }
        catch (...) { platform::jni_context::clear_exception(); }
        return false;
    }

    // Triangulate in the wing's local plane before folding it into world space.
    // Scalloped outlines and feather notches cannot use a fan from the shoulder.
    template<class Transform>
    void wing_panel(mesh& m, polygon outline, const Transform& transform, ImVec4 color)
    {
        if (outline.size()<3) return;
        const auto turn=[](point a,point b,point c) {
            return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
        };
        double area=0;
        for (std::size_t i=0;i<outline.size();++i)
        {
            const auto a=outline[i],b=outline[(i+1)%outline.size()];
            area+=a.x*b.y-b.x*a.y;
        }
        if (std::abs(area)<1e-9) return;
        if (area<0) std::reverse(outline.begin(),outline.end());
        polygon remaining=outline;
        std::vector<triangle> faces;
        while (remaining.size()>3)
        {
            bool found=false;
            for (std::size_t i=0;i<remaining.size();++i)
            {
                const auto prev=(i+remaining.size()-1)%remaining.size(),next=(i+1)%remaining.size();
                const point a=remaining[prev],b=remaining[i],c=remaining[next];
                if (turn(a,b,c)<=1e-10) continue;
                bool occupied=false;
                for (std::size_t j=0;j<remaining.size();++j)
                {
                    if (j==prev || j==i || j==next) continue;
                    const auto p=remaining[j];
                    if (turn(a,b,p)>=-1e-10 && turn(b,c,p)>=-1e-10 && turn(c,a,p)>=-1e-10)
                    { occupied=true; break; }
                }
                if (occupied) continue;
                faces.push_back({{transform(a),transform(b),transform(c)},color});
                remaining.erase(remaining.begin()+i);
                found=true; break;
            }
            if (!found) return;
        }
        faces.push_back({{transform(remaining[0]),transform(remaining[1]),transform(remaining[2])},color});
        m.faces.insert(m.faces.end(),faces.begin(),faces.end());
        color.w=std::min(1.f,color.w*1.8f);
        for (std::size_t i=0;i<outline.size();++i)
            m.edges.push_back({transform(outline[i]),transform(outline[(i+1)%outline.size()]),color});
    }

    void curve_to(polygon& path,point control,point end,int steps=6)
    {
        const point start=path.back();
        for (int i=1;i<=steps;++i)
        {
            const double t=static_cast<double>(i)/steps;
            path.push_back(start*((1-t)*(1-t))+control*(2*(1-t)*t)+end*(t*t));
        }
    }

    struct trail_ray { bool blocked=false,top=false; point position; };
    bool cast_trail_ray(const scene& s,point a,point b,trail_ray& result,bool ground=false)
    {
        using platform::jni_context::clear_exception;
        jni::frame frame{24};
        result={};
        auto start=maps::Vec3d::new_object(&maps::Vec3d::constructor,a.x,a.y,a.z);
        if (clear_exception() || !start) return false;
        auto end=maps::Vec3d::new_object(&maps::Vec3d::constructor,b.x,b.y,b.z);
        if (clear_exception() || !end) return false;
        auto shape=maps::RaycastContext$ShapeType{}.COLLIDER.get();
        if (clear_exception() || !shape) return false;
        auto fluid=maps::RaycastContext$FluidHandling{}.NONE.get();
        if (clear_exception() || !fluid) return false;
        auto context=maps::RaycastContext::new_object(&maps::RaycastContext::constructor,start,end,shape,fluid,
            maps::Entity(s.context.player.object_instance,false));
        if (clear_exception() || !context) return false;
        auto hit=s.context.world.raycast.call(context);
        if (clear_exception() || !hit) return false;
        maps::HitResult view(hit.object_instance,false);
        auto type=view.getType.call();
        if (clear_exception() || !type) return false;
        auto block=maps::HitResult$Type{}.BLOCK.get();
        if (clear_exception() || !block) return false;
        result.blocked=type.is_same_object(block);
        if (clear_exception()) return false;
        if (result.blocked && ground)
        {
            auto pos=view.getPos.call();
            if (clear_exception() || !pos) return false;
            result.position=from(pos.to_vector3d());
            if (clear_exception() || !finite(result.position)) return false;
            auto side=hit.getSide.call();
            if (clear_exception() || !side) return false;
            auto up=maps::Direction{}.UP.get();
            if (clear_exception() || !up) return false;
            result.top=side.is_same_object(up);
            if (clear_exception()) return false;
        }
        return true;
    }

    struct trail_particle
    {
        point origin,velocity;
        ImVec4 color;
        double born=0,radius=.025,next_visibility=0;
        bool visible=false;
        point position(double age) const { return origin+velocity*age+point{0,-.045*age*age,0}; }
    };
    struct footprint
    {
        point position;
        ImVec4 color;
        double born=0,yaw=0,next_visibility=0;
        bool visible=false;
    };
}

struct cosmetics::trail_state::runtime
{
    services::game::overlay_session session;
    std::deque<trail_particle> particles;
    std::deque<footprint> feet;
    point previous;
    double time=0,particle_distance=0,foot_distance=0;
    bool initialized=false,left=false;
    int mode=-1,age=0;
    std::size_t visibility_cursor=0;
    std::uint32_t random=0x58a9c12du;
    double noise()
    {
        random^=random<<13; random^=random>>17; random^=random<<5;
        return static_cast<double>(random)/4294967295.;
    }
    void clear_motion()
    {
        particles.clear(); feet.clear(); initialized=left=false;
        particle_distance=foot_distance=time=0; visibility_cursor=0;
    }
};
cosmetics::trail_state::trail_state() : state_(std::make_unique<runtime>()) {}
cosmetics::trail_state::~trail_state() = default;
void cosmetics::trail_state::clear() { state_->clear_motion(); state_->session.reset(); }

void cosmetics::draw_hat(const module_settings::china_hat& config)
{
    draw_cosmetic([&](const scene& s,mesh& m)
    {
        if (!std::isfinite(config.radius) || !std::isfinite(config.height) || !std::isfinite(config.offset)) return;
        const double radius=std::clamp(config.radius,.35f,1.f), height=std::clamp(config.height,.15f,.65f);
        const point root=s.head_center+s.head.up*(.28+std::clamp(config.offset,0.f,.25f));
        constexpr int segments=64;
        // A revolved profile gives each hat its own silhouette while keeping the
        // same head attachment, width/height controls and occlusion handling.
        const auto roof=[&](std::initializer_list<point> profile,int color_offset)
        {
            const auto rings=std::span<const point>(profile.begin(),profile.size());
            const auto vertex=[&](point ring,double angle) {
                return root+s.head.apply({std::cos(angle)*radius*ring.x,height*ring.y,
                    std::sin(angle)*radius*ring.x});
            };
            for (std::size_t band=1;band<rings.size();++band) for (int i=0;i<segments;++i)
            {
                const double a=i*2*pi/segments,b=(i+1)*2*pi/segments;
                const point p=vertex(rings[band-1],a),q=vertex(rings[band-1],b);
                const point r=vertex(rings[band],a),t=vertex(rings[band],b);
                ImVec4 c=tint(config.theme_color,config.color,config.opacity,color_offset+i*3);
                const float shade=.78f+.22f*static_cast<float>(.5+.5*std::cos(a));
                c.x*=shade; c.y*=shade; c.z*=shade;
                if (rings[band-1].x>0) m.faces.push_back({{p,r,q},c});
                m.faces.push_back({{q,r,t},c});
                c.w=std::min(1.f,c.w*1.8f);
                if (band+1==rings.size()) m.edges.push_back({r,t,c});
                if (i%8==0) { c.w*=.35f; m.edges.push_back({p,r,c}); }
            }
        };
        switch (config.style)
        {
        case 1: // Low crown, curved brim and a lifted outer edge.
            roof({{0,1,0},{.28,.84,0},{.78,.08,0},{1,.16,0}},0);
            break;
        case 2: // Two overlapping roofs with separately outlined eaves.
            roof({{0,.70,0},{.56,.23,0},{1,0,0}},28);
            roof({{0,1,0},{.25,.79,0},{.67,.55,0}},0);
            break;
        default:
            roof({{0,1,0},{1,0,0}},0);
            break;
        }
    },config.glow);
}

struct cosmetics::cape_motion::runtime
{
    services::game::overlay_session session;
    cosmetic_physics::signal signal;
    cosmetic_physics::wing_rig wings;
    cosmetic_physics::cloth cloth;
    bool observe(const scene& s,bool animate,double speed=1)
    {
        const int entity=s.context.player.getId.call(),age=s.context.player.age.get();
        if (platform::jni_context::clear_exception()) return false;
        if (session.observe({s.context.world,s.context.player})) { signal.clear(); cloth.initialized=false; wings.reset(); }
        const bool reset=signal.observe({{s.position.x,s.position.y,s.position.z},ImGui::GetTime(),
            std::atan2(s.body.right.z,s.body.right.x),entity,age},animate,speed);
        if (reset) { cloth.initialized=false; wings.reset(); }
        return signal.initialized;
    }
};
cosmetics::cape_motion::cape_motion() : state_(std::make_unique<runtime>()) {}
cosmetics::cape_motion::~cape_motion() = default;
void cosmetics::cape_motion::clear()
{
    state_->signal.clear(); state_->cloth.initialized=false; state_->wings.reset();
    if (jni::get_env()) state_->session.reset(); else state_->session.invalidate();
}

void cosmetics::draw_wings(const module_settings::wings& config,cape_motion& motion)
{
    const bool rendered=draw_cosmetic([&](const scene& s,mesh& m)
    {
        if (!std::isfinite(config.size) || !std::isfinite(config.speed) || !std::isfinite(config.physics)) return;
        const double size=std::clamp(config.size,.5f,1.6f);
        const bool animate=config.animate && !gui::ui_state::get_preferences().reduce_motion;
        auto& rig=*motion.state_;
        if (!rig.observe(s,animate,std::clamp(config.speed,.25f,2.f))) return;
        rig.wings.advance(rig.signal,rig.signal.steps(),config.physics);
        const auto asset=config.style==1 ? gui::icon_asset::wing_angel : config.style==2 ? gui::icon_asset::wing_butterfly : gui::icon_asset::wing_dragon;
        const auto image=config.detailed_art ? gui::get_icon_texture(asset) : nullptr;
        m.faces.reserve(image ? 576 : 384);
        for (int side:{-1,1})
        {
            const int joint=side<0 ? 0 : 1;
            const double angle=animate ? rig.wings.shoulder[joint].position : .38;
            const double flex=animate ? (rig.wings.tip[joint].position-angle)*std::clamp(config.physics,0.f,1.5f) : 0.;
            const auto transform=[&](point p)
            {
                const double span=std::clamp(p.x/1.45,0.,1.),fold=angle+flex*span*span;
                return s.body_point({side*(.11+p.x*size*std::cos(fold)),
                    1.25+p.y*size,-.19-p.x*size*std::sin(fold)+p.z*size});
            };
            const auto color=[&](int offset=0) { return tint(config.theme_color,config.color,config.opacity,offset); };
            const auto rib=[&](point a,point b,int offset=0,float alpha=1.1f) {
                auto c=color(offset); c.w=std::min(1.f,c.w*alpha);
                m.edges.push_back({transform(a),transform(b),c});
            };
            if (image) {
                constexpr int columns=12,rows=12;
                const double root=config.style==0 ? .64 : config.style==1 ? .49 : .50;
                const auto vertex=[&](float u,float v) { return transform({u*1.45,(root-v)*1.75,0}); };
                const auto ink=config.tint_art ? color() : ImVec4(1,1,1,config.opacity);
                for (int y=0;y<rows;++y) for (int x=0;x<columns;++x) {
                    const float u=float(x)/columns,v=float(y)/rows,a=float(x+1)/columns,b=float(y+1)/rows;
                    const auto p=vertex(u,v),q=vertex(a,v),r=vertex(u,b),t=vertex(a,b);
                    m.faces.push_back({{p,r,q},ink,image,{{{u,v},{u,b},{a,v}}}});
                    m.faces.push_back({{q,r,t},ink,image,{{{a,v},{u,b},{a,b}}}});
                }
                continue;
            }
            if (config.style==1)
            {
                // Long, curved feathers fan down from a rounded shoulder cover.
                for (int i=0;i<8;++i)
                {
                    const double t=i/7.;
                    const point base{.20+.39*t,.32-.54*t,-.012};
                    const point tip{1.27-.44*t,.58-1.43*t,-.012};
                    const point direction=tip-base;
                    const double length=std::sqrt(dot(direction,direction));
                    const point normal{-direction.y/length,direction.x/length,0};
                    const point bend=base+direction*.56+normal*.075;
                    polygon feather{base+normal*.045};
                    curve_to(feather,bend+normal*.105,tip,5);
                    curve_to(feather,bend-normal*.105,base-normal*.045,5);
                    wing_panel(m,std::move(feather),transform,color(i*17));
                    rib(base,tip,i*17,.55f);
                }
                polygon cover{{0,.14,0}};
                curve_to(cover,{.42,.62,0},{.88,.48,0});
                curve_to(cover,{.66,.10,0},{.40,-.40,0});
                curve_to(cover,{.12,-.40,0},{0,-.28,0},4);
                wing_panel(m,std::move(cover),transform,color(10));
            }
            else if (config.style==2)
            {
                // Separate rounded upper/lower lobes, with scallops and inlaid
                // eye-shaped markings that follow the wing rather than the camera.
                polygon upper{{0,.06,0}};
                curve_to(upper,{.22,.80,0},{.87,.91,0});
                curve_to(upper,{1.48,.93,0},{1.24,.39,0});
                curve_to(upper,{1.13,.44,0},{1.06,.21,0},4);
                curve_to(upper,{.94,.31,0},{.85,.09,0},4);
                curve_to(upper,{.70,.19,0},{.57,-.06,0},4);
                curve_to(upper,{.22,-.13,0},{0,-.12,0},4);
                wing_panel(m,std::move(upper),transform,color(0));
                polygon lower{{0,-.14,0}};
                curve_to(lower,{.68,.10,0},{.98,-.24,0});
                curve_to(lower,{1.02,-.56,0},{.59,-.79,0});
                curve_to(lower,{.47,-.58,0},{.34,-.65,0},4);
                curve_to(lower,{.11,-.54,0},{0,-.28,0},4);
                wing_panel(m,std::move(lower),transform,color(85));
                for (const point end: {point{.87,.91,0},point{1.24,.39,0},point{.85,.09,0},point{.59,-.79,0}})
                    rib({.02,-.08,0},end,25,.7f);
                const auto marking=[&](point center,double rx,double ry,int offset) {
                    polygon ring;
                    for (int i=0;i<16;++i)
                    {
                        const double a=i*2*pi/16;
                        ring.push_back(center+point{std::cos(a)*rx,std::sin(a)*ry,0});
                    }
                    auto c=color(offset); c.x*=.55f; c.y*=.55f; c.z*=.55f; c.w*=.8f;
                    wing_panel(m,std::move(ring),transform,c);
                };
                marking({.83,.52,-.008},.16,.22,45);
                marking({.55,-.42,-.008},.12,.15,100);
            }
            else
            {
                const polygon outline{{0,.14,0},{.36,.45,0},{1.12,.68,0},{.83,.20,0},
                    {1.14,.10,0},{.73,-.05,0},{.93,-.45,0},{.48,-.28,0},{.48,-.72,0},{0,-.30,0}};
                wing_panel(m,outline,transform,color());
                for (int i:{2,4,6,8}) rib({0,0,0},outline[i],i*14,1.35f);
            }
        }
    },config.glow);
    if (!rendered) motion.clear();
}

void cosmetics::draw_cape(const module_settings::cape& config,cape_motion& motion)
{
    const bool rendered=draw_cosmetic([&](const scene& s,mesh& m)
    {
        if (!std::isfinite(config.length) || !std::isfinite(config.width) ||
            !std::isfinite(config.motion) || !std::isfinite(config.physics)) return;
        const double length=std::clamp(config.length,.65f,1.30f),width=std::clamp(config.width,.45f,.95f);
        const bool animate=config.animate && config.physics>0 && !gui::ui_state::get_preferences().reduce_motion;
        auto& rig=*motion.state_;
        if (!rig.observe(s,animate)) return;
        if (!rig.cloth.initialized || std::abs(rig.cloth.width-width)>1e-5 || std::abs(rig.cloth.length-length)>1e-5)
            rig.cloth.reset(width,length);
        if (animate) rig.cloth.advance(rig.signal,rig.signal.steps(),config.motion*config.physics);
        const auto image=config.style==8 ? gui::get_icon_texture(gui::icon_asset::cape_phoenix) :
            config.style==9 ? gui::get_icon_texture(gui::icon_asset::cape_astral) : nullptr;
        const auto surface=[&](double u,double v)
        {
            const auto p=rig.cloth.at(u,v);
            return s.body_point({p.x,p.y,p.z+.004*std::cos(u*6*pi)*v*(1-v)});
        };
        constexpr int columns=12,rows=14;
        const auto u_at=[](int i) { return i==0 ? 0. : i==columns ? 1. : .055+(i-1)*.89/(columns-2); };
        const auto v_at=[](int i) { return i==0 ? 0. : i==rows ? 1. : .045+(i-1)*.91/(rows-2); };
        std::array<std::array<point,columns+1>,rows+1> cloth;
        for (int y=0;y<=rows;++y) for (int x=0;x<=columns;++x)
            cloth[y][x]=surface(u_at(x),v_at(y));
        m.faces.reserve(columns*rows*2);
        for (int y=0;y<rows;++y) for (int x=0;x<columns;++x)
        {
            const double u=(u_at(x)+u_at(x+1))*.5,v=(v_at(y)+v_at(y+1))*.5;
            auto c=image && !config.tint_art ? ImVec4(1,1,1,config.opacity) :
                tint(config.theme_color,config.color,config.opacity,static_cast<int>(v*100+u*20));
            const bool border=(config.style==1 || config.style==2 || config.style==3 || config.style==6) &&
                (x==0 || x==columns-1 || y==0 || y==rows-1);
            const auto pattern=cape_design::sample(config.style,u,v);
            const float shade=static_cast<float>(image ? .94+.06*std::cos((u*2-1)*3*pi+v*.65) :
                .80+.12*std::cos((u*2-1)*3*pi+v*.65)-.10*v);
            c.x*=shade*pattern.shade; c.y*=shade*pattern.shade; c.z*=shade*pattern.shade;
            if (pattern.ember>0) {
                const float blend=pattern.ember*.85f;
                c.x=c.x*(1-blend)+blend;
                c.y=c.y*(1-blend)+blend*(.12f+pattern.ember*.55f);
                c.z=c.z*(1-blend)+blend*.035f;
            }
            if (border)
            {
                c.x=c.x*.45f+.50f; c.y=c.y*.45f+.50f; c.z=c.z*.45f+.50f;
            }
            const auto a=cloth[y][x],b=cloth[y][x+1],d=cloth[y+1][x],e=cloth[y+1][x+1];
            const float u0=float(u_at(x)),u1=float(u_at(x+1)),v0=float(v_at(y)),v1=float(v_at(y+1));
            m.faces.push_back({{a,d,b},c,image,{{{u0,v0},{u0,v1},{u1,v0}}}});
            m.faces.push_back({{b,d,e},c,image,{{{u1,v0},{u0,v1},{u1,v1}}}});
        }
        auto edge_color=tint(config.theme_color,config.color,config.opacity);
        if (image && !config.tint_art) edge_color=config.style==8 ? ImVec4(.96f,.63f,.21f,config.opacity) : ImVec4(.64f,.79f,1.f,config.opacity);
        edge_color.w=std::min(1.f,edge_color.w*1.15f);
        for (int y=0;y<rows;++y)
        {
            m.edges.push_back({cloth[y][0],cloth[y+1][0],edge_color});
            m.edges.push_back({cloth[y][columns],cloth[y+1][columns],edge_color});
        }
        for (int x=0;x<columns;++x)
        {
            m.edges.push_back({cloth[0][x],cloth[0][x+1],edge_color});
            m.edges.push_back({cloth[rows][x],cloth[rows][x+1],edge_color});
        }
        const auto design=cape_design::strokes(config.style);
        if (!design.empty() || config.style==4)
        {
            // The emblem is traced on the same deformed cloth, so it bends with
            // the fabric instead of floating as a flat, camera-facing overlay.
            auto emblem=edge_color;
            emblem.x=emblem.x*.25f+.72f; emblem.y=emblem.y*.25f+.72f; emblem.z=emblem.z*.25f+.72f;
            const auto stroke=[&](cape_design::uv a,cape_design::uv b)
            {
                constexpr int steps=10;
                point previous=surface(a.x,a.y);
                for (int i=1;i<=steps;++i)
                {
                    const double t=static_cast<double>(i)/steps;
                    const point next=surface(a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t);
                    m.edges.push_back({previous,next,emblem}); previous=next;
                }
            };
            for (const auto& line:design) stroke(line.a,line.b);
            if (config.style==4) {
                emblem={1.f,.48f,.12f,edge_color.w};
                // Smooth silhouette over the low-cost cloth shading.
                for (int i=0;i<24;++i) {
                    const auto flame=[](double u) {
                        return cape_design::uv{static_cast<float>(u),
                            static_cast<float>(.45+.22*std::sin(u*19.)+.12*std::cos(u*31.))};
                    };
                    stroke(flame(.06+i*.88/24),flame(.06+(i+1)*.88/24));
                }
            }
        }
    },config.glow);
    if (!rendered) motion.clear();
}

void cosmetics::draw_trails(const module_settings::trails& config,trail_state& storage)
{
    if (!platform::jni_context::bind_current_thread()) return;
    struct cleanup { ~cleanup() { platform::jni_context::clear_exception(); } } clear_on_exit;
    jni::frame frame{128};
    auto& state=*storage.state_;
    try
    {
        scene s;
        if (!capture(s,false)) { storage.clear(); return; }
        if (!std::isfinite(config.lifetime) || !std::isfinite(config.density) || !std::isfinite(config.size))
        { storage.clear(); return; }
        const services::game::overlay_identity identity(s.context.world,s.context.player);
        if (state.session.observe(identity) || state.mode!=config.mode) state.clear_motion();
        state.mode=config.mode;
        const int age=static_cast<int>(s.context.player.age.get());
        if (platform::jni_context::clear_exception()) { storage.clear(); return; }
        const double now=ImGui::GetTime();
        const point delta=s.position-state.previous;
        const double elapsed=now-state.time;
        if (!state.initialized || age<state.age || elapsed<0 || elapsed>.25 || dot(delta,delta)>9.)
        {
            state.clear_motion(); state.initialized=true; state.previous=s.position;
            state.time=now; state.age=age;
        }
        else
        {
            const double distance=std::hypot(delta.x,delta.z);
            const bool grounded=s.context.player.isOnGround.call()==JNI_TRUE;
            if (platform::jni_context::clear_exception()) { storage.clear(); return; }
            const bool water=s.context.player.isTouchingWater.call()==JNI_TRUE;
            if (platform::jni_context::clear_exception()) { storage.clear(); return; }
            const bool lava=s.context.player.isInLava.call()==JNI_TRUE;
            if (platform::jni_context::clear_exception()) { storage.clear(); return; }
            const bool reduced=gui::ui_state::get_preferences().reduce_motion;
            const bool particles=config.mode!=1 && !water && !lava;
            const bool feet=config.mode!=0 && grounded && !water && !lava;
            const auto emission=[&](double& carry,double spacing,const auto& emit)
            {
                if (distance<1e-5) return;
                const double first=spacing-carry;
                int emitted=0;
                for (double d=first;d<=distance && emitted<8;d+=spacing,++emitted)
                    emit(state.previous+delta*(d/distance));
                carry=std::fmod(carry+distance,spacing);
            };
            if (particles) emission(state.particle_distance,
                (reduced ? .32 : .14)/std::clamp(config.density,.5f,2.f),[&](point at)
            {
                trail_particle particle;
                particle.origin=at+point{(state.noise()-.5)*.24,.12+state.noise()*.24,(state.noise()-.5)*.24};
                particle.velocity=reduced ? point{} : point{(state.noise()-.5)*.12,.12+state.noise()*.18,(state.noise()-.5)*.12};
                particle.radius=.025*(.7+state.noise()*.6);
                particle.color=tint(config.theme_color,config.color,1.f,static_cast<int>(now*65));
                particle.born=now; state.particles.push_back(particle);
                if (state.particles.size()>96) state.particles.pop_front();
            });
            else state.particle_distance=0;
            if (feet) emission(state.foot_distance,.46,[&](point at)
            {
                const double sign=state.left ? -1. : 1.;
                at=at+s.body.right*(sign*.11);
                trail_ray ground;
                if (!cast_trail_ray(s,at+point{0,.3,0},at-point{0,1.2,0},ground,true) || !ground.blocked || !ground.top) return;
                footprint foot;
                foot.position=ground.position+point{0,.015,0};
                foot.yaw=std::atan2(s.body.right.z,s.body.right.x)+sign*.09;
                foot.color=tint(config.theme_color,config.color,1.f,static_cast<int>(now*65));
                foot.born=now; state.feet.push_back(foot); state.left=!state.left;
                if (state.feet.size()>48) state.feet.pop_front();
            });
            else state.foot_distance=0;
            state.previous=s.position; state.time=now; state.age=age;
        }
        const double lifetime=std::clamp(config.lifetime,.5f,5.f);
        const double particle_lifetime=std::min(lifetime,2.5);
        std::erase_if(state.particles,[&](const auto& p) { return now-p.born>=particle_lifetime; });
        std::erase_if(state.feet,[&](const auto& p) { return now-p.born>=lifetime; });

        // Rotate a bounded ray budget across both lists. Full collision shapes
        // (including slabs/stairs) decide visibility; no depth-buffer guessing.
        const auto count=state.feet.size()+state.particles.size();
        int budget=12;
        for (std::size_t scanned=0;scanned<count && budget>0;++scanned)
        {
            const auto index=state.visibility_cursor++%count;
            auto check=[&](auto& sample,point position)
            {
                if (sample.next_visibility>now) return;
                --budget;
                trail_ray ray;
                sample.visible=cast_trail_ray(s,s.camera,position,ray) && !ray.blocked;
                sample.next_visibility=now+.20;
            };
            if (index<state.feet.size()) check(state.feet[index],state.feet[index].position);
            else
            {
                auto& p=state.particles[index-state.feet.size()];
                check(p,p.position(now-p.born));
            }
        }
        mesh m;
        const double size=std::clamp(config.size,.5f,1.5f);
        const auto quad=[&](point a,point b,point c,point d,ImVec4 color)
        {
            m.faces.push_back({{a,b,c},color}); m.faces.push_back({{a,c,d},color});
            for (const auto& e: {std::pair{a,b},std::pair{b,c},std::pair{c,d},std::pair{d,a}})
                m.edges.push_back({e.first,e.second,color});
        };
        for (const auto& foot:state.feet)
        {
            if (!foot.visible || dot(foot.position-s.camera,foot.position-s.camera)>1600) continue;
            const double age=std::clamp((now-foot.born)/lifetime,0.,1.);
            auto c=foot.color; c.w*=config.opacity*static_cast<float>((1-age)*(1-age));
            const basis axes=player_basis(foot.yaw*180./pi);
            const auto vertex=[&](double x,double z) { return foot.position+axes.apply({x*size,0,z*size}); };
            quad(vertex(-.060,-.035),vertex(.060,-.035),vertex(.048,.13),vertex(-.048,.13),c);
            quad(vertex(-.045,-.14),vertex(.045,-.14),vertex(.050,-.065),vertex(-.050,-.065),c);
        }
        for (const auto& p:state.particles)
        {
            if (!p.visible) continue;
            const double age=now-p.born;
            const point center=p.position(age);
            if (dot(center-s.camera,center-s.camera)>1600) continue;
            const double t=std::clamp(age/particle_lifetime,0.,1.);
            auto c=p.color; c.w*=config.opacity*static_cast<float>((1-t)*(1-t));
            const double radius=p.radius*size*(1.-t*.35);
            quad(center+s.view.up*radius,center+s.view.right*radius,
                center-s.view.up*radius,center-s.view.right*radius,c);
        }
        if (!m.faces.empty()) render(s,m,config.glow);
    }
    catch (...) { platform::jni_context::clear_exception(); storage.clear(); }
}

struct cosmetics::visual_effect_state::runtime
{
    services::game::overlay_session session;
    std::deque<trail_particle> particles;
    std::deque<footprint> circles;
    std::vector<std::array<point,2>> boxes;
    maps::LivingEntity target{nullptr,true};
    visual_effects_logic::hit_detector hits;
    visual_effects_logic::jump_detector jumps;
    point previous{}, block{};
    double time = -1, next_shape = 0, target_seen = -1;
    int age = -1;
    bool block_valid = false;
    std::uint32_t random = 0x91bd235fu;
    std::size_t visibility_cursor = 0;
    double noise() { random ^= random<<13; random ^= random>>17; random ^= random<<5; return random/4294967295.; }
    void reset()
    {
        particles.clear(); circles.clear(); boxes.clear(); target.clear_ref();
        hits.reset(); jumps.reset(); previous = {}; time = -1; age = -1;
        next_shape = 0; target_seen = -1; block_valid = false; visibility_cursor = 0;
    }
    bool observe(const scene& s, double now)
    {
        const int current_age = s.context.player.age.get();
        if (platform::jni_context::clear_exception()) return false;
        if (session.observe({s.context.world,s.context.player}) || now < time || now-time > .5 ||
            current_age < age || (time >= 0 && dot(s.position-previous,s.position-previous) > 9.)) reset();
        age = current_age; return true;
    }
    void advance(const scene& s, double now) { previous = s.position; time = now; }
};
cosmetics::visual_effect_state::visual_effect_state() : state_(std::make_unique<runtime>()) {}
cosmetics::visual_effect_state::~visual_effect_state() = default;
void cosmetics::visual_effect_state::clear() { state_->reset(); state_->session.reset(); }

namespace
{
    void effect_ring(mesh& m, point center, point right, point up, double radius, ImVec4 color, int segments = 32)
    {
        for (int i=0;i<segments;++i) {
            const double a=2*pi*i/segments,b=2*pi*(i+1)/segments;
            m.edges.push_back({center+(right*std::cos(a)+up*std::sin(a))*radius,
                center+(right*std::cos(b)+up*std::sin(b))*radius,color});
        }
    }
    bool effect_visible(const scene& s, point p)
    {
        trail_ray ray;
        return cast_trail_ray(s,s.camera,p,ray) && !ray.blocked;
    }
    struct effect_cleanup { ~effect_cleanup() { platform::jni_context::clear_exception(); } };
}

void cosmetics::draw_hit_particles(const module_settings::hit_particles& config, visual_effect_state& storage)
{
    if (!platform::jni_context::bind_current_thread()) return;
    effect_cleanup cleanup; jni::frame frame{128};
    auto& state=*storage.state_;
    try {
        scene s; const double now=ImGui::GetTime();
        if (!capture(s,false,false) || !state.observe(s,now)) { storage.clear(); return; }
        const int ticks=s.context.player.lastAttackedTicks.get();
        if (platform::jni_context::clear_exception()) { storage.clear(); return; }
        // Arm BEFORE refreshing the crosshair. Knockback can move the attacked
        // entity out of the crosshair on the very frame the cooldown resets.
        state.hits.observe_attack(ticks,now);
        // Briefly retain the same target during acknowledgement; a quick
        // crosshair exit must not turn this into particles on another entity.
        if (!state.hits.pending(now)) {
            // A cosmetic follows vanilla's living target, not combat filters
            // (friends/AntiBot must not suppress actual visible hit effects).
            auto target=services::target::crosshair_entity(s.context.instance);
            if (platform::jni_context::clear_exception()) { storage.clear(); return; }
            const bool valid=target && target.is_instance_of<maps::LivingEntity>() &&
                !target.is_same_object(s.context.player) && target.isAlive.call()==JNI_TRUE;
            if (platform::jni_context::clear_exception()) { storage.clear(); return; }
            if (valid) {
                state.target_seen=now;
                if (!state.target.is_same_object(target)) state.target=maps::LivingEntity(target.object_instance,false);
            } else if (now-state.target_seen>.12) state.target.clear_ref();
        }
        if (state.target) {
            auto position=state.target.getLerpedPos.call(std::clamp(platform::adapter::get().get_tick_delta(s.context.instance),0.f,1.f));
            const int id=state.target.getId.call(),hurt=state.target.hurtTime.get();
            const float health=state.target.getHealth.call();
            const double height=state.target.getHeight.call();
            if (platform::jni_context::clear_exception() || !position) { storage.clear(); return; }
            const point center=from(position.to_vector3d())+point{0,height*.6,0};
            if (platform::jni_context::clear_exception() || !finite(center) || !std::isfinite(height) || height<=0) { storage.clear(); return; }
            const bool hit=state.hits.sample(ticks,id,hurt,health,now);
            if (hit && dot(center-s.camera,center-s.camera)<144. && effect_visible(s,center)) {
                const int count=gui::ui_state::get_preferences().reduce_motion ? 4 : std::clamp(config.count,4,24);
                for (int i=0;i<count;++i) {
                    while (state.particles.size()>=visual_effects_logic::max_particles) state.particles.pop_front();
                    trail_particle p;
                    p.origin=center+point{(state.noise()-.5)*.35,(state.noise()-.5)*.4,(state.noise()-.5)*.35};
                    p.velocity={(state.noise()-.5)*1.7,.2+state.noise()*.8,(state.noise()-.5)*1.7};
                    p.color=tint(config.theme_color,config.color,1.f,i*12); p.born=now;
                    p.radius=.045+state.noise()*.025; p.visible=true; state.particles.push_back(p);
                }
            }
        } else state.hits.sample(ticks,-1,0,0,now);
        const double lifetime=std::clamp(config.lifetime,.2f,1.5f);
        while (!state.particles.empty() && now-state.particles.front().born>=lifetime) state.particles.pop_front();
        // Visibility queries are round-robin and bounded independently of FPS/count.
        int budget=4;
        for (std::size_t n=0;n<state.particles.size() && budget>0;++n) {
            auto& p=state.particles[state.visibility_cursor++%state.particles.size()];
            if (p.next_visibility>now) continue;
            --budget; p.visible=effect_visible(s,p.position(now-p.born)); p.next_visibility=now+.18;
        }
        mesh m;
        for (const auto& p:state.particles) {
            if (!p.visible) continue;
            const double age=now-p.born,t=std::clamp(age/lifetime,0.,1.);
            const point center=p.position(age);
            if (dot(center-s.camera,center-s.camera)>576.) continue;
            auto color=p.color; color.w*=config.opacity*static_cast<float>((1-t)*(1-t));
            const double r=p.radius*std::clamp(config.size,.5f,2.f)*(1-t*.3);
            if (config.style==2) effect_ring(m,center,s.view.right,s.view.up,r,color,12);
            else if (config.style==1) m.edges.push_back({center,center-p.velocity*.06,color});
            else {
                m.edges.push_back({center-s.view.right*r,center+s.view.right*r,color});
                m.edges.push_back({center-s.view.up*r,center+s.view.up*r,color});
                m.edges.push_back({center-(s.view.right+s.view.up)*r*.45,center+(s.view.right+s.view.up)*r*.45,color});
                m.edges.push_back({center-(s.view.right-s.view.up)*r*.45,center+(s.view.right-s.view.up)*r*.45,color});
            }
        }
        render(s,m,config.glow); state.advance(s,now);
    } catch (...) { storage.clear(); }
}

void cosmetics::draw_jump_circles(const module_settings::jump_circles& config, visual_effect_state& storage)
{
    if (!platform::jni_context::bind_current_thread()) return;
    effect_cleanup cleanup; jni::frame frame{96};
    auto& state=*storage.state_;
    try {
        scene s; const double now=ImGui::GetTime();
        if (!capture(s,false,false) || !state.observe(s,now)) { storage.clear(); return; }
        const bool grounded=s.context.player.isOnGround.call()==JNI_TRUE;
        const bool excluded=s.context.player.isTouchingWater.call() || s.context.player.isInLava.call() ||
            s.context.player.hasVehicle.call() || s.context.player.isFallFlying.call();
        auto velocity=s.context.player.getVelocity.call();
        if (platform::jni_context::clear_exception() || !velocity) { storage.clear(); return; }
        const double vertical=velocity.y.get();
        if (platform::jni_context::clear_exception() || !std::isfinite(vertical)) { storage.clear(); return; }
        const int event=state.jumps.sample(grounded,vertical>.05);
        if (excluded) state.jumps.reset();
        if (!excluded && event && (config.trigger==2 || config.trigger==event-1)) {
            const point origin=event==1 ? state.previous : s.position;
            trail_ray ground;
            if (cast_trail_ray(s,origin+point{0,.25,0},origin-point{0,1.2,0},ground,true) && ground.blocked && ground.top) {
                while (state.circles.size()>=visual_effects_logic::max_circles) state.circles.pop_front();
                footprint ring; ring.position=ground.position+point{0,.025,0}; ring.born=now;
                ring.color=tint(config.theme_color,config.color,1.f); state.circles.push_back(ring);
            }
        }
        const double lifetime=std::clamp(config.lifetime,.3f,2.f);
        while (!state.circles.empty() && now-state.circles.front().born>=lifetime) state.circles.pop_front();
        mesh m; int budget=2;
        for (auto& ring:state.circles) {
            if (ring.next_visibility<=now && budget>0) {
                --budget; ring.visible=effect_visible(s,ring.position+point{0,.02,0}); ring.next_visibility=now+.2;
            }
            if (!ring.visible || dot(ring.position-s.camera,ring.position-s.camera)>1024.) continue;
            const double t=std::clamp((now-ring.born)/lifetime,0.,1.);
            const double expand=gui::ui_state::get_preferences().reduce_motion ? .75 : .15+.85*(1-std::pow(1-t,3));
            const double radius=std::clamp(config.radius,.3f,1.5f)*expand;
            auto color=ring.color; color.w*=config.opacity*static_cast<float>((1-t)*(1-t));
            effect_ring(m,ring.position,{1,0,0},{0,0,1},radius,color,48);
            if (config.style==1) { color.w*=.5f; effect_ring(m,ring.position,{1,0,0},{0,0,1},radius*.77,color,48); }
        }
        render(s,m,config.glow); state.advance(s,now);
    } catch (...) { storage.clear(); }
}
