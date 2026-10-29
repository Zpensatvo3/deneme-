#pragma once
// Shared SDK layout; target-specific JNI differences are selected at compile time.
#include "java_types.hpp"

// Shared Fabric bindings with compile-time target profiles.
namespace maps
{
        BEGIN_KLASS_DEF(Vec3d, ONESTEP_MC_NAME("net/minecraft/class_243"))
        jni::constructor<jdouble, jdouble, jdouble> constructor{ *this };
        jni::field<jdouble, ONESTEP_MC_NAME("field_1352")> x{ *this };
    jni::field<jdouble, ONESTEP_MC_NAME("field_1351")> y{ *this };
    jni::field<jdouble, ONESTEP_MC_NAME("field_1350")> z{ *this };

    maths::vector3d to_vector3d()
    {
        return { x.get(), y.get(), z.get() };
    }
    END_KLASS_DEF()

	KLASS_DECLARATION(BlockHitResult, ONESTEP_MC_NAME("net/minecraft/class_3965"))
	KLASS_DECLARATION(RaycastContext, ONESTEP_MC_NAME("net/minecraft/class_3959"))

	BEGIN_KLASS_DEF(RaycastContext$ShapeType, ONESTEP_MC_NAME("net/minecraft/class_3959$class_3960"))
		jni::field<RaycastContext$ShapeType, ONESTEP_MC_NAME("field_17558"), jni::STATIC> COLLIDER{ *this };
	END_KLASS_DEF()

	BEGIN_KLASS_DEF(RaycastContext$FluidHandling, ONESTEP_MC_NAME("net/minecraft/class_3959$class_242"))
		jni::field<RaycastContext$FluidHandling, ONESTEP_MC_NAME("field_1348"), jni::STATIC> NONE{ *this };
	END_KLASS_DEF()

}
