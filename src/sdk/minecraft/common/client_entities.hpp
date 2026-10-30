#pragma once
// Shared SDK layout; target-specific JNI differences are selected at compile time.
#include "entities.hpp"

// Shared Fabric bindings with compile-time target profiles.
namespace maps
{
    BEGIN_KLASS_DEF(MovementVector, ONESTEP_MC_NAME("net/minecraft/class_241"))
        jni::field<jfloat, ONESTEP_MC_NAME("field_1343")> x{*this};
        jni::field<jfloat, ONESTEP_MC_NAME("field_1342")> y{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(PlayerInputFlags, ONESTEP_MC_NAME("net/minecraft/class_10185"))
        jni::method<jboolean, ONESTEP_MC_NAME("comp_3163")> jump{*this};
    END_KLASS_DEF()

		BEGIN_KLASS_DEF(Input, ONESTEP_MC_NAME("net/minecraft/class_744"))
        jni::method<void, ONESTEP_MC_NAME("method_3129")> tick{*this};
        struct axis
        {
            jni::method<MovementVector, ONESTEP_MC_NAME("method_3128")> vector;
            bool forward;
            bool is_valid() const { return vector.is_valid(); }
            jfloat get() const { auto v = vector.call(); return v ? (forward ? v.y.get() : v.x.get()) : 0.f; }
        };
        axis movementForward{jni::method<MovementVector, ONESTEP_MC_NAME("method_3128")>{*this}, true};
        axis movementSideways{jni::method<MovementVector, ONESTEP_MC_NAME("method_3128")>{*this}, false};
        struct jump_state
        {
            jni::field<PlayerInputFlags, ONESTEP_MC_NAME("field_54155")> flags;
            bool is_valid() const { return flags.is_valid(); }
            jboolean get() const { auto value = flags.get(); return value ? value.jump.call() : JNI_FALSE; }
        } jumping{jni::field<PlayerInputFlags, ONESTEP_MC_NAME("field_54155")>{*this}};
	END_KLASS_DEF()

        BEGIN_KLASS_DEF(Identifier, ONESTEP_MC_NAME("net/minecraft/class_2960"))
        jni::method<String, ONESTEP_MC_NAME("method_12832")> getPath{ *this };
    jni::method<String, ONESTEP_MC_NAME("method_12836")> getNamespace{ *this };
    jni::method<String, "toString", jni::NOT_STATIC> toString{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Texture, ONESTEP_MC_NAME("net/minecraft/class_1044"))
        struct gl_id
        {
            const jni::empty_members& owner;
            bool is_valid() const;
            jint call() const;
        } getGlId{*this};
        END_KLASS_DEF()

    BEGIN_KLASS_DEF(GpuTexture, "com/mojang/blaze3d/textures/GpuTexture")
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(GlTexture, ONESTEP_MC_NAME("net/minecraft/class_10868"))
        jni::method<jint, ONESTEP_MC_NAME("method_68427")> getGlId{*this};
    END_KLASS_DEF()
    BEGIN_KLASS_DEF(TextureAccess, ONESTEP_MC_NAME("net/minecraft/class_1044"))
        jni::method<GpuTexture, ONESTEP_MC_NAME("method_68004")> getGpuTexture{*this};
    END_KLASS_DEF()
    inline bool Texture_members::gl_id::is_valid() const
    {
        return TextureAccess(owner.object_instance, false).getGpuTexture.is_valid();
    }
    inline jint Texture_members::gl_id::call() const
    {
        auto gpu = TextureAccess(owner.object_instance, false).getGpuTexture.call();
        if (!gpu || !gpu.is_instance_of<GlTexture>()) return 0;
        return GlTexture(gpu.object_instance, false).getGlId.call();
    }

    BEGIN_KLASS_DEF(SkinTextureAsset, ONESTEP_MC_NAME("net/minecraft/class_12079$class_12081"))
        jni::method<Identifier, ONESTEP_MC_NAME("comp_3627")> texturePath{*this};
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(SkinTextures, ONESTEP_MC_NAME("net/minecraft/class_8685"))
        jni::method<SkinTextureAsset, ONESTEP_MC_NAME("comp_1626")> body{*this};
        struct texture_path
        {
            const SkinTextures_members& owner;
            Identifier call() const { auto asset = owner.body.call(); return asset ? asset.texturePath.call() : Identifier{}; }
        } texture{*this};
        END_KLASS_DEF()

	BEGIN_KLASS_DEF_EX(Registry, ONESTEP_MC_NAME("net/minecraft/class_2378"), Iterable)
		jni::method<Identifier, ONESTEP_MC_NAME("method_10221"), jni::NOT_STATIC, Object> getId{ *this };
	END_KLASS_DEF()

	BEGIN_KLASS_DEF_EX(DefaultedRegistry, ONESTEP_MC_NAME("net/minecraft/class_7922"), Registry)
	END_KLASS_DEF()

	BEGIN_KLASS_DEF(Registries, ONESTEP_MC_NAME("net/minecraft/class_7923"))
		jni::field<DefaultedRegistry, ONESTEP_MC_NAME("field_41175"), jni::STATIC> BLOCK{ *this };
	END_KLASS_DEF()

        BEGIN_KLASS_DEF(TextureManager, ONESTEP_MC_NAME("net/minecraft/class_1060"))
        jni::method<Texture, ONESTEP_MC_NAME("method_4619"), jni::NOT_STATIC, Identifier> getTexture{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(AbstractClientPlayerEntity, ONESTEP_MC_NAME("net/minecraft/class_742"), PlayerEntity)
        jni::method<SkinTextures, ONESTEP_MC_NAME("method_52814")> getSkinTextures{ *this };
    END_KLASS_DEF()

		BEGIN_KLASS_DEF_EX(ClientPlayerEntity, ONESTEP_MC_NAME("net/minecraft/class_746"), AbstractClientPlayerEntity)
		jni::field<Input, ONESTEP_MC_NAME("field_3913")> input{ *this };
		maps::PlayerEntity toPlayerEntity() const
    {
        return maps::PlayerEntity(this->object_instance, this->is_global());
    }
    END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(MobEntity, ONESTEP_MC_NAME("net/minecraft/class_1308"), LivingEntity)
        END_KLASS_DEF()

		BEGIN_KLASS_DEF_EX(ZombieEntity, ONESTEP_MC_NAME("net/minecraft/class_1642"), MobEntity)
		END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(AnimalEntity, ONESTEP_MC_NAME("net/minecraft/class_1429"), MobEntity)
        END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(ClientWorld, ONESTEP_MC_NAME("net/minecraft/class_638"), World)
        jni::field<List, ONESTEP_MC_NAME("field_18226")> players{ *this };
    jni::method<List, ONESTEP_MC_NAME("method_18456")> getPlayers{ *this };
    jni::method<Iterable, ONESTEP_MC_NAME("method_18112")> getEntities{ *this };
    jni::method<Entity, ONESTEP_MC_NAME("method_8469"), jni::NOT_STATIC, jint> getEntityById{ *this };
    jni::method<ClientChunkManager, ONESTEP_MC_NAME("method_2935")> getChunkManager{ *this };
    END_KLASS_DEF()

		BEGIN_KLASS_DEF_EX(OtherClientPlayerEntity, ONESTEP_MC_NAME("net/minecraft/class_745"), AbstractClientPlayerEntity)
		jni::constructor<ClientWorld, GameProfile> constructor{ *this };
		END_KLASS_DEF()

}
