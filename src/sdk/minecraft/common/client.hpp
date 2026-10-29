#pragma once
// Shared SDK layout; target-specific JNI differences are selected at compile time.
#include "input.hpp"
#include "item_render.hpp"

// Shared Fabric bindings with compile-time target profiles.
namespace maps
{
	BEGIN_KLASS_DEF(ActionResult, ONESTEP_MC_NAME("net/minecraft/class_1269"))
        jni::method<jboolean, ONESTEP_MC_NAME("method_23665")> isAccepted{ *this };
	END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(EntityHitResult, ONESTEP_MC_NAME("net/minecraft/class_3966"), HitResult)
        jni::field<Entity, ONESTEP_MC_NAME("field_17592")> entity{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_MEMBERS(BlockHitResult)
        jni::constructor<Vec3d, Direction, BlockPos, jboolean> constructor{ *this };
        jni::method<Direction, ONESTEP_MC_NAME("method_17780"), jni::NOT_STATIC> getSide{ *this };
    jni::method<BlockPos, ONESTEP_MC_NAME("method_17777"), jni::NOT_STATIC> getBlockPos{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(ClientPlayerInteractionManager, ONESTEP_MC_NAME("net/minecraft/class_636"))
        jni::method<void, ONESTEP_MC_NAME("method_2918"), jni::NOT_STATIC, PlayerEntity, Entity> attackEntity{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_2910"), jni::NOT_STATIC, BlockPos, Direction> attackBlock{ *this };
    jni::method<ActionResult, ONESTEP_MC_NAME("method_2896"), jni::NOT_STATIC, ClientPlayerEntity, Hand, BlockHitResult> interactBlock{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_2906"), jni::NOT_STATIC, jint, jint, jint, SlotActionType, PlayerEntity> clickSlot{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_2911"), jni::NOT_STATIC> syncSelectedSlot{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PlayerInteractBlockC2SPacket, ONESTEP_MC_NAME("net/minecraft/class_2885"))
        jni::constructor<Hand, BlockHitResult, jint> constructor{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(MatrixStack, ONESTEP_MC_NAME("net/minecraft/class_4587"))
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(Window, ONESTEP_MC_NAME("net/minecraft/class_1041"))
    jni::method<jlong, ONESTEP_MC_NAME("method_4490")> getHandle{ *this };
        jni::method<jint, ONESTEP_MC_NAME("method_4486")> getScaledWidth{ *this };
    jni::method<jint, ONESTEP_MC_NAME("method_4502")> getScaledHeight{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(TextRenderer, ONESTEP_MC_NAME("net/minecraft/class_327"))
        jni::method<jint, ONESTEP_MC_NAME("method_30880"), jni::NOT_STATIC, String> getWidth{ *this };
#if !ONESTEP_MINECRAFT_26_1
    jni::method<jint, "method_30881", jni::NOT_STATIC, MatrixStack, String, jfloat, jfloat, jint> drawWithShadow{ *this };
#endif
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Camera, ONESTEP_MC_NAME("net/minecraft/class_4184"))
#if ONESTEP_MINECRAFT_26_1
    // Camera now owns the world-render FOV; no legacy GameRenderer signature.
    jni::method<jfloat, "getFov"> getFov{ *this };
#endif
    jni::method<jfloat, ONESTEP_MC_NAME("method_19329")> getPitch{ *this };
    jni::method<jfloat, ONESTEP_MC_NAME("method_19330")> getYaw{ *this };
    jni::method<Vec3d, bindings_profile::camera_position> getPos{ *this };
	jni::method<jboolean, ONESTEP_MC_NAME("method_19333")> isThirdPerson{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(EntityRenderDispatcher, ONESTEP_MC_NAME("net/minecraft/class_898"))
    jni::field<Camera, ONESTEP_MC_NAME("field_4686")> camera{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(GameRenderer, ONESTEP_MC_NAME("net/minecraft/class_757"))
        // Read-only vanilla ray query, rather than last frame's crosshair cache.
		// The value used by the world renderer after zoom/FOV multipliers.
#if !ONESTEP_MINECRAFT_26_1
		jni::method<bindings_profile::fov, ONESTEP_MC_NAME("method_3196"), jni::NOT_STATIC, Camera, jfloat, jboolean> getFov{ *this };
#endif
#if ONESTEP_MINECRAFT_26_1
        jni::method<Camera, ONESTEP_MC_NAME("method_19418")> getCamera{ *this };
#else
		jni::multi_method<Camera, ONESTEP_MC_NAME("method_19418"), "method_19349"> getCamera{ *this };
#endif
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(ServerInfo, ONESTEP_MC_NAME("net/minecraft/class_642"))
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(Session, ONESTEP_MC_NAME("net/minecraft/class_320"))
        jni::method<String, ONESTEP_MC_NAME("method_1676"), jni::NOT_STATIC> getUsername{ *this };
    jni::method<UUID, ONESTEP_MC_NAME("method_44717"), jni::NOT_STATIC> getUuidOrNull{ *this };
    jni::method<String, ONESTEP_MC_NAME("method_1675"), jni::NOT_STATIC> getSessionId{ *this };
    jni::method<String, ONESTEP_MC_NAME("method_1674"), jni::NOT_STATIC> getAccessToken{ *this };
#if ONESTEP_MINECRAFT_26_1
    jni::method<Optional, ONESTEP_MC_NAME("method_38740")> getClientId{*this};
    jni::method<Optional, ONESTEP_MC_NAME("method_38741")> getXuid{*this};
#else
    jni::method<String, ONESTEP_MC_NAME("method_38740"), jni::NOT_STATIC> getClientId{ *this };
    jni::method<String, ONESTEP_MC_NAME("method_38741"), jni::NOT_STATIC> getXuid{ *this };
    // No callers; User no longer exposes the historical account-type helper.
#endif
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(MinecraftClient, ONESTEP_MC_NAME("net/minecraft/class_310"))
        jni::field<MinecraftClient, ONESTEP_MC_NAME("field_1700"), jni::STATIC> instance{ *this };
    jni::field<ClientWorld, ONESTEP_MC_NAME("field_1687")> world{ *this };
    jni::field<ClientPlayerEntity, ONESTEP_MC_NAME("field_1724")> player{ *this };
    jni::field<HitResult, ONESTEP_MC_NAME("field_1765")> crosshairTarget{ *this };
    jni::field<ClientPlayerInteractionManager, ONESTEP_MC_NAME("field_1761")> interactionManager{ *this };
    jni::field<Mouse, ONESTEP_MC_NAME("field_1729")> mouse{ *this };
    jni::field<GameOptions, ONESTEP_MC_NAME("field_1690")> options{ *this };
    jni::field<RenderTickCounter, bindings_profile::tick_counter> renderTickCounter{ *this };
    jni::field<Screen, ONESTEP_MC_NAME("field_1755")> currentScreen{ *this };
    jni::method<ClientPlayNetworkHandler, ONESTEP_MC_NAME("method_1562")> getNetworkHandler{ *this };
    jni::method<Window, ONESTEP_MC_NAME("method_22683")> getWindow{ *this };
    jni::method<TextureManager, ONESTEP_MC_NAME("method_1531")> getTextureManager{ *this };
#if !ONESTEP_MINECRAFT_26_1
#endif
    jni::field<TextRenderer, ONESTEP_MC_NAME("field_1772")> textRenderer{ *this };
    jni::field<EntityRenderDispatcher, ONESTEP_MC_NAME("field_1731")> entityRenderDispatcher{ *this };
    jni::field<GameRenderer, ONESTEP_MC_NAME("field_1773")> gameRenderer{ *this };
    jni::field<Session, ONESTEP_MC_NAME("field_1726")> session{ *this };
    jni::method<GameProfile, ONESTEP_MC_NAME("method_53462")> getGameProfile{ *this };
    jni::method<ServerInfo, ONESTEP_MC_NAME("method_1558")> getCurrentServerEntry{ *this };
		jni::method<Entity, ONESTEP_MC_NAME("method_1560")> getCameraEntity{ *this };
		jni::method<void, ONESTEP_MC_NAME("method_1504"), jni::NOT_STATIC, Entity> setCameraEntity{ *this };
    jni::method<jboolean, ONESTEP_MC_NAME("method_1536"), jni::NOT_STATIC> doAttack{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_1583"), jni::NOT_STATIC> doItemUse{ *this };
    jni::field<jint, ONESTEP_MC_NAME("field_1752")> itemUseCooldown{ *this };
    END_KLASS_DEF()
}
