#include "module.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/control_draw.hpp"
#include "../../platform/input/key_codes.hpp"
#include <imgui/imgui.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

modules::module::module(const char* name, const char* category, const char* id)
    : enabled(this), id_storage(id && *id ? id : make_stable_id(name)), name(name), category(category)
{
    const auto definition = manager::find(id_storage);
    if (!definition) throw std::runtime_error("Module is not in the Java catalog: " + id_storage);
    // Empty labels occur only in inert UI fixtures. Production snapshots carry
    // the canonical catalog metadata before any native engine is constructed.
    if (!definition->name.empty()) this->name = definition->name;
    if (!definition->category.empty()) this->category = definition->category;
}

void modules::module::run(::cache&) {}
void modules::module::on_enable(::cache&) {}
void modules::module::on_disable(::cache&) {}
void modules::module::render_options() {}

int modules::module::get_keybind() const { const auto state = manager::find(get_id()); return state ? state->keybind : 0; }
modules::manager::result modules::module::set_keybind(int value) {
    return manager::keybind(get_id(), value);
}
const char* modules::module::get_id() const { return id_storage.c_str(); }
const char* modules::module::get_name() const { return name.c_str(); }
const char* modules::module::get_category() const { return category.c_str(); }

void modules::module::begin_keybind_capture()
{
    scanning = true;
    gui::reset_last_pressed_key();
}

std::string modules::module::make_stable_id(const char* value)
{
    std::string id;
    bool underscore_pending = false;
    for (const unsigned char character : std::string(value ? value : "module"))
    {
        if (std::isalnum(character))
        {
            if (underscore_pending && !id.empty()) id.push_back('_');
            id.push_back(static_cast<char>(std::tolower(character)));
            underscore_pending = false;
        }
        else
        {
            underscore_pending = !id.empty();
        }
    }
    return id.empty() ? "module" : id;
}

void modules::module::key_bind_selector(float width)
{
    const bool editable = supports_keybind();
    utils::null_string display = !editable ? "Right Shift" : (scanning ? "LISTENING" : keycode_to_string(get_keybind()));
    const ImVec2 button_size(std::max(1.f, width), 28.f);
    const ImVec2 button_min = ImGui::GetCursorScreenPos();
    ImGui::PushID(get_id());
    ImGui::InvisibleButton("##keybind", button_size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool pressed = editable && ImGui::IsItemClicked();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(button_min, ImVec2(button_min.x + button_size.x, button_min.y + button_size.y),
        ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button)),
        8.0f); // Keep this small badge rounded regardless of the parent page's style.
    ImFont* label_font = gui::ui_font ? gui::ui_font : ImGui::GetFont();
    gui::control_draw::centered_label(draw_list, label_font, 14.5f, button_min,
        ImVec2(button_min.x + button_size.x, button_min.y + button_size.y),
        ImGui::GetColorU32(ImGuiCol_Text), display.data);
    ImGui::PopID();
    if (pressed) begin_keybind_capture();
    if (editable && scanning)
    {
        int last_pressed = gui::get_last_pressed_key();
        if (!last_pressed) return;
        set_keybind(last_pressed == VK_ESCAPE ? 0 : last_pressed);
        scanning = false;
    }
}

utils::null_string modules::module::keycode_to_string(int keycode) const
{
    return platform::keys::label(keycode).c_str();
}
