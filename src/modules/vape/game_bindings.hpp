#pragma once
#include "../../mappings.hpp"

// Minecraft bindings used by the retained Aim Assist adapter.
namespace vape_bindings
{
    BEGIN_KLASS_DEF_EX(Player, ONESTEP_MC_NAME("net/minecraft/class_1657"), maps::PlayerEntity)
        jni::method<jint, ONESTEP_MC_NAME("method_6096")> getArmor{*this};
        jni::method<jboolean, ONESTEP_MC_NAME("method_6057"), jni::NOT_STATIC, maps::Entity> canSee{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF_EX(Manager, ONESTEP_MC_NAME("net/minecraft/class_636"), maps::ClientPlayerInteractionManager)
        jni::method<jboolean, ONESTEP_MC_NAME("method_2923")> breaking{*this};
    END_KLASS_DEF()
}
