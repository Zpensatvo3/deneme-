#pragma once
#include <filesystem>
#include <string>

namespace configs::keybind_profile
{
    // Initialize after registration and before enabling render/module threads.
    // A path is injectable for offline tests; default is local OneStep app data.
    bool initialize(std::filesystem::path path = {});
    bool flush(bool force = false); // Render thread only; writes are atomic.
    const std::string& status();
    bool healthy();
}
