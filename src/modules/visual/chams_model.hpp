#pragma once
#include "../../mappings.hpp"

// Private bindings for an independent model and owned, version-specific pose state.
namespace chams_model
{
    BEGIN_KLASS_DEF(Cuboid, ONESTEP_MC_NAME("net/minecraft/class_630$class_628"))
        jni::field<jfloat, ONESTEP_MC_NAME("field_3645")> minX{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_3644")> minY{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_3643")> minZ{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_3648")> maxX{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_3647")> maxY{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_3646")> maxZ{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Part, ONESTEP_MC_NAME("net/minecraft/class_630"))
        jni::method<Part, ONESTEP_MC_NAME("method_32086"), jni::NOT_STATIC, maps::String> getChild{*this};
        jni::field<maps::List, ONESTEP_MC_NAME("field_3663")> cuboids{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_3657")> pivotX{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_3656")> pivotY{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_3655")> pivotZ{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_3654")> pitch{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_3675")> yaw{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_3674")> roll{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Layer, ONESTEP_MC_NAME("net/minecraft/class_5601"))
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Layers, ONESTEP_MC_NAME("net/minecraft/class_5602"))
        jni::field<Layer, ONESTEP_MC_NAME("field_27577"), jni::STATIC> PLAYER{*this};
        jni::field<Layer, ONESTEP_MC_NAME("field_27581"), jni::STATIC> PLAYER_SLIM{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Map, "java/util/Map")
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Loader, ONESTEP_MC_NAME("net/minecraft/class_5599"))
        jni::method<Part, ONESTEP_MC_NAME("method_32072"), jni::NOT_STATIC, Layer> getModelPart{*this};
        jni::field<Map, ONESTEP_MC_NAME("field_27542")> modelParts{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Client, ONESTEP_MC_NAME("net/minecraft/class_310"))
        jni::method<Loader, ONESTEP_MC_NAME("method_31974")> getEntityModelLoader{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(RenderState, ONESTEP_MC_NAME("net/minecraft/class_10017"))
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(PlayerPose, ONESTEP_MC_NAME("net/minecraft/class_10055"))
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(EntityRenderer, ONESTEP_MC_NAME("net/minecraft/class_897"))
        jni::method<RenderState, ONESTEP_MC_NAME("method_55269")> createRenderState{*this};
        jni::method<void, ONESTEP_MC_NAME("method_62354"), jni::NOT_STATIC, maps::Entity, RenderState, jfloat> updateRenderState{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Dispatcher, ONESTEP_MC_NAME("net/minecraft/class_898"))
        jni::method<EntityRenderer, ONESTEP_MC_NAME("method_3953"), jni::NOT_STATIC, maps::Entity> getRenderer{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Model, ONESTEP_MC_NAME("net/minecraft/class_591"))
        jni::constructor<Part, jboolean> constructor{*this};
        jni::method<void, ONESTEP_MC_NAME("method_62110"), jni::NOT_STATIC, PlayerPose> setAngles{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Limb, ONESTEP_MC_NAME("net/minecraft/class_8080"))
        jni::method<jfloat, ONESTEP_MC_NAME("method_48572"), jni::NOT_STATIC, jfloat> getPos{*this};
        jni::method<jfloat, ONESTEP_MC_NAME("method_48570"), jni::NOT_STATIC, jfloat> getSpeed{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Living, ONESTEP_MC_NAME("net/minecraft/class_1309"))
        jni::field<Limb, ONESTEP_MC_NAME("field_42108")> limbAnimator{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_6283")> bodyYaw{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_6220")> prevBodyYaw{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_6241")> headYaw{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_6259")> prevHeadYaw{*this};
        jni::method<jfloat, ONESTEP_MC_NAME("method_6055"), jni::NOT_STATIC, jfloat> getHandSwingProgress{*this};
        jni::method<jboolean, ONESTEP_MC_NAME("method_6057"), jni::NOT_STATIC, maps::Entity> canSee{*this};
        jni::method<jboolean, ONESTEP_MC_NAME("method_18276")> isInSneakingPose{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(SkinModel, maps::bindings_profile::skin_model)
        jni::method<maps::String, "name"> name{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Skin, ONESTEP_MC_NAME("net/minecraft/class_8685"))
        jni::method<SkinModel, ONESTEP_MC_NAME("comp_1629")> model{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(PlayerSkin, ONESTEP_MC_NAME("net/minecraft/class_742"))
        jni::method<Skin, ONESTEP_MC_NAME("method_52814")> getSkinTextures{*this};
    END_KLASS_DEF()
}
