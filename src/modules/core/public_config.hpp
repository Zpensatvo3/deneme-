#pragma once

#include <string>

namespace modules
{
    std::string export_public_config();
    bool apply_public_config(const std::string& payload, std::string& message);
    // Local keybind profile: same modules/id/keybind JSON fields, no gameplay
    // enabled/settings changes. Public configs continue to include keybinds.
    std::string export_keybind_config();
    bool apply_keybind_config(const std::string& payload, std::string& message);
}
