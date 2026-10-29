#pragma once
// Shared SDK layout; target-specific JNI differences are selected at compile time.
#include "client_entities.hpp"
#include <bit>

// Read-only model/atlas access. No Java render calls or GL state changes.
namespace maps
{
    BEGIN_KLASS_DEF(ModelVector3f, "org/joml/Vector3f")
        jni::field<jfloat, "x"> x{*this};
        jni::field<jfloat, "y"> y{*this};
        jni::field<jfloat, "z"> z{*this};
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ModelVector3fc, "org/joml/Vector3fc")
        jni::method<jfloat, "x"> x{*this};
        jni::method<jfloat, "y"> y{*this};
        jni::method<jfloat, "z"> z{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(ItemTransformation, ONESTEP_MC_NAME("net/minecraft/class_804"))
        jni::method<ModelVector3fc, ONESTEP_MC_NAME("comp_3747")> rotation{*this};
        jni::method<ModelVector3fc, ONESTEP_MC_NAME("comp_3748")> translation{*this};
        jni::method<ModelVector3fc, ONESTEP_MC_NAME("comp_3749")> scale{*this};
        std::array<float, 3> rotation_components() const;
        std::array<float, 3> translation_components() const;
        std::array<float, 3> scale_components() const;
    END_KLASS_DEF()

    inline std::array<float, 3> ItemTransformation_members::rotation_components() const
    {
        auto v = rotation.call(); return v ? std::array<float, 3>{v.x.call(), v.y.call(), v.z.call()} : std::array<float, 3>{};
    }
    inline std::array<float, 3> ItemTransformation_members::translation_components() const
    {
        auto v = translation.call(); return v ? std::array<float, 3>{v.x.call(), v.y.call(), v.z.call()} : std::array<float, 3>{};
    }
    inline std::array<float, 3> ItemTransformation_members::scale_components() const
    {
        auto v = scale.call(); return v ? std::array<float, 3>{v.x.call(), v.y.call(), v.z.call()} : std::array<float, 3>{};
    }

#if !ONESTEP_MINECRAFT_26_1
    BEGIN_KLASS_DEF(ItemModelTransformation, ONESTEP_MC_NAME("net/minecraft/class_809"))
    END_KLASS_DEF()
#endif

    BEGIN_KLASS_DEF(ModelRandom, ONESTEP_MC_NAME("net/minecraft/class_5819"))
        jni::method<ModelRandom, ONESTEP_MC_NAME("method_43049"), jni::STATIC, jlong> createSeeded{*this};
        jni::method<void, ONESTEP_MC_NAME("method_43052"), jni::NOT_STATIC, jlong> setSeed{*this};
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(ItemSprite, ONESTEP_MC_NAME("net/minecraft/class_1058"))
        jni::method<Identifier, ONESTEP_MC_NAME("method_45852")> getAtlasId{*this};
    END_KLASS_DEF()

#if ONESTEP_MINECRAFT_26_1
    BEGIN_KLASS_DEF(QuadMaterialInfo, "net/minecraft/client/resources/model/geometry/BakedQuad$MaterialInfo")
        jni::method<jint, "tintIndex"> getColorIndex{*this};
        jni::method<ItemSprite, "sprite"> getSprite{*this};
    END_KLASS_DEF()
#endif
    BEGIN_KLASS_DEF(ItemBakedQuad, ONESTEP_MC_NAME("net/minecraft/class_777"))
        jni::method<ModelVector3fc, ONESTEP_MC_NAME("method_76648"), jni::NOT_STATIC, jint> getPosition{*this};
        jni::method<jlong, ONESTEP_MC_NAME("method_76649"), jni::NOT_STATIC, jint> getTexcoords{*this};
#if ONESTEP_MINECRAFT_26_1
        jni::method<QuadMaterialInfo, "materialInfo"> getMaterial{*this};
#else
        jni::method<jint, ONESTEP_MC_NAME("comp_3722")> getColorIndex{*this};
        jni::method<ItemSprite, ONESTEP_MC_NAME("comp_3724")> getSprite{*this};
#endif
        std::vector<jint> vertex_data() const;
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(PackedUv, ONESTEP_MC_NAME("net/minecraft/class_5611"))
        jni::method<jfloat, ONESTEP_MC_NAME("method_76641"), jni::STATIC, jlong> getX{*this};
        jni::method<jfloat, ONESTEP_MC_NAME("method_76642"), jni::STATIC, jlong> getY{*this};
    END_KLASS_DEF()
    inline std::vector<jint> ItemBakedQuad_members::vertex_data() const
    {
        std::vector<jint> result(32);
        PackedUv uv;
        for (int index = 0; index < 4; ++index)
        {
            auto position = getPosition.call(index);
            if (!position) return {};
            const jlong packed = getTexcoords.call(index);
            result[index * 8] = std::bit_cast<jint>(position.x.call());
            result[index * 8 + 1] = std::bit_cast<jint>(position.y.call());
            result[index * 8 + 2] = std::bit_cast<jint>(position.z.call());
            result[index * 8 + 4] = std::bit_cast<jint>(uv.getX.call(packed));
            result[index * 8 + 5] = std::bit_cast<jint>(uv.getY.call(packed));
        }
        return result;
    }

    BEGIN_KLASS_DEF(ItemDisplayContext, ONESTEP_MC_NAME("net/minecraft/class_811"))
        jni::field<ItemDisplayContext, ONESTEP_MC_NAME("field_4317"), jni::STATIC> GUI{*this};
    END_KLASS_DEF()
#if ONESTEP_MINECRAFT_26_1
    BEGIN_KLASS_DEF(ItemLocalTransform, "org/joml/Matrix4f")
        jni::method<jni::array<jfloat>, "get", jni::NOT_STATIC, jni::array<jfloat>> get{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(ItemTintList, "it/unimi/dsi/fastutil/ints/IntList")
        jni::method<jint, "size"> size{*this};
        jni::method<jint, "getInt", jni::NOT_STATIC, jint> getInt{*this};
    END_KLASS_DEF()
#endif
    BEGIN_KLASS_DEF(ItemRenderLayer, ONESTEP_MC_NAME("net/minecraft/class_10444$class_10446"))
#if ONESTEP_MINECRAFT_26_1
        jni::field<ItemTintList, ONESTEP_MC_NAME("field_55349")> tints{*this};
        jni::field<ItemLocalTransform, "localTransform"> localTransform{*this};
#else
        jni::field<jni::array<jint>, ONESTEP_MC_NAME("field_55349")> tints{*this};
#endif
        jni::field<ItemTransformation, ONESTEP_MC_NAME("field_56967")> transform{*this};
        jni::field<jboolean, ONESTEP_MC_NAME("field_56965")> useLight{*this};
        jni::method<List, ONESTEP_MC_NAME("method_67997")> getQuads{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(ItemRenderState, ONESTEP_MC_NAME("net/minecraft/class_10444"))
        jni::constructor<> constructor{*this};
        jni::field<jint, ONESTEP_MC_NAME("field_55339")> layerCount{*this};
        jni::field<jni::array<ItemRenderLayer>, ONESTEP_MC_NAME("field_55340")> layers{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(ItemModelManager, ONESTEP_MC_NAME("net/minecraft/class_10442"))
        jni::method<void, ONESTEP_MC_NAME("method_65597"), jni::NOT_STATIC, ItemRenderState, ItemStack, ItemDisplayContext, LivingEntity> update{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(ItemModelClient, ONESTEP_MC_NAME("net/minecraft/class_310"))
        jni::method<ItemModelManager, ONESTEP_MC_NAME("method_65386")> getItemModelManager{*this};
    END_KLASS_DEF()
}
