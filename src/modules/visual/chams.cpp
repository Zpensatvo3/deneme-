#include "../../ui/settings_renderer.hpp"
#include "../../services/game/anti_bot_service.hpp"
#include "chams.hpp"
#include "chams_model.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../../services/game/client_context.hpp"
#include "../../friends/friends.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace chams_detail
{
    constexpr double pi = 3.14159265358979323846;
    struct point { double x, y, z; };
    point operator+(point a, point b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
    point operator-(point a, point b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
    point operator*(point a, double s) { return {a.x*s, a.y*s, a.z*s}; }
    double dot(point a, point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
    point cross(point a, point b) { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
    bool finite(point p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
    float lerp_angle(float a, float b, float t) { return a + std::remainder(b-a, 360.f)*t; }
    point rotate(point p, double x, double y, double z)
    {
        p = {p.x, p.y*std::cos(x)-p.z*std::sin(x), p.y*std::sin(x)+p.z*std::cos(x)};
        p = {p.x*std::cos(y)+p.z*std::sin(y), p.y, -p.x*std::sin(y)+p.z*std::cos(y)};
        return {p.x*std::cos(z)-p.y*std::sin(z), p.x*std::sin(z)+p.y*std::cos(z), p.z};
    }
    struct cuboid { point low, high; };
    struct part
    {
        chams_model::Part object{nullptr, true};
        std::vector<cuboid> boxes;
    };
    struct model
    {
        chams_model::Model object{nullptr, true};
        chams_model::RenderState pose{nullptr, true};
        std::array<part, 6> parts;
    };
    struct face
    {
        std::vector<ImVec2> vertices;
        double depth;
        ImU32 color;
    };
    struct glow_edge
    {
        ImVec2 a, b;
        ImVec4 color;
        float radius;
    };
    struct runtime
    {
        jni::object_wrapper resource_map{nullptr, true};
        std::array<model, 2> models;
        std::vector<face> faces;
        std::vector<glow_edge> glow_edges;
        void clear() { models = {}; resource_map.clear_ref(); faces.clear(); glow_edges.clear(); }
    };

    float cross2(ImVec2 a, ImVec2 b) { return a.x*b.y-a.y*b.x; }
    ImVec2 subtract(ImVec2 a, ImVec2 b) { return {a.x-b.x,a.y-b.y}; }
    bool inside(const std::vector<ImVec2>& polygon, ImVec2 p)
    {
        for (std::size_t i = 0; i < polygon.size(); ++i) {
            const auto a = polygon[i], b = polygon[(i+1)%polygon.size()];
            if (cross2(subtract(b,a),subtract(p,a)) < -.001f) return false;
        }
        return true;
    }
    bool close(ImVec2 a, ImVec2 b)
    {
        const auto d = subtract(a,b); return d.x*d.x+d.y*d.y < .04f;
    }
    void append_glow(runtime& state, std::size_t first, ImVec4 color, float radius, const ImGuiViewport& viewport)
    {
        const std::size_t end = state.faces.size(), first_edge = state.glow_edges.size();
        if (first == end) return;
        float low = viewport.Pos.y+viewport.Size.y, high = viewport.Pos.y;
        for (std::size_t i = first; i < end; ++i)
            for (const auto p : state.faces[i].vertices) { low = std::min(low,p.y); high = std::max(high,p.y); }
        // Far-away players should not become oversized glowing dots.
        radius = std::min(radius,std::max(1.f,(high-low)*.08f));
        for (std::size_t i = first; i < end; ++i) {
            const auto& polygon = state.faces[i].vertices;
            for (std::size_t edge = 0; edge < polygon.size(); ++edge) {
                const ImVec2 a = polygon[edge], b = polygon[(edge+1)%polygon.size()];
                const auto direction = subtract(b,a);
                const float length = std::hypot(direction.x,direction.y);
                if (length < .25f) continue;
                // Clipping against the viewport must not create a glowing
                // straight line along the edge of the entire screen.
                if ((std::abs(a.x-viewport.Pos.x)<.1f && std::abs(b.x-viewport.Pos.x)<.1f) ||
                    (std::abs(a.x-viewport.Pos.x-viewport.Size.x)<.1f && std::abs(b.x-viewport.Pos.x-viewport.Size.x)<.1f) ||
                    (std::abs(a.y-viewport.Pos.y)<.1f && std::abs(b.y-viewport.Pos.y)<.1f) ||
                    (std::abs(a.y-viewport.Pos.y-viewport.Size.y)<.1f && std::abs(b.y-viewport.Pos.y-viewport.Size.y)<.1f)) continue;
                std::vector<float> splits{0.f,1.f};
                // Split edges at projected intersections, then keep only the
                // union's outside boundary. Internal cube/limb seams must not glow.
                for (std::size_t j = first; j < end; ++j) {
                    if (j == i) continue;
                    const auto& other = state.faces[j].vertices;
                    for (std::size_t k = 0; k < other.size(); ++k) {
                        const auto c = other[k], s = subtract(other[(k+1)%other.size()],c);
                        const float denominator = cross2(direction,s);
                        if (std::abs(denominator) < .0001f) continue;
                        const auto offset = subtract(c,a);
                        const float t = cross2(offset,s)/denominator, u = cross2(offset,direction)/denominator;
                        if (t > .0001f && t < .9999f && u >= 0.f && u <= 1.f) splits.push_back(t);
                    }
                }
                std::sort(splits.begin(),splits.end());
                splits.erase(std::unique(splits.begin(),splits.end(),[](float x,float y) { return std::abs(x-y)<.0001f; }),splits.end());
                for (std::size_t k = 1; k < splits.size(); ++k) {
                    const float t = (splits[k-1]+splits[k])*.5f;
                    const ImVec2 probe(a.x+direction.x*t+direction.y/length*.35f,
                        a.y+direction.y*t-direction.x/length*.35f);
                    bool covered = false;
                    for (std::size_t j = first; j < end && !covered; ++j)
                        if (j != i) covered = inside(state.faces[j].vertices,probe);
                    if (covered) continue;
                    const ImVec2 p(a.x+direction.x*splits[k-1],a.y+direction.y*splits[k-1]);
                    const ImVec2 q(a.x+direction.x*splits[k],a.y+direction.y*splits[k]);
                    if (close(p,q)) continue;
                    bool duplicate = false;
                    for (std::size_t j = first_edge; j < state.glow_edges.size() && !duplicate; ++j) {
                        const auto& previous = state.glow_edges[j];
                        duplicate = (close(previous.a,p) && close(previous.b,q)) || (close(previous.a,q) && close(previous.b,p));
                    }
                    if (!duplicate) state.glow_edges.push_back({p,q,color,radius});
                }
            }
        }
    }
    void draw_glow(ImDrawList& draw, const std::vector<glow_edge>& edges, float strength)
    {
        // Layered screen-space falloff; independent of Minecraft's shaders.
        // Draw the whole halo behind the model fill, not on each mesh face.
        for (int layer = 8; layer >= 1; --layer) {
            const float t = static_cast<float>(layer)/8.f;
            for (const auto& edge : edges) {
                ImVec4 c = edge.color;
                c.w *= strength*(.018f+.10f*(1.f-t)*(1.f-t));
                draw.AddLine(edge.a,edge.b,ImGui::ColorConvertFloat4ToU32(c),1.f+2.f*edge.radius*t);
            }
        }
        for (const auto& edge : edges) {
            ImVec4 c = edge.color; c.w *= strength*.48f;
            draw.AddLine(edge.a,edge.b,ImGui::ColorConvertFloat4ToU32(c),1.25f);
        }
    }

    bool bake(model& output, const chams_model::Loader& loader, bool slim)
    {
        model candidate;
        auto layer = slim ? chams_model::Layers{}.PLAYER_SLIM.get() : chams_model::Layers{}.PLAYER.get();
        auto root = layer ? loader.getModelPart.call(layer) : chams_model::Part{};
        if (platform::jni_context::clear_exception() || !root || !root.getChild.is_valid()) return false;
        chams_model::Model empty;
        if (!empty.constructor.is_valid()) return false;
        const jboolean thin = slim ? JNI_TRUE : JNI_FALSE;
        candidate.object = chams_model::Model::new_object(&chams_model::Model::constructor, root, thin);
        if (platform::jni_context::clear_exception() || !candidate.object || !candidate.object.setAngles.is_valid()) return false;
        const char* names[]{"head", "body", "right_arm", "left_arm", "right_leg", "left_leg"};
        for (int index = 0; index < 6; ++index) {
            jni::frame frame{48};
            auto& part = candidate.parts[index];
            part.object = root.getChild.call(maps::String::create(names[index]));
            if (platform::jni_context::clear_exception() || !part.object || !part.object.cuboids.is_valid() ||
                !part.object.pivotX.is_valid() || !part.object.pivotY.is_valid() || !part.object.pivotZ.is_valid() ||
                !part.object.pitch.is_valid() || !part.object.yaw.is_valid() || !part.object.roll.is_valid()) return false;
            auto boxes = part.object.cuboids.get();
            auto array = boxes ? boxes.toArray.call() : jni::array<maps::Object>{nullptr};
            if (platform::jni_context::clear_exception() || !array) return false;
            for (const auto& box : array.to_vector()) {
                chams_model::Cuboid c(box.object_instance, false);
                if (!c.minX.is_valid() || !c.minY.is_valid() || !c.minZ.is_valid() ||
                    !c.maxX.is_valid() || !c.maxY.is_valid() || !c.maxZ.is_valid()) return false;
                cuboid b{{c.minX.get(), c.minY.get(), c.minZ.get()}, {c.maxX.get(), c.maxY.get(), c.maxZ.get()}};
                if (platform::jni_context::clear_exception() || !finite(b.low) || !finite(b.high)) return false;
                part.boxes.push_back(b);
            }
        }
        output = std::move(candidate); return true;
    }

    // Clip before perspective division so a limb crossing the camera cannot
    // generate huge screen-space triangles.
    template <typename Distance>
    std::vector<point> clip(const std::vector<point>& input, Distance distance)
    {
        std::vector<point> out;
        if (input.empty()) return out;
        out.reserve(input.size()+1);
        point a = input.back(); double da = distance(a);
        for (point b : input) {
            const double db = distance(b);
            if ((da >= 0) != (db >= 0)) out.push_back(a + (b-a)*(da/(da-db)));
            if (db >= 0) out.push_back(b);
            a = b; da = db;
        }
        return out;
    }
    struct camera
    {
        point position, right, up, forward;
        ImVec2 origin, size;
        double focal;
        point view(point p) const { const auto d = p-position; return {dot(d,right), dot(d,up), dot(d,forward)}; }
    };
    void append_box(runtime& state, const std::array<point,8>& world, const camera& camera, ImVec4 color)
    {
        constexpr int sides[6][4]{{0,3,2,1},{4,5,6,7},{0,4,7,3},{1,2,6,5},{0,1,5,4},{3,7,6,2}};
        for (const auto& side : sides) {
            const auto a = world[side[0]], b = world[side[1]], c = world[side[2]];
            auto normal = cross(b-a,c-a);
            const double length = std::sqrt(dot(normal,normal));
            if (length < 1e-9 || dot(normal,camera.position-a) <= 0) continue;
            normal = normal*(1./length);
            std::vector<point> polygon;
            for (int index : side) polygon.push_back(camera.view(world[index]));
            polygon = clip(polygon, [](point p) { return p.z-.05; });
            if (polygon.size() < 3) continue;
            double depth = 0;
            for (auto& p : polygon) {
                depth += p.z;
                p = {camera.origin.x+camera.size.x*.5+p.x*camera.focal/p.z,
                    camera.origin.y+camera.size.y*.5-p.y*camera.focal/p.z, p.z};
            }
            depth /= polygon.size();
            polygon = clip(polygon, [&](point p) { return p.x-camera.origin.x; });
            polygon = clip(polygon, [&](point p) { return camera.origin.x+camera.size.x-p.x; });
            polygon = clip(polygon, [&](point p) { return p.y-camera.origin.y; });
            polygon = clip(polygon, [&](point p) { return camera.origin.y+camera.size.y-p.y; });
            if (polygon.size() < 3) continue;
            face output; output.depth = depth;
            double area = 0;
            for (std::size_t i = 0; i < polygon.size(); ++i) {
                const auto p = polygon[i], q = polygon[(i+1)%polygon.size()];
                if (!finite(p)) return;
                area += p.x*q.y-q.x*p.y;
                output.vertices.emplace_back(static_cast<float>(p.x),static_cast<float>(p.y));
            }
            if (std::abs(area) < .1) continue;
            if (area < 0) std::reverse(output.vertices.begin(),output.vertices.end());
            const float shade = static_cast<float>(.76+.24*std::max(0.,dot(normal,point{-.4,.8,-.25})));
            output.color = ImGui::ColorConvertFloat4ToU32({color.x*shade,color.y*shade,color.z*shade,color.w});
            state.faces.push_back(std::move(output));
        }
    }
    void append_model(runtime& state, model& model, point position, float body_yaw, const camera& camera, ImVec4 color)
    {
        for (const auto& part : model.parts) {
            point pivot{part.object.pivotX.get(),part.object.pivotY.get(),part.object.pivotZ.get()};
            const double pitch = part.object.pitch.get(), yaw = part.object.yaw.get(), roll = part.object.roll.get();
            if (platform::jni_context::clear_exception() || !finite(pivot) ||
                !std::isfinite(pitch) || !std::isfinite(yaw) || !std::isfinite(roll)) continue;
            for (const auto& box : part.boxes) {
                std::array<point,8> vertices{{
                    {box.low.x,box.low.y,box.low.z},{box.high.x,box.low.y,box.low.z},
                    {box.high.x,box.high.y,box.low.z},{box.low.x,box.high.y,box.low.z},
                    {box.low.x,box.low.y,box.high.z},{box.high.x,box.low.y,box.high.z},
                    {box.high.x,box.high.y,box.high.z},{box.low.x,box.high.y,box.high.z}}};
                for (auto& p : vertices) {
                    p = (rotate(p,pitch,yaw,roll)+pivot)*(1./16.);
                    p = {-p.x*.9375,(1.501-p.y)*.9375,p.z*.9375};
                    p = rotate(p,0,(180.-body_yaw)*pi/180.,0)+position;
                }
                append_box(state,vertices,camera,color);
            }
        }
    }
}

modules::chams::chams() : module("chams", "Visual"), state(std::make_unique<chams_detail::runtime>()) {}
modules::chams::~chams() = default;
void modules::chams::run(::cache&) {}
void modules::chams::on_enable(::cache&) { reset_requested.store(true); }
void modules::chams::on_disable(::cache&) { reset_requested.store(true); }

void modules::chams::render_overlay()
{
    using namespace chams_detail;
    if (!platform::jni_context::bind_current_thread()) return;
    if (reset_requested.exchange(false) || !enabled) state->clear();
    if (!enabled || !gui::runtime_ready.load(std::memory_order_acquire) || gui::should_unload) return;
    struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } cleanup;
    try {
        jni::frame frame{128};
        services::game::client_context context;
        if (!services::game::capture_client_context(context,true,false)) { state->clear(); return; }
        if (context.screen) return;
        const auto config = settings.read();
        if (!std::isfinite(config.opacity) ||
            !std::isfinite(config.glow_strength) || !std::isfinite(config.glow_radius)) return;
        const float opacity = std::clamp(config.opacity,0.f,1.f);
        const float glow_strength = config.glow ? std::clamp(config.glow_strength,0.f,1.f) : 0.f;
        const float glow_radius = std::clamp(config.glow_radius,2.f,20.f);
        if (opacity == 0) return;
        auto& adapter = platform::adapter::get();
        const float delta = std::clamp(adapter.get_tick_delta(context.instance),0.f,1.f);
        const auto source = adapter.get_camera_data(context.instance,context.player,delta);
        if (!source.valid || !std::isfinite(source.fov) || source.fov <= 1 || source.fov >= 179 ||
            !std::isfinite(source.yaw) || !std::isfinite(source.pitch)) return;
        const auto* viewport = ImGui::GetMainViewport();
        const double yaw = source.yaw*pi/180., pitch = source.pitch*pi/180.;
        camera camera{{source.pos.x,source.pos.y,source.pos.z},
            {-std::cos(yaw),0,-std::sin(yaw)},
            {-std::sin(yaw)*std::sin(pitch),std::cos(pitch),std::cos(yaw)*std::sin(pitch)},
            {-std::sin(yaw)*std::cos(pitch),-std::sin(pitch),std::cos(yaw)*std::cos(pitch)},
            viewport->Pos,viewport->Size,viewport->Size.y/(2*std::tan(source.fov*pi/360.))};
        if (!finite(camera.position) || !std::isfinite(delta) || viewport->Size.y <= 0) return;
        auto loader = chams_model::Client(context.instance.object_instance,false).getEntityModelLoader.call();
        auto resource_map = loader ? loader.modelParts.get() : chams_model::Map{};
        if (platform::jni_context::clear_exception() || !loader || !resource_map) return;
        if (!state->resource_map || !state->resource_map.is_same_object(resource_map)) {
            state->clear(); state->resource_map = resource_map;
        }
        auto players = adapter.get_players(context.world);
        auto array = players ? players.toArray.call() : jni::array<maps::Object>{nullptr};
        if (platform::jni_context::clear_exception() || !array) return;
        state->faces.clear();
        state->glow_edges.clear();
        const int count = std::min<int>(array.get_length(),256);
        for (int index = 0, drawn = 0; index < count && drawn < 64; ++index) {
            jni::frame entity_frame{96};
            maps::Entity entity(jni::get_env()->GetObjectArrayElement(static_cast<jobjectArray>(array.object_instance),index),false);
            if (!entity || entity.is_same_object(context.player) || !entity.is_instance_of<maps::PlayerEntity>() ||
                entity.isAlive.call() != JNI_TRUE || entity.isInvisible.call() == JNI_TRUE ||
                (config.ignore_friends && friends::is_friend(entity)) || platform::jni_context::clear_exception()) {
                platform::jni_context::clear_exception(); continue;
            }
            if (services::anti_bot::is_bot(entity)) continue;
            maps::LivingEntity living(entity.object_instance,false);
            // Special root transforms are excluded rather than drawing an
            // upright model over a player who is flying, swimming or in bed.
            if (living.isFallFlying.call() || entity.isSwimming.call() || living.isSleeping.call() || entity.hasVehicle.call() ||
                platform::jni_context::clear_exception()) { platform::jni_context::clear_exception(); continue; }
            auto lerped = entity.getLerpedPos.call(delta);
            if (!lerped || platform::jni_context::clear_exception()) continue;
            const auto p = lerped.to_vector3d();
            point position{p.x,p.y,p.z};
            if (!finite(position)) continue;
            chams_model::Living local(context.player.object_instance,false);
            if (!local.canSee.is_valid()) continue;
            const bool visible = local.canSee.call(entity) == JNI_TRUE;
            if (platform::jni_context::clear_exception() || (!visible && !config.through_walls)) continue;
            bool slim = false;
            auto skin = chams_model::PlayerSkin(entity.object_instance,false).getSkinTextures.call();
            auto skin_model = skin ? skin.model.call() : chams_model::SkinModel{};
            auto name = skin_model ? skin_model.name.call() : maps::String{};
            if (name) slim = name.to_string() == "SLIM";
            platform::jni_context::clear_exception();
            auto& model = state->models[slim ? 1 : 0];
            if (!model.object && !bake(model,loader,slim)) { platform::jni_context::clear_exception(); continue; }
            chams_model::Living pose(entity.object_instance,false);
            if (!pose.bodyYaw.is_valid() || !pose.prevBodyYaw.is_valid() || !pose.headYaw.is_valid() ||
                !pose.prevHeadYaw.is_valid() || !pose.limbAnimator.is_valid() || !pose.isInSneakingPose.is_valid() ||
                !pose.getHandSwingProgress.is_valid()) continue;
            auto limb = pose.limbAnimator.get();
            if (!limb || !limb.getPos.is_valid() || !limb.getSpeed.is_valid()) continue;
            const float body = lerp_angle(pose.prevBodyYaw.get(),pose.bodyYaw.get(),delta);
            const float head = lerp_angle(pose.prevHeadYaw.get(),pose.headYaw.get(),delta);
            const float head_pitch = entity.prevPitch.get()+(entity.pitch.get()-entity.prevPitch.get())*delta;
            const float limb_pos = limb.getPos.call(delta), speed = std::clamp(limb.getSpeed.call(delta),0.f,1.f);
            const float age = static_cast<float>(entity.age.get())+delta;
            if (platform::jni_context::clear_exception() || !std::isfinite(body) || !std::isfinite(head) ||
                !std::isfinite(head_pitch) || !std::isfinite(limb_pos) || !std::isfinite(speed)) continue;
            const jboolean crouching = pose.isInSneakingPose.call();
            auto dispatcher = context.instance.entityRenderDispatcher.get();
            auto renderer = chams_model::Dispatcher(dispatcher.object_instance, false).getRenderer.call(entity);
            if (!renderer || platform::jni_context::clear_exception()) continue;
            // Own the render state; do not overwrite the renderer's shared state.
            if (!model.pose) model.pose = renderer.createRenderState.call();
            if (!model.pose || platform::jni_context::clear_exception()) continue;
            renderer.updateRenderState.call(entity, model.pose, delta);
            if (platform::jni_context::clear_exception()) continue;
            model.object.setAngles.call(chams_model::PlayerPose(model.pose.object_instance, false));
            if (platform::jni_context::clear_exception()) continue;
            ImVec4 color;
            if (visible && config.theme_color) color = gui::get_dynamic_color(index*12).Value;
            else {
                const auto* values = visible ? config.visible_color : config.hidden_color;
                color = {values[0],values[1],values[2],values[3]};
            }
            if (!std::isfinite(color.x) || !std::isfinite(color.y) || !std::isfinite(color.z) || !std::isfinite(color.w)) continue;
            color.x = std::clamp(color.x,0.f,1.f); color.y = std::clamp(color.y,0.f,1.f); color.z = std::clamp(color.z,0.f,1.f);
            color.w = opacity*std::clamp(color.w,0.f,1.f)*(visible ? 1.f : .7f);
            if (crouching == JNI_TRUE) position.y -= .125;
            const auto first_face = state->faces.size();
            append_model(*state,model,position,body,camera,color);
            if (glow_strength > 0) append_glow(*state,first_face,color,glow_radius,*viewport);
            ++drawn;
        }
        if (!enabled || reset_requested.load()) { state->faces.clear(); return; }
        std::sort(state->faces.begin(),state->faces.end(),[](const face& a,const face& b) { return a.depth > b.depth; });
        auto* draw = ImGui::GetBackgroundDrawList();
        draw->PushClipRect(viewport->Pos,ImVec2(viewport->Pos.x+viewport->Size.x,viewport->Pos.y+viewport->Size.y),true);
        if (glow_strength > 0) draw_glow(*draw,state->glow_edges,glow_strength);
        for (const auto& face : state->faces) draw->AddConvexPolyFilled(face.vertices.data(),static_cast<int>(face.vertices.size()),face.color);
        draw->PopClipRect();
    } catch (...) { platform::jni_context::clear_exception(); state->faces.clear(); }
}

void modules::chams::render_options()
{
    gui::settings::render(settings, settings_type::schema());
}
