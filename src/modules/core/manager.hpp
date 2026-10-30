#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace modules::manager
{
    struct module_state
    {
        std::string name, category, settings;
        bool enabled = false, supports_keybind = true;
        int keybind = 0;
        std::uint64_t generation = 0;
    };
    struct snapshot
    {
        std::uint64_t revision = 0, keybind_revision = 0;
        std::unordered_map<std::string, module_state> modules;
    };
    struct result
    {
        bool success = false;
        std::string message, payload;
        std::uint64_t revision = 0;
        explicit operator bool() const { return success; }
    };
    // Transport never owns module state. Production installs the parent-loader
    // JNI bridge; offline tests can install an isolated deterministic transport.
    struct transport
    {
        std::string (*snapshot_since)(std::uint64_t) = nullptr;
        std::string (*request)(const std::string&) = nullptr;
    };
    // Pin one publication for the duration of a native engine/UI callback.
    // Commands may publish concurrently; this callback still sees one coherent view.
    class view_scope
    {
    public:
        view_scope();
        ~view_scope();
        view_scope(const view_scope&) = delete;
        view_scope& operator=(const view_scope&) = delete;
    private:
        std::shared_ptr<const snapshot> previous_;
    };
    bool connect(transport io);
    void disconnect();
    bool refresh();
    std::shared_ptr<const snapshot> read();
    module_state state(std::string_view id);
    std::shared_ptr<const module_state> find(std::string_view id);
    result request(const std::string& command);
    result enable(std::string_view id, bool value);
    result toggle(std::string_view id);
    result keybind(std::string_view id, int value);
    result reset(std::string_view id);
    result settings(std::string_view id, const std::string& json, bool validate = false, std::uint64_t revision = 0);
    result import_config(const std::string& json, bool keybinds_only = false);
    result export_config(bool keybinds_only = false);
    result disable_all();
}
