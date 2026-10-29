#pragma once
#include <jni.h>
#include <string>
#include <memory>
#include "../../utils/version_manager.hpp"
#include "../../mappings.hpp"
#include "../../maths/maths.hpp"

namespace platform::adapter
{
    struct CameraData
    {
        maths::vector3d pos{};
        float yaw = 0.0f;
        float pitch = 0.0f;
        float fov = 70.0f;
        bool valid = false;
    };

    class IGameAdapter
    {
    public:
        virtual ~IGameAdapter() = default;
        virtual mc::version get_version() const = 0;
        virtual const char* get_version_name() const = 0;

        // Bounding box & Position raw / jobject
        virtual maps::Box get_bounding_box(jobject entity_obj) const = 0;
        virtual maths::vector3d get_position(jobject entity_obj) const = 0;
        virtual maths::vector3d get_prev_position(jobject entity_obj) const = 0;
        virtual maths::angles get_angles(jobject entity_obj) const = 0;
        virtual double get_eye_height(jobject entity_obj) const = 0;
        virtual maths::vector3d get_eye_position(jobject entity_obj) const = 0;
        virtual bool is_on_ground(jobject entity_obj) const = 0;

        // Template overloads to allow passing any maps::Entity / maps::ClientPlayerEntity / maps::PlayerEntity / etc.
        template<typename T>
        maps::Box get_bounding_box(const T& entity) const
        {
            return get_bounding_box(entity.object_instance);
        }

        template<typename T>
        maths::vector3d get_position(const T& entity) const
        {
            return get_position(entity.object_instance);
        }

        template<typename T>
        maths::vector3d get_prev_position(const T& entity) const
        {
            return get_prev_position(entity.object_instance);
        }

        template<typename T>
        maths::angles get_angles(const T& entity) const
        {
            return get_angles(entity.object_instance);
        }

        template<typename T>
        double get_eye_height(const T& entity) const
        {
            return get_eye_height(entity.object_instance);
        }

        template<typename T>
        maths::vector3d get_eye_position(const T& entity) const
        {
            return get_eye_position(entity.object_instance);
        }

        template<typename T>
        bool is_on_ground(const T& entity) const
        {
            return is_on_ground(entity.object_instance);
        }

        // Camera & Render tick
        virtual CameraData get_camera_data(const maps::MinecraftClient& client, jobject player_obj, float tick_delta) const = 0;

        template<typename T>
        CameraData get_camera_data(const maps::MinecraftClient& client, const T& player, float tick_delta) const
        {
            return get_camera_data(client, player.object_instance, tick_delta);
        }

        virtual float get_tick_delta(const maps::MinecraftClient& client) const = 0;
        virtual float get_fov(const maps::MinecraftClient& client, float tick_delta) const = 0;

        // World & entities
        virtual maps::List get_players(const maps::ClientWorld& world) const = 0;
        virtual maps::Iterable get_entities(const maps::ClientWorld& world) const = 0;

        // Movement & combat checks
        virtual maps::HitResult crosshair_target(const maps::MinecraftClient& client,
            const maps::ClientPlayerEntity& player, const maps::Entity& camera,
            double block_range, double entity_range, float delta) const = 0;
    };

    IGameAdapter& get();
    void initialize(mc::version ver);
}
