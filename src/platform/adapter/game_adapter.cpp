#include "game_adapter.hpp"
#include "../jni/jni_context.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace platform::adapter
{
    // =========================================================================
    // Each binary uses one mapping profile, selected at compile time.
    // =========================================================================
    class target_adapter final : public IGameAdapter
    {
    public:
        maps::Box get_bounding_box(jobject entity_obj) const override
        {
            if (!entity_obj) return maps::Box{ nullptr };
            maps::Entity entity(entity_obj, false);
            return entity.get_bounding_box();
        }

        maths::vector3d get_position(jobject entity_obj) const override
        {
            if (!entity_obj) return { 0, 0, 0 };
            maps::Entity entity(entity_obj, false);
            return entity.get_position();
        }

        maths::vector3d get_prev_position(jobject entity_obj) const override
        {
            if (!entity_obj) return { 0, 0, 0 };
            maps::Entity entity(entity_obj, false);
            return entity.get_prev_position();
        }

        maths::angles get_angles(jobject entity_obj) const override
        {
            if (!entity_obj) return { 0, 0 };
            maps::Entity entity(entity_obj, false);
            return entity.get_angles();
        }

        double get_eye_height(jobject entity_obj) const override
        {
            if (!entity_obj) return 1.62;
            maps::Entity entity(entity_obj, false);
            try
            {
                if (entity.getEyeHeight.is_valid() && entity.getPose.is_valid())
                {
                    maps::EntityPose pose = entity.getPose.call();
                    float h = entity.getEyeHeight.call(pose);
                    if (h > 0.1f && h < 5.0f && !platform::jni_context::clear_exception())
                        return static_cast<double>(h);
                }
                if (entity.getStandingEyeHeight.is_valid())
                {
                    float h = entity.getStandingEyeHeight.call();
                    if (h > 0.1f && h < 5.0f && !platform::jni_context::clear_exception())
                        return static_cast<double>(h);
                }
            }
            catch (...)
            {
                platform::jni_context::clear_exception();
            }
            return 1.62;
        }

        maths::vector3d get_eye_position(jobject entity_obj) const override
        {
            if (!entity_obj) return { 0, 0, 0 };
            maps::Entity entity(entity_obj, false);
            try
            {
                if (entity.getEyeY.is_valid())
                {
                    jdouble ey = entity.getEyeY.call();
                    if (!platform::jni_context::clear_exception() && std::isfinite(ey))
                    {
                        maths::vector3d p = get_position(entity_obj);
                        p.y = ey;
                        return p;
                    }
                }
            }
            catch (...)
            {
                platform::jni_context::clear_exception();
            }
            maths::vector3d p = get_position(entity_obj);
            p.y += get_eye_height(entity_obj);
            return p;
        }

        bool is_on_ground(jobject entity_obj) const override
        {
            if (!entity_obj) return false;
            maps::Entity entity(entity_obj, false);
            try
            {
                if (entity.isOnGround.is_valid() && entity.isOnGround.call() == JNI_TRUE)
                    return true;
                platform::jni_context::clear_exception();

                if (entity.onGround.is_valid() && entity.onGround.get() == JNI_TRUE)
                    return true;
                platform::jni_context::clear_exception();
            }
            catch (...)
            {
                platform::jni_context::clear_exception();
            }
            return false;
        }

        maps::List get_players(const maps::ClientWorld& world) const override
        {
            if (!world) return maps::List{ nullptr };
            try
            {
                if (world.players.is_valid())
                {
                    maps::List p = world.players.get();
                    if (p && !platform::jni_context::clear_exception()) return p;
                }
                platform::jni_context::clear_exception();

                if (world.getPlayers.is_valid())
                {
                    maps::List p = world.getPlayers.call();
                    if (p && !platform::jni_context::clear_exception()) return p;
                }
                platform::jni_context::clear_exception();
            }
            catch (...)
            {
                platform::jni_context::clear_exception();
            }
            return maps::List{ nullptr };
        }

        maps::Iterable get_entities(const maps::ClientWorld& world) const override
        {
            if (!world) return maps::Iterable{ nullptr };
            try
            {
                if (world.getEntities.is_valid())
                {
                    maps::Iterable it = world.getEntities.call();
                    if (it && !platform::jni_context::clear_exception()) return it;
                }
            }
            catch (...)
            {
                platform::jni_context::clear_exception();
            }
            return maps::Iterable{ nullptr };
        }

        float get_tick_delta(const maps::MinecraftClient& client) const override
        {
            if (!client) return 1.0f;
            try
            {
                maps::RenderTickCounter rtc = client.renderTickCounter.get();
                if (rtc)
                    return rtc.get_tick_delta();
            }
            catch (...)
            {
                platform::jni_context::clear_exception();
            }
            return 1.0f;
        }

        float get_fov(const maps::MinecraftClient& client, float tick_delta) const override
        {
            if (!client) return 70.0f;
            JNIEnv* env = jni::get_env();

            // Method 1: Ask GameRenderer with live Camera
            try
            {
                maps::GameRenderer renderer = client.gameRenderer.get();
                if (renderer)
                {
                    maps::Camera cam = renderer.getCamera.is_valid() ? renderer.getCamera.call() : maps::Camera{ nullptr };
                    if (!cam && client.entityRenderDispatcher.is_valid())
                    {
                        maps::EntityRenderDispatcher erd = client.entityRenderDispatcher.get();
                        if (erd && erd.camera.is_valid())
                            cam = erd.camera.get();
                    }

                    if (cam)
                    {
#if ONESTEP_MINECRAFT_26_1
                        if (cam.getFov.is_valid())
                        {
                            const float render_fov = cam.getFov.call();
                            if (!platform::jni_context::clear_exception() && std::isfinite(render_fov) &&
                                render_fov >= 1.0f && render_fov <= 175.0f)
                                return render_fov;
                        }
#else
                        if (renderer.getFov.is_valid())
                        {
                            double render_fov = renderer.getFov.call(cam, tick_delta, JNI_TRUE);
                            if (!platform::jni_context::clear_exception() && std::isfinite(render_fov) &&
                                render_fov >= 1.0 && render_fov <= 175.0)
                            {
                                return static_cast<float>(render_fov);
                            }
                        }

                        // JNI fallback if method_3196 returns float on this runtime
                        if (env && renderer.object_instance && cam.object_instance)
                        {
                            jclass gr_cls = env->GetObjectClass(renderer.object_instance);
                            if (gr_cls)
                            {
                                jmethodID mid_f = env->GetMethodID(gr_cls, ONESTEP_MC_NAME("method_3196"), ONESTEP_MC_NAME("(Lnet/minecraft/class_4184;FZ)F"));
                                if (mid_f && !env->ExceptionCheck())
                                {
                                    jfloat rf = env->CallFloatMethod(renderer.object_instance, mid_f, cam.object_instance, (jfloat)tick_delta, JNI_TRUE);
                                    if (!env->ExceptionCheck() && std::isfinite(rf) && rf >= 1.0f && rf <= 175.0f)
                                    {
                                        env->DeleteLocalRef(gr_cls);
                                        return rf;
                                    }
                                }
                                if (env->ExceptionCheck()) env->ExceptionClear();
                                env->DeleteLocalRef(gr_cls);
                            }
                        }
#endif
                    }
                }
            }
            catch (...)
            {
                platform::jni_context::clear_exception();
            }
            platform::jni_context::clear_exception();

            // Method 2: Ask GameOptions FOV SimpleOption
            try
            {
                maps::GameOptions opts = client.options.get();
                if (opts && opts.fov.is_valid())
                {
                    maps::SimpleOption fov_opt = opts.fov.get();
                    if (fov_opt && fov_opt.getValue.is_valid())
                    {
                        maps::Object val = fov_opt.getValue();
                        if (val && env)
                        {
                            jclass num_cls = env->FindClass("java/lang/Number");
                            if (num_cls && !env->ExceptionCheck())
                            {
                                jmethodID dv = env->GetMethodID(num_cls, "doubleValue", "()D");
                                if (dv && !env->ExceptionCheck())
                                {
                                    jdouble d = env->CallDoubleMethod(val.object_instance, dv);
                                    if (!env->ExceptionCheck() && d >= 1.0 && d <= 175.0)
                                    {
                                        env->DeleteLocalRef(num_cls);
                                        return static_cast<float>(d);
                                    }
                                }
                                if (env->ExceptionCheck()) env->ExceptionClear();
                                env->DeleteLocalRef(num_cls);
                            }
                            else if (env->ExceptionCheck())
                            {
                                env->ExceptionClear();
                            }
                        }
                    }
                }
            }
            catch (...)
            {
                platform::jni_context::clear_exception();
            }

            return 70.0f;
        }

        CameraData get_camera_data(const maps::MinecraftClient& client, jobject player_obj, float tick_delta) const override
        {
            CameraData data{};
            data.fov = get_fov(client, tick_delta);

            if (!client)
            {
                if (player_obj)
                {
                    data.pos = get_eye_position(player_obj);
                    maths::angles a = get_angles(player_obj);
                    data.yaw = a.yaw;
                    data.pitch = a.pitch;
                    data.valid = true;
                }
                return data;
            }

            JNIEnv* env = jni::get_env();

            // Attempt 1: GameRenderer camera (authoritative camera in Minecraft render loop, supports freelook & third person)
            maps::Camera cam{ nullptr };
            try
            {
                if (client.gameRenderer.is_valid())
                {
                    maps::GameRenderer renderer = client.gameRenderer.get();
                    if (renderer)
                    {
                        if (renderer.getCamera.is_valid())
                            cam = renderer.getCamera.call();

                        if (!cam && env && renderer.object_instance)
                        {
                            jclass gr_cls = env->GetObjectClass(renderer.object_instance);
                            if (gr_cls)
                            {
                                jmethodID mid_gc = env->GetMethodID(gr_cls, ONESTEP_MC_NAME("method_19418"), ONESTEP_MC_NAME("()Lnet/minecraft/class_4184;"));
                                if (mid_gc && !env->ExceptionCheck())
                                {
                                    jobject cam_obj = env->CallObjectMethod(renderer.object_instance, mid_gc);
                                    if (!env->ExceptionCheck() && cam_obj)
                                        cam = maps::Camera(cam_obj, false);
                                }
                                if (env->ExceptionCheck()) env->ExceptionClear();
                                env->DeleteLocalRef(gr_cls);
                            }
                        }
                    }
                }
            }
            catch (...) {}
            platform::jni_context::clear_exception();

            // Attempt 2: EntityRenderDispatcher camera
            if (!cam)
            {
                try
                {
                    if (client.entityRenderDispatcher.is_valid())
                    {
                        maps::EntityRenderDispatcher erd = client.entityRenderDispatcher.get();
                        if (erd && erd.camera.is_valid())
                        {
                            cam = erd.camera.get();
                        }
                    }
                }
                catch (...) {}
                platform::jni_context::clear_exception();
            }

            // Extract camera transforms (works in 1st person, freelook, and 3rd person)
            if (cam)
            {
                try
                {
                    maps::Vec3d cp = cam.getPos();
                    float cyaw = cam.getYaw();
                    float cpitch = cam.getPitch();
                    if (!platform::jni_context::clear_exception() && cp)
                    {
                        data.pos = cp.to_vector3d();
                        data.yaw = cyaw;
                        data.pitch = cpitch;
                        data.valid = true;
                        return data;
                    }
                }
                catch (...) {}
                platform::jni_context::clear_exception();

                // Direct JNI fallback on Camera
                if (env && cam.object_instance)
                {
                    try
                    {
                        jclass cam_cls = env->GetObjectClass(cam.object_instance);
                        if (cam_cls)
                        {
                            jmethodID mid_pos = env->GetMethodID(cam_cls, maps::bindings_profile::camera_position, ONESTEP_MC_NAME("()Lnet/minecraft/class_243;"));
                            jmethodID mid_yaw = env->GetMethodID(cam_cls, ONESTEP_MC_NAME("method_19330"), "()F");
                            jmethodID mid_pitch = env->GetMethodID(cam_cls, ONESTEP_MC_NAME("method_19329"), "()F");
                            if (mid_pos && mid_yaw && mid_pitch && !env->ExceptionCheck())
                            {
                                jobject pos_obj = env->CallObjectMethod(cam.object_instance, mid_pos);
                                jfloat yaw_val = env->CallFloatMethod(cam.object_instance, mid_yaw);
                                jfloat pitch_val = env->CallFloatMethod(cam.object_instance, mid_pitch);
                                if (pos_obj && !env->ExceptionCheck())
                                {
                                    maps::Vec3d pos_vec(pos_obj, false);
                                    data.pos = pos_vec.to_vector3d();
                                    data.yaw = yaw_val;
                                    data.pitch = pitch_val;
                                    data.valid = true;
                                    env->DeleteLocalRef(cam_cls);
                                    return data;
                                }
                            }
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            env->DeleteLocalRef(cam_cls);
                        }
                    }
                    catch (...) {}
                    platform::jni_context::clear_exception();
                }
            }

            // Fallback: Smoothly interpolate player eye position & angles using tick_delta
            if (player_obj)
            {
                try
                {
                    maps::Entity player(player_obj, false);
                    const maths::vector3d cur_pos = player.get_position();
                    const maths::vector3d prev_pos = player.get_prev_position();
                    const maths::angles cur_ang = player.get_angles();
                    const maths::angles prev_ang = player.get_prev_angles();
                    const double eye_h = get_eye_height(player_obj);

                    maths::vector3d lerped_pos = cur_pos;
                    bool has_lerp = false;
                    if (player.getLerpedPos.is_valid())
                    {
                        maps::Vec3d lp = player.getLerpedPos.call(tick_delta);
                        if (lp && !platform::jni_context::clear_exception())
                        {
                            lerped_pos = lp.to_vector3d();
                            has_lerp = true;
                        }
                        platform::jni_context::clear_exception();
                    }

                    if (!has_lerp)
                    {
                        if (prev_pos.x != 0.0 || prev_pos.y != 0.0 || prev_pos.z != 0.0)
                        {
                            lerped_pos.x = prev_pos.x + (cur_pos.x - prev_pos.x) * tick_delta;
                            lerped_pos.y = prev_pos.y + (cur_pos.y - prev_pos.y) * tick_delta;
                            lerped_pos.z = prev_pos.z + (cur_pos.z - prev_pos.z) * tick_delta;
                        }
                    }

                    data.pos = lerped_pos;
                    data.pos.y += eye_h;

                    float diff_yaw = cur_ang.yaw - prev_ang.yaw;
                    while (diff_yaw < -180.0f) diff_yaw += 360.0f;
                    while (diff_yaw >= 180.0f) diff_yaw -= 360.0f;
                    data.yaw = prev_ang.yaw + diff_yaw * tick_delta;
                    data.pitch = prev_ang.pitch + (cur_ang.pitch - prev_ang.pitch) * tick_delta;
                    data.valid = true;
                    platform::jni_context::clear_exception();
                    return data;
                }
                catch (...)
                {
                    platform::jni_context::clear_exception();
                }
            }

            return data;
        }

    };

    static target_adapter active_adapter;

    IGameAdapter& get()
    {
        return active_adapter;
    }

    void initialize(mc::version ver)
    {
        if (!mc::matches_target(ver))
            throw std::runtime_error("Minecraft adapter target does not match the loaded game");
    }
}
