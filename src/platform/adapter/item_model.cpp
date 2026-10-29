#include "item_model.hpp"
#include "item_transform.hpp"
#include "../jni/jni_context.hpp"
#include <bit>
#include <cmath>

namespace platform::adapter
{
    namespace
    {
        bool append(const maps::List& quads, const maps::ItemTransformation& transform,
            bool side_lit, const maps::TextureManager& textures, const std::vector<jint>& tints,
            std::vector<item_model_quad>& output, const std::array<float, 16>* local_transform = nullptr)
        {
            using jni_context::clear_exception;
            if (!quads || !transform) return false;
            const auto rotation = transform.rotation_components(), translation = transform.translation_components(), scale = transform.scale_components();
            if (clear_exception()) return false;
            for (int i = 0; i < 3; ++i)
                if (!std::isfinite(rotation[i]) || !std::isfinite(translation[i]) || !std::isfinite(scale[i])) return false;
            auto objects = quads.toArray.call();
            if (!objects || clear_exception()) return false;
            const int count = objects.get_length();
            if (count < 0 || count > 512 || clear_exception()) return false;
            for (int index = 0; index < count; ++index)
            {
                jni::frame frame{32};
                maps::ItemBakedQuad baked(jni::get_env()->GetObjectArrayElement(static_cast<jobjectArray>(objects.object_instance), index), false);
                if (!baked || clear_exception()) return false;
                const auto packed = baked.vertex_data();
                if (clear_exception() || packed.size() < 32 || packed.size() > 256 || packed.size() % 4) return false;
                item_model_quad quad;
                quad.rotation = rotation; quad.translation = translation; quad.scale = scale; quad.side_lit = side_lit;
                const auto stride = packed.size() / 4;
                for (int vertex = 0; vertex < 4; ++vertex)
                {
                    for (int axis = 0; axis < 3; ++axis) quad.position[vertex][axis] = std::bit_cast<float>(packed[vertex * stride + axis]);
                    if (local_transform && !apply_item_local_transform(quad.position[vertex], *local_transform)) return false;
                    for (int axis = 0; axis < 2; ++axis) quad.uv[vertex][axis] = std::bit_cast<float>(packed[vertex * stride + 4 + axis]);
                }
#if ONESTEP_MINECRAFT_26_1
                auto material = baked.getMaterial.call();
                if (!material || clear_exception()) return false;
                auto sprite = material.getSprite.call();
#else
                auto sprite = baked.getSprite.call();
#endif
                auto atlas = sprite ? sprite.getAtlasId.call() : maps::Identifier{};
                auto texture = atlas ? textures.getTexture.call(atlas) : maps::Texture{};
                if (!texture || clear_exception()) return false;
                const int gl_id = texture.getGlId.call();
                if (gl_id <= 0 || clear_exception()) return false;
                quad.texture = static_cast<std::uintptr_t>(gl_id);
#if ONESTEP_MINECRAFT_26_1
                const int tint = material.getColorIndex.call();
#else
                const int tint = baked.getColorIndex.call();
#endif
                if (clear_exception()) return false;
                if (tint >= 0)
                {
                    if (static_cast<std::size_t>(tint) < tints.size()) quad.tint = static_cast<std::uint32_t>(tints[tint]);
                    if (clear_exception()) return false;
                }
                output.push_back(quad);
                if (output.size() > 512) return false;
            }
            return true;
        }

    }

    std::vector<item_model_quad> read_item_model(const maps::MinecraftClient& client,
        const maps::World& world, const maps::LivingEntity& player,
        const maps::ItemStack& stack, std::string_view key)
    {
        using jni_context::clear_exception;
        jni::frame frame{96};
        std::vector<item_model_quad> output;
        auto textures = client.getTextureManager.call();
        if (!textures || clear_exception()) return {};
        auto manager = maps::ItemModelClient(client.object_instance, false).getItemModelManager.call();
        auto state = maps::ItemRenderState::new_object(&maps::ItemRenderState::constructor);
        auto gui = maps::ItemDisplayContext{}.GUI.get();
        if (!manager || !state || !gui || clear_exception()) return {};
        manager.update.call(state, stack, gui, player);
        const int count = state.layerCount.get();
        auto layers = state.layers.get();
        if (clear_exception() || !layers || count < 0 || count > 32 || count > layers.get_length()) return {};
        for (int index = 0; index < count; ++index)
        {
            jni::frame layer_frame{48};
            maps::ItemRenderLayer layer(jni::get_env()->GetObjectArrayElement(static_cast<jobjectArray>(layers.object_instance), index), false);
            auto tint_array = layer.tints.get();
            if (clear_exception()) return {};
#if ONESTEP_MINECRAFT_26_1
            std::vector<jint> tints;
            if (tint_array)
            {
                const int tint_count = tint_array.size.call();
                if (clear_exception() || tint_count < 0 || tint_count > 4096) return {};
                tints.reserve(static_cast<std::size_t>(tint_count));
                for (int tint_index = 0; tint_index < tint_count; ++tint_index)
                {
                    tints.push_back(tint_array.getInt.call(tint_index));
                    if (clear_exception()) return {};
                }
            }
#else
            if (tint_array && tint_array.get_length() > 4096) return {};
            const auto tints = tint_array ? tint_array.to_vector() : std::vector<jint>{};
#endif
            auto quads = layer.getQuads.call();
            auto transform = layer.transform.get();
            const bool side_lit = layer.useLight.get() == JNI_TRUE;
#if ONESTEP_MINECRAFT_26_1
            auto local_transform = layer.localTransform.get();
            auto floats = jni::array<jfloat>(jni::get_env()->NewFloatArray(16));
            if (!local_transform || !floats || clear_exception()) return {};
            local_transform.get.call(floats);
            if (clear_exception()) return {};
            std::array<float, 16> matrix{};
            jni::get_env()->GetFloatArrayRegion(static_cast<jfloatArray>(floats.object_instance), 0, 16, matrix.data());
            if (clear_exception() || !append(quads, transform, side_lit, textures, tints, output, &matrix)) return {};
#else
            if (clear_exception() || !append(quads, transform, side_lit, textures, tints, output)) return {};
#endif
        }
        return output;
    }
}
