#include "../src/modules/core/public_config.hpp"
#include "../src/modules/core/module_registry.hpp"
#include "../src/modules/core/config_json.hpp"
#include "../src/modules/combat/settings/anti_bot_settings.hpp"
#include "../src/modules/core/settings_store.hpp"
#include <cassert>
#include <iostream>
#include <locale>

namespace {
    std::vector<modules::module*> registry;
}
namespace modules {
    module::module(const char* n, const char* c, const char* id) : id_storage(id ? id : n), name(n), category(c) {}
    void module::run(::cache&) {}
    void module::render_options() {}
    void module::on_enable(::cache&) {}
    void module::on_disable(::cache&) {}
    const char* module::get_id() const { return id_storage.c_str(); }
    int module::get_keybind() const { return keybind; }
    void module::set_keybind(int value) { keybind = value; }
    std::string module::export_config_settings() const { return serialize_config_fields(const_cast<module&>(*this)); }
    bool module::validate_config_settings(const std::string& text) const { return validate_config_fields(const_cast<module&>(*this), text); }
    bool module::apply_config_settings(const std::string& text) { return deserialize_config_fields(*this, text); }
    const std::vector<module*>& get_modules() { return registry; }
    module* find_module_by_id(const std::string& id) {
        for (auto* m : registry) if (m && id == m->get_id()) return m;
        return nullptr;
    }
}
class sample final : public modules::module {
public:
    explicit sample(const char* id) : module(id) {}
    int count = 7;
    float strength = .625f;
    float range[2]{.5f, .75f};
    std::string text = "braces } {, quotes \" and newline\n";
    bool fail_apply = false;
    void visit_config(modules::config_visitor& v) override {
        v.field("count", count); v.field("text", text);
        v.field("strength", strength); v.field("range", range);
        if (v.reading() && fail_apply && count == 13) v.reject("Simulated application failure.");
    }
};

int main()
{
    struct comma_locale final : std::numpunct<char> { char do_decimal_point() const override { return ','; } };
    const auto original_locale = std::locale::global(std::locale(std::locale::classic(), new comma_locale));
    sample a("sample"), b("other");
    registry = {&a, nullptr, &b};
    a.enabled = true; a.set_keybind(65);
    std::string message;
    const auto saved = modules::export_public_config();
    assert(saved.find("\"version\"") == std::string::npos);
    assert(modules::apply_public_config(saved, message));
    assert(saved.find("0.625") != std::string::npos);
    assert(a.count == 7 && a.enabled && a.get_keybind() == 65);
    assert(modules::apply_public_config("{\"version\":3," + saved.substr(1), message));
    assert(modules::export_public_config() == saved);

    for (const std::string invalid : {
        "", "{}", "[]", "{\"modules\":[]}", "{\"modules\":[", "{\"modules\":[]}garbage",
        "{\"version\":4,\"modules\":[]}", "{\"version\":\"3\",\"modules\":[]}",
        "{\"modules\":[],\"modules\":[]}", "{\"modules\":[{},]}",
        "{\"modules\":[{\"id\":\"sample\",\"enabled\":true,\"keybind\":256,\"settings\":{}}]}",
        "{\"modules\":[{\"id\":\"sample\",\"enabled\":true,\"keybind\":1.5,\"settings\":{}}]}",
        "{\"modules\":[{\"id\":\"sample\",\"enabled\":true,\"keybind\":-1,\"settings\":{}}]}",
        "{\"modules\":[{\"id\":\"sample\",\"enabled\":true,\"keybind\":1,\"settings\":{\"count\":2,\"count\":3}}]}",
        "{\"modules\":[{\"id\":\"sample\",\"enabled\":true,\"keybind\":1,\"settings\":{\"count\":2}},"
            "{\"id\":\"other\",\"enabled\":false,\"keybind\":2,\"settings\":{\"count\":\"bad\"}}]}",
        "{\"modules\":[{\"id\":\"sample\",\"enabled\":true,\"keybind\":1,\"settings\":{}},"
            "{\"id\":\"sample\",\"enabled\":false,\"keybind\":2,\"settings\":{}}]}"
    }) {
        assert(!modules::apply_public_config(invalid, message));
        assert(!message.empty() && modules::export_public_config() == saved);
    }
    for (const char* invalid : {"[]", "{", "{\"count\":01}", "{\"count\":7,}", "{\"count\":1} extra",
        "{\"count\":1,\"coun\\u0074\":2}", "{\"text\":\"\\uD800\"}", "{\"count\":nan}"})
        assert(!a.validate_config_settings(invalid));
    assert(!a.validate_config_settings(std::string("{\"text\":\"") + char(0xff) + "\"}"));
    assert(a.apply_config_settings(R"({"text":"\u0131\u015f\uD83D\uDE00\b\f\u0000"})"));
    assert(a.text == std::string("\xc4\xb1\xc5\x9f\xf0\x9f\x98\x80\b\f\0", 11));
    assert(a.apply_config_settings(a.export_config_settings()));
    assert(modules::apply_public_config(saved, message));

    b.fail_apply = true;
    assert(!modules::apply_public_config(R"({"modules":[
        {"id":"sample","enabled":false,"keybind":3,"settings":{"count":8}},
        {"id":"other","enabled":true,"keybind":4,"settings":{"count":13}}
    ]})", message));
    assert(modules::export_public_config() == saved);
    assert(message.find("restored") != std::string::npos);
    b.fail_apply = false;
    assert(modules::apply_public_config(R"({"modules":[
        {"id":"removed_module","enabled":true,"keybind":0,"settings":{}},
        {"id":"sample","enabled":false,"keybind":255,"settings":{"count":8}}
    ]})", message));
    assert(a.count == 8 && !a.enabled && a.get_keybind() == 255 && b.count == 7);
    assert(!modules::config_json::parse(std::string(96 * 1024 + 1, ' ')));
    assert(!modules::config_json::parse(std::string(40, '[') + "0" + std::string(40, ']')));
    sample escaped("quoted\"id\n");
    registry = {&escaped};
    assert(modules::apply_public_config(modules::export_public_config(), message));
    // An old config that omits a new module preserves its default-on state.
    // An explicit saved false remains false; defaults never override imports.
    sample anti_bot("anti_bot");
    anti_bot.enabled = module_settings::anti_bot_default_enabled;
    registry = {&a, &anti_bot};
    assert(modules::apply_public_config(R"({"modules":[{"id":"sample","enabled":true,"keybind":0,"settings":{}}]})", message));
    assert(anti_bot.enabled);
    assert(modules::apply_public_config(R"({"modules":[{"id":"anti_bot","enabled":false,"keybind":0,"settings":{}}]})", message));
    assert(!anti_bot.enabled);
    const auto disabled_anti_bot = modules::export_public_config();
    anti_bot.enabled = true;
    assert(modules::apply_public_config(disabled_anti_bot, message) && !anti_bot.enabled);
    std::locale::global(original_locale);
    std::cout << "Plain configs: legacy reads, strict JSON, Unicode, stable IDs, validation and failure rollback passed\n";
}
