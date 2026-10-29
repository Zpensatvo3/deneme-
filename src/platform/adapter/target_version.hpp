#pragma once
#include <array>
#include <string_view>

// One target per binary.
#ifndef ONESTEP_MINECRAFT_VERSION
#define ONESTEP_MINECRAFT_VERSION 12111
#endif
#if ONESTEP_MINECRAFT_VERSION != 12111 && \
    ONESTEP_MINECRAFT_VERSION != 26100 && ONESTEP_MINECRAFT_VERSION != 26101 && \
    ONESTEP_MINECRAFT_VERSION != 26102
#error Unsupported OneStep Minecraft target
#endif

// Shared SDK layout verified against all three exact release jars. This is not
// a wildcard: later patches/releases still require their own audit and target.
#define ONESTEP_MINECRAFT_26_1 (ONESTEP_MINECRAFT_VERSION == 26100 || \
    ONESTEP_MINECRAFT_VERSION == 26101 || ONESTEP_MINECRAFT_VERSION == 26102)

namespace mc
{
    enum class version { unknown, v1_21_11, v26_1, v26_1_1, v26_1_2 };
    struct version_info { version id; int build_id; std::string_view name; std::string_view yarn; };
    inline constexpr std::array versions{
        version_info{version::v1_21_11, 12111, "1.21.11", "1.21.11+build.6"},
        version_info{version::v26_1, 26100, "26.1", ""},
        version_info{version::v26_1_1, 26101, "26.1.1", ""},
        version_info{version::v26_1_2, 26102, "26.1.2", ""}
    };
    constexpr version parse_version(std::string_view text)
    {
        for (const auto& entry : versions) if (entry.name == text) return entry.id;
        return version::unknown;
    }
    constexpr std::string_view version_name(version id)
    {
        for (const auto& entry : versions) if (entry.id == id) return entry.name;
        return "unknown";
    }
    inline constexpr auto target = [] {
        for (const auto& entry : versions)
            if (entry.build_id == ONESTEP_MINECRAFT_VERSION) return entry;
        return version_info{version::unknown, 0, "unknown", ""};
    }();
    constexpr bool matches_target(version detected) { return detected == target.id; }
}
