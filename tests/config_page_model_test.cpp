#include "../src/ui/config_page_model.hpp"
#include <cassert>
#include <cstdio>

int main()
{
    namespace model = gui::config_page::model;
    configs::snapshot library;
    library.entries = {
        { 1, "Survival", "Wolf", "Calm HUD", 4, 80, "2026-10-01", "2026-10-12", true },
        { 2, "Arena", "Player", "Combat profile", 8, 210, "2026-10-02", "2026-10-10", false },
        { 3, "Builder", "Wolf", "Minimal layout", 2, 12, "2026-10-13", "", true },
        { 4, "Explorer", "Alex", "Night profile", 3, 20, "2026-10-14", "", false }
    };
    const std::unordered_set<int> favourites{ 2 };
    auto list = model::filter(library, model::collection::community, "", favourites, model::order::newest);
    assert(list.size() == 2 && list[0]->id == 4 && list[1]->id == 2);
    for (const auto* entry : list) assert(!entry->mine);
    list = model::filter(library, model::collection::community, "  COMBAT  ", favourites, model::order::popular);
    assert(list.size() == 1 && list[0]->id == 2);
    list = model::filter(library, model::collection::mine, "wolf", favourites, model::order::name);
    assert(list.size() == 2 && list[0]->id == 3);
    for (const auto* entry : list) assert(entry->mine);
    list = model::filter(library, model::collection::favourites, "", favourites, model::order::popular);
    assert(list.size() == 1 && list[0]->id == 2);
    assert(model::filter(library, model::collection::mine, "Arena", favourites, model::order::name).empty());
    assert(model::filter(library, model::collection::community, "Survival", favourites, model::order::name).empty());
    assert(model::trim("  \n \t").empty());
    char short_text[4]{};
    model::copy_utf8(short_text, "ab\xc4\xb1" "c");
    assert(std::string(short_text) == "ab");
    model::copy_utf8(short_text, "a\xc4\xb1" "c");
    assert(std::string(short_text) == "a\xc4\xb1");
    assert(model::date("2026-10-13T12:00:00") == "2026-10-13");
    assert(model::date("") == "--");
    std::puts("Config collection, description search, sorting, trim and UTF-8 boundary checks passed.");
}
