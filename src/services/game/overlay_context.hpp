#pragma once
#include "client_context.hpp"
#include "../../modules/core/snapshot_session.hpp"
#include "../../platform/jni/jni_context.hpp"

namespace services::game
{
    struct overlay_identity
    {
        maps::ClientWorld world;
        maps::ClientPlayerEntity player;
        // A temporary view costs no global references. Stored copies retain
        // both Java objects, and compare Java identity, not JNI handle addresses.
        overlay_identity(const maps::ClientWorld& w, const maps::ClientPlayerEntity& p)
            : world(w.object_instance, false), player(p.object_instance, false) {}
        overlay_identity(const overlay_identity& other)
            : world(other.world.object_instance, true), player(other.player.object_instance, true) {}
        overlay_identity& operator=(const overlay_identity& other)
        {
            world = other.world; player = other.player; return *this;
        }
        bool operator==(const overlay_identity& other) const
        {
            const bool same = world.is_same_object(other.world) && player.is_same_object(other.player);
            return !platform::jni_context::clear_exception() && same;
        }
    };
    using overlay_session = modules::snapshot_session<overlay_identity>;
}
