#include "../src/mappings.hpp"
#include "../src/modules/visual/chams_model.hpp"
#include "../src/modules/visual/sky_bindings.hpp"
#include <iostream>

bool include_owners = false;
bool include_contracts = false;
template<class Owner, class Member> void binding()
{
    if (include_owners)
        std::cout << static_cast<const char*>(Owner::get_name()) << '\t';
    std::cout << static_cast<const char*>(Member::get_name()) << '\t'
        << static_cast<const char*>(Member::get_signature());
    if (include_contracts)
    {
        constexpr auto signature = Member::get_signature();
        if constexpr (signature.value[0] == '(') std::cout << '\t' << Member::is_method_static();
        else std::cout << '\t' << Member::is_field_static();
    }
    std::cout << '\n';
}
#define B(owner, member) binding<owner, decltype(owner##_members::member)>()
int main(int argc, char** argv)
{
    include_contracts = argc == 2 && std::string_view(argv[1]) == "--contracts";
    include_owners = include_contracts || (argc == 2 && std::string_view(argv[1]) == "--owners");
    if (argc > 1 && !include_owners) return 2;
    B(maps::MinecraftClient, instance); B(maps::MinecraftClient, player); B(maps::MinecraftClient, world);
    B(maps::MinecraftClient, renderTickCounter); B(maps::MinecraftClient, doAttack); B(maps::MinecraftClient, doItemUse);
    B(maps::Entity, getWorld); B(maps::Entity, getPos); B(maps::Entity, fallDistance);
    B(maps::Entity, getBoundingBox); B(maps::Entity, getLerpedPos); B(maps::Entity, getEyeY);
    B(maps::LivingEntity, jumpingCooldown); B(maps::LivingEntity, hasStatusEffectEntry);
    B(maps::PlayerEntity, getInventory); B(maps::PlayerEntity, getAttackCooldownProgress);
    B(maps::PlayerEntity, getBlockInteractionRange); B(maps::PlayerEntity, getEntityInteractionRange);
    B(maps::PlayerInventory, main); B(maps::PlayerInventory, selectedSlot);
    B(maps::Inventory, getStack); B(maps::ClientPlayerInteractionManager, clickSlot);
    B(maps::ClientPlayerInteractionManager, syncSelectedSlot); B(maps::ClientPlayerInteractionManager, attackEntity);
    B(maps::ItemStack, getComponent); B(maps::ItemStack, getMiningSpeedMultiplier);
    B(maps::Item, getRegistryEntry); B(maps::ItemTags, SWORDS); B(maps::ItemRegistryEntry, isIn);
    B(maps::Camera, getPos);
#if ONESTEP_MINECRAFT_26_1
    B(maps::Camera, getFov);
#else
    B(maps::GameRenderer, getFov);
#endif
    B(maps::RenderTickCounter, getTickDelta);
    B(maps::ClientWorld, getChunkManager); B(maps::ClientChunkManager, chunks); B(maps::ClientChunkMap, chunks);
    B(maps::PaletteData, palette); B(maps::PaletteData, storage); B(maps::PalettedContainer, data);
    B(maps::Chunk, getSectionArray); B(maps::WorldChunk, getBlockEntities);
    B(maps::TextureManager, getTexture); B(maps::AbstractClientPlayerEntity, getSkinTextures);
    B(sky_bindings::World, getRegistryManager); B(sky_bindings::World, getDimension);
    B(sky_bindings::Dimension, hasSkyLight); B(sky_bindings::Camera, getSubmersionType);
    B(chams_model::Model, constructor); B(chams_model::Model, setAngles); B(chams_model::Skin, model);
    B(chams_model::Loader, getModelPart); B(chams_model::Client, getEntityModelLoader);
#if ONESTEP_MINECRAFT_26_1
    B(maps::ItemBakedQuad, getMaterial); B(maps::QuadMaterialInfo, getColorIndex); B(maps::QuadMaterialInfo, getSprite);
#else
    B(maps::ItemBakedQuad, getColorIndex); B(maps::ItemBakedQuad, getSprite);
#endif
    B(maps::PlayerInputFlags, jump); B(maps::RecipeBookScreen, recipeBook);
    B(maps::ItemModelManager, update);
    B(maps::ItemRenderState, layerCount); B(maps::ItemRenderState, layers); B(maps::ItemRenderLayer, tints);
    B(chams_model::EntityRenderer, createRenderState); B(chams_model::EntityRenderer, updateRenderState);
    B(chams_model::Dispatcher, getRenderer);
    B(maps::TextureAccess, getGpuTexture); B(maps::GlTexture, getGlId);
    B(maps::ItemRenderLayer, getQuads); B(maps::ItemRenderLayer, transform); B(maps::ItemRenderLayer, useLight);
    B(maps::ItemTransformation, rotation); B(maps::ItemTransformation, translation); B(maps::ItemTransformation, scale);
    binding<maps::Input, decltype(maps::Input_members::axis::vector)>();
    B(maps::ClientPlayerEntity, findCrosshairTarget); B(maps::Mouse, onMouseInput);
    B(maps::MouseInput, constructor); B(maps::SkinTextures, body); B(maps::SkinTextureAsset, texturePath);
    B(maps::ItemBakedQuad, getPosition); B(maps::ItemBakedQuad, getTexcoords);
    B(maps::PackedUv, getX); B(maps::PackedUv, getY);
#if ONESTEP_MINECRAFT_26_1
    B(maps::MinecraftClient, session); B(maps::Session, getUsername);
    B(maps::GameRenderer, getCamera); B(maps::ClientPlayNetworkHandler, sendPacket);
    B(maps::ClientPlayNetworkHandler, getPlayerList); B(maps::ItemSprite, getAtlasId);
    B(maps::ItemModelClient, getItemModelManager); B(maps::ItemRenderState, constructor);
    B(maps::ItemRenderLayer, localTransform); B(maps::SlotActionType, PICKUP);
    B(maps::SlotActionType, QUICK_MOVE); B(maps::SlotActionType, SWAP);
#endif
}
