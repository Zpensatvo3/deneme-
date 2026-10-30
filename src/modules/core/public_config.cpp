#include "public_config.hpp"
#include "manager.hpp"

std::string modules::export_public_config() { return manager::export_config().payload; }
std::string modules::export_keybind_config() { return manager::export_config(true).payload; }
bool modules::apply_public_config(const std::string& payload, std::string& message)
{
    const auto result = manager::import_config(payload); message = result.message; return result.success;
}
bool modules::apply_keybind_config(const std::string& payload, std::string& message)
{
    const auto result = manager::import_config(payload, true); message = result.message; return result.success;
}
