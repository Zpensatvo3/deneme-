#include "keybind_profile.hpp"
#include "../modules/core/module_registry.hpp"
#include "../modules/core/public_config.hpp"
#include <chrono>
#include <fstream>
#include <iterator>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace {
    std::filesystem::path destination;
    std::uint64_t observed = 0;
    bool dirty = false, success = true;
    std::string message;
    std::chrono::steady_clock::time_point changed_at{};
    std::filesystem::path default_path() {
#ifdef _WIN32
        wchar_t directory[32768]{};
        const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", directory, 32768);
        if (length && length < 32768) return std::filesystem::path(directory) / L"onestep" / L"keybinds.json";
#endif
        return {};
    }
    bool fail(const char* text) { message = text; success = false; return false; }
}

bool configs::keybind_profile::initialize(std::filesystem::path path)
{
    destination = path.empty() ? default_path() : std::move(path);
    dirty = false; success = true; message = "Keybind profile ready.";
    observed = modules::manager::read()->keybind_revision;
    if (destination.empty()) return fail("Local app data is unavailable; keybinds cannot be saved.");
    std::error_code error;
    const auto parent = destination.parent_path();
    if (!parent.empty()) {
        const bool parent_exists = std::filesystem::exists(parent, error);
        if (error || (parent_exists && !std::filesystem::is_directory(parent, error)) || error)
            return fail("Cannot access the local profile directory.");
    }
    const bool exists = std::filesystem::exists(destination, error);
    if (error) return fail("Cannot access the local keybind profile.");
    if (!exists) return true;
    const auto bytes = std::filesystem::file_size(destination, error);
    if (error || bytes > 96 * 1024) return fail("Invalid local keybind profile; existing file preserved.");
    std::ifstream input(destination, std::ios::binary);
    if (!input) return fail("Cannot read the local keybind profile.");
    const std::string payload((std::istreambuf_iterator<char>(input)), {});
    if (input.bad() || !modules::apply_keybind_config(payload, message)) {
        observed = modules::manager::read()->keybind_revision;
        return fail("Invalid local keybind profile; existing file preserved.");
    }
    observed = modules::manager::read()->keybind_revision;
    return true;
}

bool configs::keybind_profile::flush(bool force)
{
    const auto revision = modules::manager::read()->keybind_revision;
    const auto now = std::chrono::steady_clock::now();
    if (revision != observed) { observed = revision; dirty = true; changed_at = now; }
    if (!dirty) return success;
    if (!force && now - changed_at < std::chrono::milliseconds(400)) return success;
    changed_at = now; // Bound retries after an I/O failure.
    if (destination.empty()) return fail("Local app data is unavailable; keybinds cannot be saved.");
    std::error_code error;
    if (!destination.parent_path().empty()) std::filesystem::create_directories(destination.parent_path(), error);
    if (error) return fail("Cannot create the local profile directory.");
    auto temporary = destination; temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return fail("Cannot write the local keybind profile.");
        const auto payload = modules::export_keybind_config();
        output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        output.close();
        if (output.fail()) { std::filesystem::remove(temporary, error); return fail("Keybind save failed; previous profile preserved."); }
    }
#ifdef _WIN32
    const bool replaced = MoveFileExW(temporary.c_str(), destination.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#else
    std::filesystem::rename(temporary, destination, error);
    const bool replaced = !error;
#endif
    if (!replaced) { std::filesystem::remove(temporary, error); return fail("Keybind save failed; previous profile preserved."); }
    dirty = false; success = true; message = "Keybinds saved locally.";
    return true;
}

const std::string& configs::keybind_profile::status() { return message; }
bool configs::keybind_profile::healthy() { return success; }
