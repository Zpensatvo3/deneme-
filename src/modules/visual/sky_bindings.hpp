#pragma once
#include "../../mappings.hpp"

// Private biome/dimension bindings. No world generation or render hooks.
// https://maven.fabricmc.net/docs/yarn-1.21.1+build.3/net/minecraft/world/biome/BiomeEffects.html
namespace sky_bindings
{
    BEGIN_KLASS_DEF(Effects, ONESTEP_MC_NAME("net/minecraft/class_4763"))
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Biome, ONESTEP_MC_NAME("net/minecraft/class_1959"))
        jni::method<Effects,ONESTEP_MC_NAME("method_24377")> getEffects{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Key, ONESTEP_MC_NAME("net/minecraft/class_5321"))
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Keys, ONESTEP_MC_NAME("net/minecraft/class_7924"))
        jni::field<Key,ONESTEP_MC_NAME("field_41236"),jni::STATIC> BIOME{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Registry, ONESTEP_MC_NAME("net/minecraft/class_2378"))
        jni::method<maps::Iterator,"iterator"> iterator{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Manager, ONESTEP_MC_NAME("net/minecraft/class_5455"))
        jni::method<Registry,ONESTEP_MC_NAME("method_30530"),jni::NOT_STATIC,Key> get{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Dimension, ONESTEP_MC_NAME("net/minecraft/class_2874"))
        jni::method<jboolean,ONESTEP_MC_NAME("comp_642")> hasSkyLight{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Submersion, ONESTEP_MC_NAME("net/minecraft/class_5636"))
        jni::field<Submersion,ONESTEP_MC_NAME("field_27888"),jni::STATIC> NONE{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Camera, ONESTEP_MC_NAME("net/minecraft/class_4184"))
        jni::method<Submersion,ONESTEP_MC_NAME("method_19334")> getSubmersionType{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(World, ONESTEP_MC_NAME("net/minecraft/class_1937"))
        jni::method<Manager,ONESTEP_MC_NAME("method_30349")> getRegistryManager{*this};
        jni::method<Dimension,ONESTEP_MC_NAME("method_8597")> getDimension{*this};
    END_KLASS_DEF()
}
