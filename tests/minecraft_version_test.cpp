#include "../src/platform/adapter/target_version.hpp"
#include "../src/mappings.hpp"
#include "../src/modules/visual/chams_model.hpp"
#include <cassert>
#include <iostream>
#include <string_view>

int main()
{
    using namespace std::literals;
    for (const auto& entry : mc::versions)
    {
        assert(mc::parse_version(entry.name) == entry.id);
        assert(mc::version_name(entry.id) == entry.name);
        assert(mc::matches_target(entry.id) == (entry.build_id == ONESTEP_MINECRAFT_VERSION));
    }
    for (const auto name : {"", "1.21", "1.21.1", "1.21.4", "1.21.8", "1.21.10", "1.21.12", "1.21.1-pre1", "Minecraft 1.21.1",
                           "26.1.3", "26.1.1-pre1", "26.1.2-rc1", "26.2", "26.3", "26.4-snapshot-4"})
        assert(mc::parse_version(name) == mc::version::unknown);
    assert(mc::parse_version("26.1") == mc::version::v26_1);
    assert(mc::parse_version("26.1.1") == mc::version::v26_1_1);
    assert(mc::parse_version("26.1.2") == mc::version::v26_1_2);
    assert(!mc::matches_target(mc::version::unknown));
    using fall = decltype(maps::Entity_members::fallDistance);
#if ONESTEP_MINECRAFT_26_1
    static_assert(std::string_view(fall::get_signature()) == "D"sv);
    static_assert(std::string_view(decltype(maps::Camera_members::getFov)::get_signature()) == "()F"sv);
    static_assert(std::string_view(decltype(maps::Mouse_members::onMouseInput)::get_signature()) == "(JLnet/minecraft/client/input/MouseButtonInfo;I)V"sv);
#else
    using fov = decltype(maps::GameRenderer_members::getFov);
    static_assert(std::string_view(fall::get_signature()) == "D"sv);
    static_assert(std::string_view(decltype(maps::ItemRenderLayer_members::getQuads)::get_signature()) == "()Ljava/util/List;"sv);
    static_assert(std::string_view(fov::get_signature()) == "(Lnet/minecraft/class_4184;FZ)F"sv);
    static_assert(std::string_view(decltype(maps::MinecraftClient_members::renderTickCounter)::get_name()) == "field_52750"sv);
    static_assert(std::string_view(decltype(chams_model::Model_members::setAngles)::get_signature()) == "(Lnet/minecraft/class_10055;)V"sv);
    static_assert(std::string_view(decltype(maps::Entity_members::getWorld)::get_name()) == "method_73183"sv);
    static_assert(std::string_view(decltype(maps::Camera_members::getPos)::get_name()) == "method_71156"sv);
    static_assert(std::string_view(decltype(maps::Mouse_members::onMouseInput)::get_signature()) == "(JLnet/minecraft/class_11910;I)V"sv);
    static_assert(std::string_view(decltype(maps::GameProfile_members::getName)::get_name()) == "name"sv);
    static_assert(std::string_view(decltype(maps::SkinTextures_members::body)::get_signature()) == "()Lnet/minecraft/class_12079$class_12081;"sv);
#endif
    std::cout << "Minecraft target " << mc::target.name << ": exact version matching and JNI profile signatures passed\n";
}
