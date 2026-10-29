#include "../src/gui/popup_controls.hpp"
#include "../src/gui/surface_style.hpp"
#include "../src/ui/keyboard_layout.hpp"
#include "../src/ui/block_picker.hpp"
#include "../src/ui/item_icon.hpp"
#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>

int main()
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {800, 600};
    io.DeltaTime = 1.f / 60.f;
    io.FontGlobalScale = 1.15f;
    const auto fonts = std::filesystem::path(__FILE__).parent_path().parent_path() / "assets/fonts";
    auto* regular = io.Fonts->AddFontFromFileTTF((fonts / "Montserrat-SemiBold.ttf").string().c_str(), 19.5f);
    auto* bold = io.Fonts->AddFontFromFileTTF((fonts / "Montserrat-Bold.ttf").string().c_str(), 22.5f);
    assert(regular && bold);
    unsigned char* pixels; int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    bool legends_fit = true;
    for (const auto& key : platform::keys::keyboard) {
        auto* face = gui::keyboard_layout::single_character(key) ? bold : regular;
        const auto ink = gui::control_draw::text_ink_bounds(face, gui::keyboard_layout::font_size(key), gui::keyboard_layout::legend(key));
        // The smallest supported keyboard must not silently shrink any legend.
        if (ink.z - ink.x > key.w * 29.f - gui::keyboard_layout::gap(29.f) - 4.f) {
            std::cerr << "Key legend too wide: " << key.label << " / " << gui::keyboard_layout::legend(key)
                << " (" << ink.z - ink.x << " px)\n";
            legends_fit = false;
        }
        assert(ink.w - ink.y <= key.h * 29.f - gui::keyboard_layout::gap(29.f) - 4.f);
    }
    assert(legends_fit);
    gui::surface_style::configure(ImGui::GetStyle());
    ImVec2 rows[3]{};
    int selection = 0;
    constexpr const char* choices[] = {"Public Configs", "My Configs", "Favourites"};
    const ImVec4 accent(.9f, .25f, .4f, 1.f);

    for (int frame = 0; frame < 5; ++frame) {
        if (frame == 3) { io.AddMousePosEvent(rows[1].x, rows[1].y); io.AddMouseButtonEvent(0, true); }
        if (frame == 4) io.AddMouseButtonEvent(0, false);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({20, 20});
        ImGui::SetNextWindowSize({600, 500});
        ImGui::Begin("Workspace", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetWindowFontScale(.8f);
        const float before = ImGui::GetFontSize();
        const int colors_before = GImGui->ColorStack.Size, vars_before = GImGui->StyleVarStack.Size;
        {
            gui::typography::scoped_font body(regular);
            assert(std::abs(ImGui::GetFontSize() - 15.f) < .01f);
            {
                gui::typography::scoped_font heading(bold, gui::typography::title);
                assert(std::abs(ImGui::GetFontSize() - 19.f) < .01f);
            }
            assert(std::abs(ImGui::GetFontSize() - 15.f) < .01f);
            ImGui::TextUnformatted("Delete config?");
        }
        assert(std::abs(ImGui::GetFontSize() - before) < .01f);
        if (frame == 0) ImGui::OpenPopup("Collection");
        {
            gui::popup_controls::style popup_style;
            ImGui::SetNextWindowPos({60, 100});
            ImGui::SetNextWindowSize({210, 132});
            if (ImGui::BeginPopup("Collection", ImGuiWindowFlags_NoSavedSettings)) {
                {
                    gui::typography::scoped_font popup_font(regular);
                    assert(std::abs(ImGui::GetFontSize() - 15.f) < .01f);
                    assert(ImGui::GetCurrentWindow()->WindowRounding == 12.f);
                    for (int i = 0; i < 3; ++i) {
                        ImGui::PushID(i);
                        if (gui::popup_controls::choice("##choice", choices[i], selection == i, regular, accent)) selection = i;
                        const auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
                        assert(std::abs(maximum.y - minimum.y - 36.f) < .01f);
                        rows[i] = {(minimum.x + maximum.x) * .5f, (minimum.y + maximum.y) * .5f};
                        assert(regular->CalcTextSizeA(15.f, FLT_MAX, 0.f, choices[i]).x <= maximum.x - minimum.x - 42.f);
                        ImGui::PopID();
                    }
                }
                ImGui::EndPopup();
            }
        }
        assert(GImGui->ColorStack.Size == colors_before && GImGui->StyleVarStack.Size == vars_before);
        ImGui::End();
        ImGui::Render();
    }
    assert(selection == 1);
    assert(gui::typography::readable(10.f) == 14.f);
    {
        // Exercise the shipping grid with a large registry but no JNI/game services.
        std::vector<gui::block_picker::entry> catalog;
        for (int index=0; index<1000; ++index)
        {
            const auto key = "minecraft:block_"+std::to_string(index);
            catalog.push_back({key,"Block "+std::to_string(index),key});
        }
        std::vector<std::string> selected;
        gui::block_picker::state picker;
        ImVec2 first_tile{};
        int drawn = 0;
        float horizontal_overflow = 0.f;
        auto render_picker = [&](float panel_width=280.f) {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({20,20}); ImGui::SetNextWindowSize({panel_width+16.f,380});
            ImGui::Begin("Block picker fixture",nullptr,ImGuiWindowFlags_NoSavedSettings);
            const int colors=GImGui->ColorStack.Size, vars=GImGui->StyleVarStack.Size;
            drawn=0;
            const bool changed=gui::block_picker::render(catalog,selected,picker,regular,accent,
                [&](const std::string&,ImDrawList* draw,ImVec2 minimum,ImVec2 maximum) {
                    if (!drawn) first_tile={(minimum.x+maximum.x)*.5f,(minimum.y+maximum.y)*.5f};
                    ++drawn; draw->AddRectFilled(minimum,maximum,IM_COL32_WHITE);
                });
            assert(GImGui->ColorStack.Size==colors && GImGui->StyleVarStack.Size==vars);
            horizontal_overflow=ImGui::GetCurrentWindow()->ScrollMax.x;
            ImGui::End(); ImGui::Render(); return changed;
        };
        for (float panel_width : {240.f,280.f,360.f}) {
            render_picker(panel_width); render_picker(panel_width);
            assert(horizontal_overflow<.5f && drawn>0 && drawn<60);
        }
        render_picker(); render_picker();
        io.AddMousePosEvent(first_tile.x,first_tile.y); render_picker();
        io.AddMouseButtonEvent(0,true); render_picker();
        io.AddMouseButtonEvent(0,false); assert(render_picker());
        assert(selected.size()==1 && selected[0]==catalog[0].key);
        picker.selected_only=true; render_picker(); render_picker(); assert(drawn==1);
        picker.selected_only=false; picker.search_open=true;
        std::strcpy(picker.search,"block_999"); render_picker(); render_picker(); assert(drawn==1);
        std::strcpy(picker.search,"absent_block"); render_picker(); assert(drawn==0);
    }
    {
        struct model_quad {
            std::array<std::array<float,3>,4> position{{{0,0,0},{1,0,0},{1,1,0},{0,1,0}}};
            std::array<std::array<float,2>,4> uv{{{0,0},{1,0},{1,1},{0,1}}};
            std::array<float,3> rotation{},translation{},scale{1,1,1};
            std::uintptr_t texture=7; std::uint32_t tint=0xffffffff; bool side_lit=false;
        };
        std::vector<model_quad> model(1);
        auto projected=gui::item_icons::project(model);
        assert(projected.size()==1 && projected[0].texture==7);
        assert(projected[0].position[0].x==0.f && projected[0].position[0].y==1.f);
        model[0].rotation[2]=90.f;
        projected=gui::item_icons::project(model);
        assert(projected.size()==1 && std::abs(projected[0].position[0].x-1.f)<.001f);
        model[0].rotation[1]=180.f; assert(gui::item_icons::project(model).empty());
        model[0].rotation={}; model[0].position[0][0]=NAN; assert(gui::item_icons::project(model).empty());
    }
    ImGui::DestroyContext();
    std::cout << "PASS readable fonts/popups, virtualized block grid, selection/filter, item projection and balanced style stacks\n";
}
