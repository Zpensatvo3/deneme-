#include "../src/mappings.hpp"
#include "../src/platform/adapter/item_transform.hpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <string_view>

using namespace std::literals;
static_assert(ONESTEP_MINECRAFT_26_1);
static_assert(mc::target.build_id == ONESTEP_MINECRAFT_VERSION);
static_assert(mc::parse_version(mc::target.name) == mc::target.id);
static_assert(!mc::matches_target(mc::parse_version("26.1.3")));
static_assert(!mc::matches_target(mc::parse_version("1.21.11")));
static_assert(!mc::matches_target(mc::parse_version("26.1-pre1")));
static_assert(std::string_view(maps::MinecraftClient::get_name()) == "net/minecraft/client/Minecraft"sv);
static_assert(std::string_view(maps::SlotActionType::get_name()) == "net/minecraft/world/inventory/ContainerInput"sv);
static_assert(std::string_view(decltype(maps::ClientPlayerInteractionManager_members::clickSlot)::get_name()) == "handleContainerInput"sv);
static_assert(std::string_view(decltype(maps::ClientPlayerInteractionManager_members::clickSlot)::get_signature()) ==
    "(IIILnet/minecraft/world/inventory/ContainerInput;Lnet/minecraft/world/entity/player/Player;)V"sv);
static_assert(std::string_view(decltype(maps::Camera_members::getFov)::get_signature()) == "()F"sv);
static_assert(std::string_view(decltype(maps::ItemRenderLayer_members::tints)::get_signature()) == "Lit/unimi/dsi/fastutil/ints/IntList;"sv);
static_assert(std::string_view(decltype(maps::ItemBakedQuad_members::getMaterial)::get_signature()) ==
    "()Lnet/minecraft/client/resources/model/geometry/BakedQuad$MaterialInfo;"sv);
static_assert(decltype(maps::ClientPlayerEntity_members::findCrosshairTarget)::is_method_static());
static_assert(std::string_view(decltype(maps::ItemStack_members::getItem)::get_signature()) == "()Lnet/minecraft/world/item/Item;"sv);
static_assert(std::string_view(decltype(maps::ItemStack_members::matchesItem)::get_signature()) == "(Ljava/lang/Object;)Z"sv);
static_assert(std::string_view(decltype(maps::BlockState_members::matchesBlock)::get_signature()) == "(Ljava/lang/Object;)Z"sv);
static_assert(std::string_view(decltype(maps::Potions_members::healingReference)::get_signature()) == "Lnet/minecraft/core/Holder$Reference;"sv);

// Compile the existing module-facing calls with their concrete argument types.
// This function is never executed: the ordinary offline tests do not start a JVM.
[[maybe_unused]] static void module_contracts(const maps::ItemStack& stack, const maps::Item& item,
    const maps::BlockState& state, const maps::Block& block, const maps::PotionContentsComponent& contents)
{
    maps::Item resolved = stack.item.get();
    stack.isOf.call(item);
    state.isOf.call(block);
    maps::RegistryEntry healing = maps::Potions{}.HEALING.get();
    contents.matches.call(healing);
}

int main()
{
    using platform::adapter::apply_item_local_transform;
    std::array<float, 16> matrix{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    std::array<float, 3> point{1,2,3};
    assert(apply_item_local_transform(point, matrix));
    assert((point == std::array<float,3>{1,2,3}));
    matrix[12] = 4; matrix[13] = -1; matrix[14] = 2;
    assert(apply_item_local_transform(point, matrix));
    assert((point == std::array<float,3>{5,1,5}));
    // Local coordinates are transformed before the HUD's existing -.5 centering.
    matrix = {2,0,0,0, 0,3,0,0, 0,0,4,0, 0,0,0,1};
    point = {0.5f,0.5f,0.5f};
    assert(apply_item_local_transform(point, matrix));
    assert((point == std::array<float,3>{1,1.5f,2}));
    const auto original = point;
    matrix[0] = std::numeric_limits<float>::infinity();
    assert(!apply_item_local_transform(point, matrix) && point == original);
    matrix[0] = 1; matrix[15] = 0;
    assert(!apply_item_local_transform(point, matrix) && point == original);
    matrix[15] = 1; matrix[3] = 0.5f;
    assert(!apply_item_local_transform(point, matrix) && point == original);
    std::cout << mc::target.name << " native SDK: exact names, contracts, static ray query, bounded affine item transforms passed\n";
}
