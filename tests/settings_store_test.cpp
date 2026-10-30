#define main dto_roundtrip_test
#include "config_fields_test.cpp"
#undef main
#include "../src/modules/core/settings_store.hpp"
#include "../src/modules/misc/settings/fast_exp_settings.hpp"
#include <cassert>
#include <stdexcept>

int main()
{
    modules::settings::store<module_settings::fast_exp> settings{"fast_exp"};
    const auto initial = settings.read();
    auto copy = initial; copy.min_cps = 42;
    assert(copy.min_cps == 42 && settings.read() == initial);
    assert(!settings.update([](auto& candidate) { candidate.min_cps = 3; return false; }));
    assert(settings.read() == initial);
    try {
        settings.update([](auto& candidate) -> bool { candidate.min_cps = 8; throw std::runtime_error("reject"); });
        assert(false);
    } catch (const std::runtime_error&) {}
    assert(settings.read() == initial);
    assert(!settings.update([](auto&) { return true; }));
    assert(settings.update([](auto& candidate) { candidate.min_cps = 45; return true; }));
    assert(settings.read().min_cps == 45);
    const auto stale = modules::manager::read()->revision;
    modules::manager::enable("sprint", true);
    assert(!modules::manager::settings("fast_exp", R"({"min_cps":5})", false, stale));
    assert(settings.read().min_cps == 45);
    std::cout << "Settings view: copy isolation, rejected/throwing edits, no-op suppression and stale edit rejection passed\n";
}
