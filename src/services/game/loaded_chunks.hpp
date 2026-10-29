#pragma once

#include "../../sdk/minecraft/common/client_entities.hpp"
#include "../../platform/jni/jni_context.hpp"
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace services::game
{
    inline std::uint64_t chunk_key(int x, int z)
    {
        return (std::uint64_t(static_cast<std::uint32_t>(x)) << 32) |
            static_cast<std::uint32_t>(z);
    }

    struct loaded_chunk_slot { int slot, x, z; };
    struct loaded_chunks
    {
        maps::AtomicReferenceArray array{}; // Local reference, caller owns a JNI frame.
        std::vector<loaded_chunk_slot> slots;
        std::unordered_set<std::uint64_t> keys;
    };

    inline bool capture_loaded_chunks(const maps::ClientWorld& world, loaded_chunks& output)
    {
        output = {};
        if (!world || !jni::get_env()) return false;
        auto manager = world.getChunkManager.call();
        auto map = manager ? manager.chunks.get() : maps::ClientChunkMap{};
        output.array = map ? map.chunks.get() : maps::AtomicReferenceArray{};
        if (platform::jni_context::clear_exception() || !output.array) return false;
        const int length = output.array.length.call();
        if (platform::jni_context::clear_exception() || length < 0) return false;
        for (int slot = 0; slot < length; ++slot)
        {
            jni::frame frame{16};
            auto object = output.array.get.call(slot);
            if (platform::jni_context::clear_exception()) return false;
            if (!object) continue;
            maps::WorldChunk chunk(object.object_instance, false);
            auto pos = chunk.getPos.call();
            if (platform::jni_context::clear_exception() || !pos) return false;
            const int x = pos.x.get(), z = pos.z.get();
            if (platform::jni_context::clear_exception()) return false;
            output.slots.push_back({slot, x, z});
            output.keys.insert(chunk_key(x, z));
        }
        return true;
    }

    // A slot can be recycled by a packet while the worker is scanning. Never
    // associate a replacement chunk's data with the old chunk coordinates.
    inline maps::WorldChunk read_loaded_chunk(const loaded_chunks& source, const loaded_chunk_slot& slot)
    {
        auto object = source.array.get.call(slot.slot);
        if (platform::jni_context::clear_exception() || !object) return {};
        maps::WorldChunk chunk(object.object_instance, false);
        auto pos = chunk.getPos.call();
        if (platform::jni_context::clear_exception() || !pos) return {};
        const int x = pos.x.get(), z = pos.z.get();
        if (platform::jni_context::clear_exception() || x != slot.x || z != slot.z) return {};
        return chunk;
    }
}
