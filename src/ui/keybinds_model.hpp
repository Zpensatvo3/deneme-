#pragma once
#include "../modules/core/module_registry.hpp"
#include "../platform/input/key_codes.hpp"
#include "module_categories.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

namespace gui::keybinds_model
{
    inline std::string_view category(const modules::module& entry) { return module_categories::display(entry.get_category()); }
    inline bool visible(const modules::module* entry) { return entry && entry->supports_keybind() && !category(*entry).empty(); }
    inline std::string folded(std::string_view value) {
        std::string result(value);
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }
    inline std::vector<std::string> categories() {
        std::vector<std::string> result;
        for (const auto* entry : modules::get_modules())
            if (visible(entry)) result.emplace_back(category(*entry));
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }
    inline std::vector<modules::module*> filtered(std::string_view search, std::string_view category) {
        const auto query = folded(search);
        std::vector<modules::module*> result;
        for (auto* entry : modules::get_modules()) {
            if (!visible(entry)) continue;
            const auto group = keybinds_model::category(*entry);
            if (!category.empty() && module_categories::display(category) != group) continue;
            if (!query.empty() && folded(std::string(entry->get_name()) + " " + std::string(group)).find(query) == std::string::npos) continue;
            result.push_back(entry);
        }
        std::stable_sort(result.begin(), result.end(), [](const auto* a, const auto* b) {
            return keybinds_model::category(*a) < keybinds_model::category(*b);
        });
        return result;
    }
    inline std::array<int, platform::keys::num_enter + 1> counts() {
        std::array<int, platform::keys::num_enter + 1> result{};
        for (const auto* entry : modules::get_modules()) {
            if (!visible(entry)) continue;
            const int key = entry->get_keybind();
            if (key > 0 && key < static_cast<int>(result.size())) ++result[key];
        }
        return result;
    }
    inline bool assign(modules::module& entry, int key) {
        if (!visible(&entry) || !platform::keys::assignable(key)) return false;
        return entry.set_keybind(key).success;
    }
    inline bool clear(modules::module& entry) {
        if (!visible(&entry)) return false;
        return entry.set_keybind(0).success;
    }
}
