#pragma once
// Shared SDK layout; target-specific JNI differences are selected at compile time.
#include "inventory_network.hpp"

// Shared Fabric bindings with compile-time target profiles.
namespace maps
{
        BEGIN_KLASS_DEF(Box, ONESTEP_MC_NAME("net/minecraft/class_238"))
        jni::field<jdouble, ONESTEP_MC_NAME("field_1323")> minX{ *this };
    jni::field<jdouble, ONESTEP_MC_NAME("field_1322")> minY{ *this };
    jni::field<jdouble, ONESTEP_MC_NAME("field_1321")> minZ{ *this };
    jni::field<jdouble, ONESTEP_MC_NAME("field_1320")> maxX{ *this };
    jni::field<jdouble, ONESTEP_MC_NAME("field_1325")> maxY{ *this };
    jni::field<jdouble, ONESTEP_MC_NAME("field_1324")> maxZ{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(EntityPose, ONESTEP_MC_NAME("net/minecraft/class_4050"))
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(EntityDimensions, ONESTEP_MC_NAME("net/minecraft/class_4048"))
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(StatusEffect, ONESTEP_MC_NAME("net/minecraft/class_1291"))
        END_KLASS_DEF()

#if ONESTEP_MINECRAFT_26_1
    using EffectBinding = RegistryEntry;
#else
    using EffectBinding = StatusEffect;
#endif

	BEGIN_KLASS_DEF(StatusEffects, ONESTEP_MC_NAME("net/minecraft/class_1294"))
		jni::field<RegistryEntry, ONESTEP_MC_NAME("field_5919"), jni::STATIC> BLINDNESS{ *this };
		jni::field<EffectBinding, ONESTEP_MC_NAME("field_5912"), jni::STATIC> GLOWING{ *this };
    jni::field<EffectBinding, ONESTEP_MC_NAME("field_5925"), jni::STATIC> NIGHT_VISION{ *this };
    jni::field<EffectBinding, ONESTEP_MC_NAME("field_5918"), jni::STATIC> FIRE_RESISTANCE{ *this };
    jni::field<EffectBinding, ONESTEP_MC_NAME("field_5924"), jni::STATIC> REGENERATION{ *this };
    jni::field<EffectBinding, ONESTEP_MC_NAME("field_5917"), jni::STATIC> HASTE{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(StatusEffectInstance, ONESTEP_MC_NAME("net/minecraft/class_1293"))
        jni::constructor<EffectBinding, jint, jint> constructor{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PlayerAbilities, ONESTEP_MC_NAME("net/minecraft/class_1656"))
        jni::field<jboolean, ONESTEP_MC_NAME("field_7477")> creativeMode{ *this };
    jni::field<jboolean, ONESTEP_MC_NAME("field_7478")> allowFlying{ *this };
    jni::field<jboolean, ONESTEP_MC_NAME("field_7479")> flying{ *this };
    jni::field<jfloat, ONESTEP_MC_NAME("field_7481")> flySpeed{ *this };
    jni::field<jfloat, ONESTEP_MC_NAME("field_7482")> walkSpeed{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(OrderedText, ONESTEP_MC_NAME("net/minecraft/class_5481"))
        END_KLASS_DEF()

        BEGIN_KLASS_MEMBERS(Text)
        jni::method<Text, ONESTEP_MC_NAME("method_30163"), jni::STATIC, String> of{ *this };
        jni::method<String, "getString"> getString{ *this };
        jni::method<String, ONESTEP_MC_NAME("method_54160")> getLiteralString{ *this };
    jni::method<OrderedText, ONESTEP_MC_NAME("method_30937"), jni::NOT_STATIC> asOrderedText{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Hand, ONESTEP_MC_NAME("net/minecraft/class_1268"))
        jni::field<Hand, ONESTEP_MC_NAME("field_5808"), jni::STATIC> MAIN_HAND{ *this };
    jni::field<Hand, ONESTEP_MC_NAME("field_5810"), jni::STATIC> OFF_HAND{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PlayerInteractItemC2SPacket, ONESTEP_MC_NAME("net/minecraft/class_2886"))
#if ONESTEP_MINECRAFT_26_1
        jni::constructor<maps::Hand, jint, jfloat, jfloat> constructor{*this};
#else
        jni::constructor<maps::Hand, jint> constructor{ *this };
#endif
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(HitResult$Type, ONESTEP_MC_NAME("net/minecraft/class_239$class_240"))
        jni::field<HitResult$Type, ONESTEP_MC_NAME("field_1331"), jni::STATIC> ENTITY{ *this };
    jni::field<HitResult$Type, ONESTEP_MC_NAME("field_1332"), jni::STATIC> BLOCK{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(HitResult, ONESTEP_MC_NAME("net/minecraft/class_239"))
        jni::method<HitResult$Type, ONESTEP_MC_NAME("method_17783")> getType{ *this };
    jni::method<Vec3d, ONESTEP_MC_NAME("method_17784")> getPos{ *this };
    END_KLASS_DEF()

}
