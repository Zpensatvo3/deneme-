#pragma once
#include "../../mappings.hpp"

// UI-only, read-only registry/stack views. Kept out of gameplay SDK wrappers.
// Yarn Item.getDefaultStack / Identifier.tryParse / Registry.get(Identifier).
namespace modules::block_icon_bindings
{
    BEGIN_KLASS_DEF(Item, ONESTEP_MC_NAME("net/minecraft/class_1792"))
#if ONESTEP_MINECRAFT_26_1
        jni::method<maps::ItemStack, "getDefaultInstance"> default_stack{*this};
#else
        jni::method<maps::ItemStack, "method_7854"> default_stack{*this};
#endif
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Identifier, ONESTEP_MC_NAME("net/minecraft/class_2960"))
#if ONESTEP_MINECRAFT_26_1
        jni::method<maps::Identifier, "tryParse", jni::STATIC, maps::String> parse{*this};
#else
        jni::method<maps::Identifier, "method_12829", jni::STATIC, maps::String> parse{*this};
#endif
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(Registry, ONESTEP_MC_NAME("net/minecraft/class_2378"))
#if ONESTEP_MINECRAFT_26_1
        jni::method<maps::Object, "getValue", jni::NOT_STATIC, maps::Identifier> get{*this};
#else
        jni::method<maps::Object, "method_63535", jni::NOT_STATIC, maps::Identifier> get{*this};
#endif
    END_KLASS_DEF()
}
