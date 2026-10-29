#pragma once
#include "../../mappings.hpp"
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace platform::adapter
{
    struct item_model_quad
    {
        std::array<std::array<float, 3>, 4> position{};
        std::array<std::array<float, 2>, 4> uv{};
        std::array<float, 3> rotation{}, translation{}, scale{};
        std::uintptr_t texture = 0;
        std::uint32_t tint = 0xffffffff;
        bool side_lit = false;
    };
    // Reads model/atlas data only. GUI projection and drawing remain in the HUD.
    std::vector<item_model_quad> read_item_model(const maps::MinecraftClient& client,
        const maps::World& world, const maps::LivingEntity& player,
        const maps::ItemStack& stack, std::string_view translation_key);
}
