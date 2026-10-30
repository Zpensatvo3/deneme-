#include "manager.hpp"
#include "config_json.hpp"
#include <atomic>
#include <charconv>
#include <stdexcept>

namespace
{
    std::atomic<std::shared_ptr<const modules::manager::snapshot>> published{
        std::make_shared<const modules::manager::snapshot>()};
    std::atomic<std::shared_ptr<const modules::manager::transport>> connection{};
    thread_local std::shared_ptr<const modules::manager::snapshot> pinned;
    std::uint64_t unsigned_value(const modules::config_json::value* value)
    {
        if (!value || value->type != modules::config_json::kind::number) throw std::runtime_error("Invalid manager revision");
        std::uint64_t number = 0;
        auto parsed = std::from_chars(value->raw.data(), value->raw.data() + value->raw.size(), number);
        if (parsed.ec != std::errc{} || parsed.ptr != value->raw.data() + value->raw.size()) throw std::runtime_error("Invalid manager revision");
        return number;
    }
    std::string command(std::string_view operation, std::string_view id)
    {
        return "{\"operation\":\"" + std::string(operation) + "\",\"id\":\"" + modules::config_json::escape(id) + '"';
    }
}

bool modules::manager::connect(transport io)
{
    if (!io.snapshot_since || !io.request || connection.load()) return false;
    connection.store(std::make_shared<const transport>(io));
    if (refresh() && !read()->modules.empty()) return true;
    disconnect(); return false;
}
void modules::manager::disconnect()
{
    connection.store(nullptr);
    published.store(std::make_shared<const snapshot>());
}
modules::manager::view_scope::view_scope() : previous_(std::move(pinned)) { pinned = previous_ ? previous_ : published.load(); }
modules::manager::view_scope::~view_scope() { pinned = std::move(previous_); }
std::shared_ptr<const modules::manager::snapshot> modules::manager::read() { return pinned ? pinned : published.load(); }
modules::manager::module_state modules::manager::state(std::string_view id)
{
    const auto current = read(); const auto found = current->modules.find(std::string(id));
    return found == current->modules.end() ? module_state{} : found->second;
}
std::shared_ptr<const modules::manager::module_state> modules::manager::find(std::string_view id)
{
    const auto snapshot = read(); const auto found = snapshot->modules.find(std::string(id));
    return found == snapshot->modules.end() ? nullptr : std::shared_ptr<const module_state>(snapshot, &found->second);
}
bool modules::manager::refresh()
{
    const auto io = connection.load(); if (!io) return false;
    try {
        const auto before = published.load(); const std::string bytes = io->snapshot_since(before->revision);
        if (bytes.empty()) return true;
        const auto root = config_json::protocol(bytes);
        if (!root || root->type != config_json::kind::object) return false;
        auto candidate = std::make_shared<snapshot>();
        candidate->revision = unsigned_value(root->find("revision"));
        candidate->keybind_revision = unsigned_value(root->find("keybind_revision"));
        const auto* entries = root->find("modules");
        if (!entries || entries->type != config_json::kind::array || entries->items.size() != 36) return false;
        for (const auto& entry : entries->items) {
            const auto* id = entry.find("id"); const auto* name = entry.find("name"); const auto* category = entry.find("category");
            const auto* enabled = entry.find("enabled"); const auto* binding = entry.find("keybind");
            const auto* supports = entry.find("supports_keybind"); const auto* settings = entry.find("settings");
            if (!id || id->type != config_json::kind::string || !name || name->type != config_json::kind::string ||
                !category || category->type != config_json::kind::string || !enabled || enabled->type != config_json::kind::boolean ||
                !binding || !binding->integer() || !supports || supports->type != config_json::kind::boolean ||
                !settings || settings->type != config_json::kind::object) return false;
            module_state state{name->text, category->text, std::string(settings->raw), enabled->boolean, supports->boolean,
                *binding->integer(), unsigned_value(entry.find("generation"))};
            if (!candidate->modules.emplace(id->text, std::move(state)).second) return false;
        }
        // A slow render-thread response must never overwrite a newer worker/UI response.
        auto current = published.load();
        while (current->revision < candidate->revision) {
            if (connection.load() != io) return false;
            if (published.compare_exchange_weak(current, candidate)) break;
        }
        return true;
    } catch (...) { return false; }
}
modules::manager::result modules::manager::request(const std::string& command)
{
    const auto io = connection.load(); if (!io) return {false, "Java module manager is unavailable"};
    try {
        const auto bytes = io->request(command); const auto response = config_json::protocol(bytes);
        if (!response) return {false, "Invalid Java manager response"};
        const auto* success = response->find("success"); const auto* message = response->find("message");
        if (!success || success->type != config_json::kind::boolean || !message || message->type != config_json::kind::string)
            return {false, "Invalid Java manager response"};
        result output{success->boolean, message->text, {}, unsigned_value(response->find("revision"))};
        if (const auto* payload = response->find("payload"); payload && payload->type == config_json::kind::string) output.payload = payload->text;
        if (output.success && !refresh()) return {false, "Java state changed but snapshot synchronization failed", {}, output.revision};
        return output;
    } catch (...) { return {false, "Java manager request failed"}; }
}
modules::manager::result modules::manager::enable(std::string_view id, bool value)
{ return request(command("enable", id) + ",\"enabled\":" + (value ? "true}" : "false}")); }
modules::manager::result modules::manager::toggle(std::string_view id) { return request(command("toggle", id) + '}'); }
modules::manager::result modules::manager::keybind(std::string_view id, int value)
{ return request(command("keybind", id) + ",\"keybind\":" + std::to_string(value) + '}'); }
modules::manager::result modules::manager::reset(std::string_view id) { return request(command("reset", id) + '}'); }
modules::manager::result modules::manager::settings(std::string_view id, const std::string& json, bool validate, std::uint64_t revision)
{
    return request(command(validate ? "validate" : "settings", id) + ",\"settings\":" + json +
        (revision ? ",\"expected_revision\":" + std::to_string(revision) : "") + '}');
}
modules::manager::result modules::manager::import_config(const std::string& json, bool keybinds_only)
{ return request(command(keybinds_only ? "import_keybinds" : "import", "") + ",\"payload\":\"" + config_json::escape(json) + "\"}"); }
modules::manager::result modules::manager::export_config(bool keybinds_only)
{ return request(command(keybinds_only ? "export_keybinds" : "export", "") + '}'); }
modules::manager::result modules::manager::disable_all() { return request("{\"operation\":\"disable_all\"}"); }
