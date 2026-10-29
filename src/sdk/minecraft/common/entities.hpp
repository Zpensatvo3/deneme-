#pragma once
// Shared SDK layout; target-specific JNI differences are selected at compile time.
#include "entity_types.hpp"

// Shared Fabric bindings with compile-time target profiles.
namespace maps
{
		BEGIN_KLASS_DEF(Entity, ONESTEP_MC_NAME("net/minecraft/class_1297"))
		jni::method<jfloat, ONESTEP_MC_NAME("method_5739"), jni::NOT_STATIC, Entity> distanceTo{ *this };
		jni::method<jint, ONESTEP_MC_NAME("method_5628")> getId{ *this };
        jni::method<World, bindings_profile::entity_world> getWorld{ *this };
    jni::field<jint, ONESTEP_MC_NAME("field_6012")> age{ *this };
    jni::field<Vec3d, ONESTEP_MC_NAME("field_22467")> pos{ *this };
    jni::field<jfloat, ONESTEP_MC_NAME("field_6031")> yaw{ *this };
    jni::field<jfloat, ONESTEP_MC_NAME("field_5965")> pitch{ *this };
    jni::field<jfloat, ONESTEP_MC_NAME("field_5982")> prevYaw{ *this };
    jni::field<jfloat, ONESTEP_MC_NAME("field_6004")> prevPitch{ *this };
    jni::field<Box, ONESTEP_MC_NAME("field_6005")> boundingBox{ *this };
    jni::method<Box, ONESTEP_MC_NAME("method_5829")> getBoundingBox{ *this };
    jni::method<Vec3d, bindings_profile::entity_position> getPos{ *this };
    jni::method<jdouble, ONESTEP_MC_NAME("method_23320")> getEyeY{ *this };
    jni::method<jfloat, ONESTEP_MC_NAME("method_5751")> getStandingEyeHeight{ *this };
    jni::field<bindings_profile::fall_distance, ONESTEP_MC_NAME("field_6017")> fallDistance{ *this };
    jni::field<jboolean, ONESTEP_MC_NAME("field_5952")> onGround{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_24828")> isOnGround{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_5809")> isOnFire{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_5799")> isTouchingWater{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_5771")> isInLava{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_5757")> isInsideWall{ *this };
    jni::field<jdouble, ONESTEP_MC_NAME("field_6038")> lastRenderX{ *this };
    jni::field<jdouble, ONESTEP_MC_NAME("field_5971")> lastRenderY{ *this };
    jni::field<jdouble, ONESTEP_MC_NAME("field_5989")> lastRenderZ{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_5767")> isInvisible{ *this };

    maths::vector3d get_prev_position() const
    {
        return { lastRenderX.get(), lastRenderY.get(), lastRenderZ.get() };
    }

    maps::Box get_bounding_box() const
    {
        if (boundingBox.is_valid())
        {
            maps::Box b = boundingBox.get();
            if (b) return b;
        }
        if (getBoundingBox.is_valid())
        {
            return getBoundingBox.call();
        }
        return maps::Box{ nullptr };
    }

    maths::vector3d get_position() const
    {
        if (pos.is_valid())
        {
            maps::Vec3d p = pos.get();
            if (p) return p.to_vector3d();
        }
        if (getPos.is_valid())
        {
            maps::Vec3d p = getPos.call();
            if (p) return p.to_vector3d();
        }
        return { 0, 0, 0 };
    }

    maths::angles get_prev_angles() const
    {
        return { prevYaw.get(), prevPitch.get() };
    }

    maths::angles get_angles() const
    {
        return { yaw.get(), pitch.get() };
    }

    jni::method<EntityPose, ONESTEP_MC_NAME("method_18376")> getPose{ *this };
    jni::method<EntityDimensions, ONESTEP_MC_NAME("method_18377"), jni::NOT_STATIC, EntityPose> getDimensions{ *this };
    jni::method<jfloat, ONESTEP_MC_NAME("method_18381"), jni::NOT_STATIC, EntityPose> getEyeHeight{ *this };
    jni::method<Text, ONESTEP_MC_NAME("method_23315")> getDefaultName{ *this };
    jni::method<String, ONESTEP_MC_NAME("method_5820")> getEntityName{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_5805")> isAlive{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_7325")> isSpectator{ *this };
    jni::method<HitResult, ONESTEP_MC_NAME("method_5745"), jni::NOT_STATIC, jdouble, jfloat, jboolean> raycast{ *this };
		jni::method<jboolean, ONESTEP_MC_NAME("method_5624"), jni::NOT_STATIC> isSprinting{ *this };
		jni::method<jboolean, ONESTEP_MC_NAME("method_5765"), jni::NOT_STATIC> hasVehicle{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_5728"), jni::NOT_STATIC, jboolean> setSprinting{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_5660"), jni::NOT_STATIC, jboolean> setSneaking{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_5834"), jni::NOT_STATIC, jboolean> setGlowing{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_5729"), jni::NOT_STATIC, jint, jboolean> setFlag{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_5681")> isSwimming{ *this };
    jni::method<Vec3d, ONESTEP_MC_NAME("method_18798")> getVelocity{ *this };
		jni::method<void, ONESTEP_MC_NAME("method_18800"), jni::NOT_STATIC, jdouble, jdouble, jdouble> setVelocity{ *this };
		jni::method<void, ONESTEP_MC_NAME("method_5814"), jni::NOT_STATIC, jdouble, jdouble, jdouble> setPosition{ *this };
		jni::method<void, ONESTEP_MC_NAME("method_5808"), jni::NOT_STATIC, jdouble, jdouble, jdouble, jfloat, jfloat> refreshPositionAndAngles{ *this };
    jni::method<jfloat, ONESTEP_MC_NAME("method_17681")> getWidth{ *this };
    jni::method<jfloat, ONESTEP_MC_NAME("method_17682")> getHeight{ *this };
    jni::method<Text, ONESTEP_MC_NAME("method_5797")> getCustomName{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_5807")> isCustomNameVisible{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_5880"), jni::NOT_STATIC, jboolean> setCustomNameVisible{ *this };
        jni::method<Vec3d, ONESTEP_MC_NAME("method_30950"), jni::NOT_STATIC, jfloat> getLerpedPos{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF_EX(ItemEntity, ONESTEP_MC_NAME("net/minecraft/class_1542"), Entity)
        jni::method<ItemStack, ONESTEP_MC_NAME("method_6983")> getStack{ *this };
    END_KLASS_DEF()

	BEGIN_KLASS_MEMBERS(RaycastContext)
		jni::constructor<Vec3d, Vec3d, RaycastContext$ShapeType,
			RaycastContext$FluidHandling, Entity> constructor{ *this };
	END_KLASS_MEMBERS()

	BEGIN_KLASS_DEF_EX(ArmorStandEntity, ONESTEP_MC_NAME("net/minecraft/class_1531"), Entity)
	END_KLASS_DEF()



        BEGIN_KLASS_DEF(DamageSource, ONESTEP_MC_NAME("net/minecraft/class_1282"))
        jni::method<Entity, ONESTEP_MC_NAME("method_5529")> getAttacker{ *this };
    END_KLASS_DEF()

		BEGIN_KLASS_DEF_EX(LivingEntity, ONESTEP_MC_NAME("net/minecraft/class_1309"), Entity)
    jni::field<jint, ONESTEP_MC_NAME("field_6273")> lastAttackedTicks{ *this };
    jni::field<jint, ONESTEP_MC_NAME("field_6235")> hurtTime{ *this };
    jni::field<jint, ONESTEP_MC_NAME("field_6254")> maxHurtTime{ *this };
    jni::field<jint, ONESTEP_MC_NAME("field_6228")> jumpingCooldown{ *this };
    // Intermediary mappings.
    jni::method<jfloat, ONESTEP_MC_NAME("method_6032")> getHealth{ *this };
    jni::method<jfloat, ONESTEP_MC_NAME("method_6063")> getMaxHealth{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_6128")> isFallFlying{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_6113")> isSleeping{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_6115")> isUsingItem{ *this };
	jni::method<jint, ONESTEP_MC_NAME("method_6048")> getItemUseTime{ *this };
	jni::method<ItemStack, ONESTEP_MC_NAME("method_6030")> getActiveItem{ *this };
		jni::method<jboolean, ONESTEP_MC_NAME("method_6039")> isBlocking{ *this };
		jni::method<jboolean, ONESTEP_MC_NAME("method_6101")> isClimbing{ *this };
#if ONESTEP_MINECRAFT_26_1
    jni::method<LivingEntity, ONESTEP_MC_NAME("method_6065")> getAttacker{*this};
#else
    jni::method<Entity, ONESTEP_MC_NAME("method_6065")> getAttacker{ *this };
#endif
    jni::method<DamageSource, ONESTEP_MC_NAME("method_6081")> getRecentDamageSource{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_6043")> jump{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_24518"), jni::NOT_STATIC, Item> isHolding{ *this };
    jni::method<ItemStack, ONESTEP_MC_NAME("method_6047")> getMainHandStack{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_6092"), jni::NOT_STATIC, StatusEffectInstance> addStatusEffect{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_6016"), jni::NOT_STATIC, EffectBinding> removeStatusEffect{ *this };
		jni::method<jboolean, ONESTEP_MC_NAME("method_6059"), jni::NOT_STATIC, EffectBinding> hasStatusEffect{ *this };
		jni::method<jboolean, ONESTEP_MC_NAME("method_6059"), jni::NOT_STATIC, RegistryEntry> hasStatusEffectEntry{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_6122"), jni::NOT_STATIC, Hand, ItemStack> setStackInHand{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_6104"), jni::NOT_STATIC, Hand> swingHand{ *this };
    END_KLASS_DEF()

		BEGIN_KLASS_DEF_EX(PlayerEntity, ONESTEP_MC_NAME("net/minecraft/class_1657"), LivingEntity)
        jni::method<jfloat, ONESTEP_MC_NAME("method_7261"), jni::NOT_STATIC, jfloat> getAttackCooldownProgress{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_7350"), jni::NOT_STATIC> resetLastAttackedTicks{ *this };
    // Vanilla interaction ranges; delivery checks never extend reach.
    jni::method<jdouble, ONESTEP_MC_NAME("method_55754")> getBlockInteractionRange{ *this };
    jni::method<jdouble, ONESTEP_MC_NAME("method_55755")> getEntityInteractionRange{ *this };
    jni::method<GameProfile, ONESTEP_MC_NAME("method_7334")> getGameProfile{ *this };
    jni::method<ItemStack, ONESTEP_MC_NAME("method_6079"), jni::NOT_STATIC> getOffHandStack{ *this };
    jni::method<PlayerInventory, ONESTEP_MC_NAME("method_31548")> getInventory{ *this };
    jni::method<PlayerAbilities, ONESTEP_MC_NAME("method_31549")> getAbilities{ *this };
    jni::field<ScreenHandler, ONESTEP_MC_NAME("field_7512")> currentScreenHandler{ *this };
    jni::method<jdouble, ONESTEP_MC_NAME("method_5858"), jni::NOT_STATIC, Entity> squaredDistanceTo{ *this };
    END_KLASS_DEF()

}
