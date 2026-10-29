#include "../src/ui/module_categories.hpp"
#include "../src/ui/keyboard_layout.hpp"
#include <cassert>
#include <iostream>
#include <set>

int main()
{
    using gui::module_categories::display;
    std::set<std::string_view> groups;
    for (auto source : {"Combat", "Movement", "Player", "World", "Misc", "Friends", "Visual", "Render", "Vape", "Client"})
        if (auto group = display(source); !group.empty()) groups.insert(group);
    assert(groups == (std::set<std::string_view>{"Combat", "Misc", "Visuals", "Hud", "Vape", "Client"}));
    for (auto source : {"Movement", "Player", "World", "Misc"}) assert(display(source) == "Misc");
    assert(display("Friends").empty());
    assert(display("Custom group") == "Custom group"); // extension modules stay usable
    for (float width : {420.f, 600.f, 700.f, 1000.f}) {
        const auto layout = gui::keyboard_layout::measure(width);
        assert(layout.unit >= 29.f && layout.width >= width - .01f);
        assert(layout.scroll == (width < 667.f));
        for (const auto& key : platform::keys::keyboard) {
            assert(key.x * layout.unit + key.w * layout.unit <= layout.width + .01f);
            assert(key.y * layout.unit + key.h * layout.unit <= layout.height + .01f);
            assert(gui::keyboard_layout::font_size(key) >= 12.5f);
        }
    }
    std::cout << "PASS unified keybind categories and readable 104-key layout\n";
}
