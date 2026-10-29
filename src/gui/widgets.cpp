#include "widgets.hpp"
#include "gui.hpp"
#include "ui_state.hpp"
#include "control_draw.hpp"
#include "popup_controls.hpp"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <unordered_map>
#include <string>

namespace
{
    struct animation_state
    {
        float toggle = 0.0f;
        float hover = 0.0f;
        bool color_open = false;
        float color_popup = 0.0f;
    };

    std::unordered_map<ImGuiID, animation_state> animations;

    constexpr float option_right_gutter = 0.0f;
    constexpr float toggle_height = 36.0f;
    constexpr float slider_height = 48.0f;

    void draw_option_row(ImDrawList* draw, const ImVec2& start, float width, float height, float hover)
    {
        // A single quiet list, with only the current row receiving a surface.
        if (hover > 0.01f)
            draw->AddRectFilled(start, ImVec2(start.x + width, start.y + height),
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.025f * hover)), 5.0f);
        draw->AddLine(ImVec2(start.x + 10.0f, start.y + height),
            ImVec2(start.x + width - 10.0f, start.y + height),
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.045f)));
    }


    const char* visible_end(const char* label)
    {
        const char* marker = std::strstr(label, "##");
        return marker ? marker : label + std::strlen(label);
    }

    float approach(float current, float target, float speed = 12.0f)
    {
        if (gui::ui_state::get_preferences().reduce_motion) return target;
        const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0001f, 0.05f);
        const float diff = target - current;
        if (std::abs(diff) < 0.0005f)
            return target;
        const float amount = 1.0f - std::exp(-speed * dt);
        return current + diff * amount;
    }

    ImU32 with_alpha(const ImVec4& color, float alpha)
    {
        ImVec4 value = color;
        value.w *= alpha;
        return ImGui::GetColorU32(value);
    }

    void draw_text_right(ImDrawList* draw, ImFont* font, float size, const ImVec2& right,
        ImU32 color, const char* text)
    {
        const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
        draw->AddText(font, size, ImVec2(right.x - extent.x, right.y), color, text);
    }

    std::string fit_option_value(ImFont* font, float size, float width, const char* text)
    {
        if (!text || width <= 0.f) return {};
        if (font->CalcTextSizeA(size, FLT_MAX, 0.f, text).x <= width) return text;
        constexpr const char* ellipsis = "...";
        const float ellipsis_width = font->CalcTextSizeA(size, FLT_MAX, 0.f, ellipsis).x;
        if (width <= ellipsis_width) return {};
        const char* remaining = text;
        font->CalcTextSizeA(size, width - ellipsis_width, 0.f, text, nullptr, &remaining);
        return std::string(text, remaining) + ellipsis;
    }

    void draw_label_clipped(ImDrawList* draw, ImFont* font, float size, const ImVec2& position,
        float maximum_x, ImU32 color, const char* label)
    {
        const ImVec4 clip(position.x, position.y - 2.0f, maximum_x, position.y + size + 4.0f);
        draw->AddText(font, size, position, color, label, visible_end(label), 0.0f, &clip);
    }

    template <typename T>
    bool scalar_slider(const char* label, T* value, T minimum, T maximum, const char* format)
    {
        ImGui::PushID(label);
        const ImGuiID id = ImGui::GetID("##option_slider");
        animation_state& anim = animations[id];
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float width = std::max(120.0f, ImGui::GetContentRegionAvail().x - option_right_gutter);
        const float height = slider_height;
        ImGui::InvisibleButton("##option_slider", ImVec2(width, height));
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        anim.hover = approach(anim.hover, hovered || held ? 1.0f : 0.0f);

        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float track_left = start.x + 10.0f;
        const float track_right = start.x + width - 10.0f;
        const float track_y = start.y + 37.0f;
        bool changed = false;

        if (held && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (mouse.y >= start.y + 24.0f)
            {
                if (mouse.x < track_left)
                {
                    const T step = std::is_integral_v<T> ? T(1) : T((maximum - minimum) / T(100));
                    const T next = std::max(minimum, T(*value - step));
                    changed = next != *value;
                    *value = next;
                }
                else if (mouse.x > track_right)
                {
                    const T step = std::is_integral_v<T> ? T(1) : T((maximum - minimum) / T(100));
                    const T next = std::min(maximum, T(*value + step));
                    changed = next != *value;
                    *value = next;
                }
                else
                {
                    const float ratio = std::clamp((mouse.x - track_left) / (track_right - track_left), 0.0f, 1.0f);
                    T next = T(minimum + T((maximum - minimum) * ratio));
                    if constexpr (std::is_integral_v<T>)
                        next = T(std::lround(static_cast<double>(minimum) + static_cast<double>(maximum - minimum) * ratio));
                    next = std::clamp(next, minimum, maximum);
                    changed = next != *value;
                    *value = next;
                }
            }
        }

        char value_text[64]{};
        if constexpr (std::is_integral_v<T>)
            std::snprintf(value_text, sizeof(value_text), format ? format : "%d", static_cast<int>(*value));
        else
            std::snprintf(value_text, sizeof(value_text), format ? format : "%.3f", static_cast<double>(*value));

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrabActive);
        const ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text);
        const ImVec2 value_extent = font->CalcTextSizeA(gui::typography::caption, FLT_MAX, 0.0f, value_text);
        draw_option_row(draw, start, width, height, anim.hover);
        draw_label_clipped(draw, font, gui::typography::control, ImVec2(start.x + 10.0f, start.y + 7.0f),
            start.x + width - value_extent.x - 24.0f, text_color, label);
        draw_text_right(draw, font, gui::typography::caption, ImVec2(start.x + width - 10.0f, start.y + 7.0f),
            ImGui::GetColorU32(ImGuiCol_TextDisabled), value_text);
		draw->AddRectFilled(ImVec2(track_left, track_y), ImVec2(track_right, track_y + 2.0f),
			IM_COL32(55, 59, 70, 220), 1.0f);
        const float ratio = maximum > minimum
            ? std::clamp(static_cast<float>(*value - minimum) / static_cast<float>(maximum - minimum), 0.0f, 1.0f)
            : 0.0f;
        const float thumb_x = track_left + (track_right - track_left) * ratio;
        draw->AddRectFilled(ImVec2(track_left, track_y), ImVec2(thumb_x, track_y + 2.0f), with_alpha(accent, 0.88f), 1.0f);
        draw->AddCircleFilled(ImVec2(thumb_x, track_y + 1.0f), 3.4f + anim.hover * 0.45f, with_alpha(accent, 1.0f));
        ImGui::PopID();
        return changed;
    }

    template <typename T>
    bool range_slider(const char* label, T value[2], T minimum, T maximum, const char* format)
    {
        ImGui::PushID(label);
        const ImGuiID id = ImGui::GetID("##option_range");
        animation_state& anim = animations[id];
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float width = std::max(120.0f, ImGui::GetContentRegionAvail().x - option_right_gutter);
        ImGui::InvisibleButton("##option_range", ImVec2(width, slider_height));
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        anim.hover = approach(anim.hover, hovered || held ? 1.0f : 0.0f);
        const float track_left = start.x + 10.0f;
        const float track_right = start.x + width - 10.0f;
        const float track_y = start.y + 37.0f;
        bool changed = false;

        if (held && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const float ratio = std::clamp((ImGui::GetIO().MousePos.x - track_left) / (track_right - track_left), 0.0f, 1.0f);
            T next = T(minimum + T((maximum - minimum) * ratio));
            if constexpr (std::is_integral_v<T>)
                next = T(std::lround(static_cast<double>(minimum) + static_cast<double>(maximum - minimum) * ratio));
            next = std::clamp(next, minimum, maximum);
            const float low_ratio = static_cast<float>(value[0] - minimum) / static_cast<float>(maximum - minimum);
            const float high_ratio = static_cast<float>(value[1] - minimum) / static_cast<float>(maximum - minimum);
            const int selected = std::abs(ratio - low_ratio) <= std::abs(ratio - high_ratio) ? 0 : 1;
            if (selected == 0) next = std::min(next, value[1]);
            else next = std::max(next, value[0]);
            changed = next != value[selected];
            value[selected] = next;
        }

        char first[48]{}, second[48]{}, combined[104]{};
        if constexpr (std::is_integral_v<T>)
        {
            std::snprintf(first, sizeof(first), format ? format : "%d", static_cast<int>(value[0]));
            std::snprintf(second, sizeof(second), format ? format : "%d", static_cast<int>(value[1]));
        }
        else
        {
            std::snprintf(first, sizeof(first), format ? format : "%.3f", static_cast<double>(value[0]));
            std::snprintf(second, sizeof(second), format ? format : "%.3f", static_cast<double>(value[1]));
        }
        std::snprintf(combined, sizeof(combined), "%s  -  %s", first, second);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrabActive);
        const ImVec2 value_extent = font->CalcTextSizeA(gui::typography::caption, FLT_MAX, 0.0f, combined);
        draw_option_row(draw, start, width, slider_height, anim.hover);
        draw_label_clipped(draw, font, gui::typography::control, ImVec2(start.x + 10.0f, start.y + 7.0f), start.x + width - value_extent.x - 24.0f,
            ImGui::GetColorU32(ImGuiCol_Text), label);
        draw_text_right(draw, font, gui::typography::caption, ImVec2(start.x + width - 10.0f, start.y + 7.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), combined);
        draw->AddRectFilled(ImVec2(track_left, track_y), ImVec2(track_right, track_y + 2.0f), IM_COL32(55, 59, 70, 220), 1.0f);
        const float low_x = track_left + (track_right - track_left) * std::clamp(static_cast<float>(value[0] - minimum) / static_cast<float>(maximum - minimum), 0.0f, 1.0f);
        const float high_x = track_left + (track_right - track_left) * std::clamp(static_cast<float>(value[1] - minimum) / static_cast<float>(maximum - minimum), 0.0f, 1.0f);
        draw->AddRectFilled(ImVec2(low_x, track_y), ImVec2(high_x, track_y + 2.0f), with_alpha(accent, 0.88f), 1.0f);
        draw->AddCircleFilled(ImVec2(low_x, track_y + 1.0f), 3.4f + anim.hover * 0.45f, with_alpha(accent, 1.0f));
        draw->AddCircleFilled(ImVec2(high_x, track_y + 1.0f), 3.4f + anim.hover * 0.45f, with_alpha(accent, 1.0f));
        ImGui::PopID();
        return changed;
    }
}

bool gui::centered_text_button(const char* label, const ImVec2& size)
{
    const ImVec2 button_min = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(label, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool pressed = ImGui::IsItemClicked();
    ImGui::GetWindowDrawList()->AddRectFilled(button_min, ImVec2(button_min.x + size.x, button_min.y + size.y),
        ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button)),
        ImGui::GetStyle().FrameRounding);
    const char* visible_end = std::strstr(label, "##");
    if (!visible_end) visible_end = label + std::strlen(label);
    const ImVec2 text_size = ImGui::CalcTextSize(label, visible_end);
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
        ImVec2(button_min.x + std::floor((size.x - text_size.x) * 0.5f), button_min.y + std::floor((size.y - text_size.y) * 0.5f)),
        ImGui::GetColorU32(ImGuiCol_Text), label, visible_end);
    return pressed;
}

ImVec2 gui::page_header(const char* title, const char* subtitle)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImFont* title_font = ui_bold_font ? ui_bold_font : ImGui::GetFont();
    const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    draw->AddRectFilled(start, ImVec2(start.x + 3.0f, start.y + 43.0f), with_alpha(accent, 0.88f), 1.5f);
    draw->AddText(title_font, 20.0f, ImVec2(start.x + 12.0f, start.y), IM_COL32(239, 240, 245, 255), title);
    if (subtitle && *subtitle)
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(start.x + 12.0f, start.y + 24.0f),
            IM_COL32(116, 117, 128, 255), subtitle);
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 52.0f));
    return start;
}

bool gui::action_button(const char* label, const ImVec2& size, action_tone tone)
{
    ImVec4 normal(0.038f, 0.038f, 0.050f, 0.64f);
    ImVec4 hovered(0.075f, 0.068f, 0.090f, 0.80f);
    ImVec4 active(0.105f, 0.090f, 0.130f, 0.88f);
    if (tone == action_tone::accent)
    {
        const ImVec4 accent_color = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
        normal = ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.86f);
        hovered = ImVec4(accent_color.x, accent_color.y, accent_color.z, 1.0f);
        active = ImVec4(accent_color.x * 0.82f, accent_color.y * 0.82f, accent_color.z * 0.82f, 1.0f);
    }
    else if (tone == action_tone::danger)
    {
        normal = ImVec4(0.43f, 0.075f, 0.11f, 0.90f);
        hovered = ImVec4(0.58f, 0.09f, 0.14f, 1.0f);
        active = ImVec4(0.36f, 0.055f, 0.085f, 1.0f);
    }
    ImGui::PushStyleColor(ImGuiCol_Button, normal);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
    const bool clicked = centered_text_button(label, size);
    ImGui::PopStyleColor(3);
    return clicked;
}

bool gui::icon_action(const char* id, const char* glyph, const char* fallback, const char* tooltip,
    bool danger, bool accent)
{
    ImGui::PushID(id);
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImVec2 size(30.0f, 30.0f);
    ImGui::InvisibleButton("##icon_action", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool clicked = ImGui::IsItemClicked();
    const ImVec4 accent_color = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (hovered || held || accent)
    {
        const ImU32 background = danger
            ? IM_COL32(126, 32, 48, held ? 118 : (hovered ? 82 : 44))
            : with_alpha(accent_color, held ? 0.24f : (hovered ? 0.15f : 0.09f));
        draw->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y), background, 7.0f);
    }
    ImFont* font = icon_font ? icon_font : ImGui::GetFont();
    const char* text = icon_font && glyph && *glyph ? glyph : fallback;
    const float text_size = icon_font ? 16.0f : 14.0f;
    const ImVec2 extent = font->CalcTextSizeA(text_size, FLT_MAX, 0.0f, text);
    const ImU32 color = danger
        ? (hovered ? IM_COL32(247, 87, 103, 255) : IM_COL32(147, 149, 159, 255))
        : (hovered || accent ? with_alpha(accent_color, 1.0f) : IM_COL32(149, 151, 163, 255));
    draw->AddText(font, text_size,
        ImVec2(position.x + std::floor((size.x - extent.x) * 0.5f),
            position.y + std::floor((size.y - extent.y) * 0.5f)), color, text);
    if (hovered && tooltip && *tooltip)
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(tooltip);
        ImGui::EndTooltip();
    }
    ImGui::PopID();
    return clicked;
}

void gui::reset_widget_runtime_state()
{
    animations.clear();
}
void gui::list_row_background(const ImVec2& minimum, const ImVec2& maximum, bool hovered, bool selected)
{
    const ImVec4 accent_color = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(minimum, maximum,
        selected ? with_alpha(accent_color, hovered ? 0.16f : 0.10f)
            : (hovered ? IM_COL32(35, 33, 41, 148) : IM_COL32(255, 255, 255, 4)), 7.0f);
    if (selected)
        draw->AddRect(minimum, maximum, with_alpha(accent_color, 0.28f), 7.0f);
}

bool gui::option_toggle(const char* label, bool* value)
{
    return option_toggle(label, value, true);
}

bool gui::option_toggle(const char* label, bool* value, bool highlight_row)
{
    ImGui::PushID(label);
    const ImGuiID id = ImGui::GetID("##option_toggle");
    animation_state& anim = animations[id];
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = std::max(120.0f, ImGui::GetContentRegionAvail().x - option_right_gutter);
    ImGui::InvisibleButton("##option_toggle", ImVec2(width, toggle_height));
    const bool pressed = ImGui::IsItemClicked();
    if (pressed)
    {
        *value = !*value;
    }
    anim.toggle = approach(anim.toggle, *value ? 1.0f : 0.0f, 13.0f);
    anim.hover = approach(anim.hover, ImGui::IsItemHovered() ? 1.0f : 0.0f, 13.0f);

    const float eased = anim.toggle * anim.toggle * (3.0f - 2.0f * anim.toggle);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    draw_option_row(draw, start, width, toggle_height, highlight_row ? anim.hover : 0.f);
    draw_label_clipped(draw, font, typography::control, ImVec2(start.x + 10.0f, start.y + 9.0f), start.x + width - 52.0f,
        ImGui::GetColorU32(ImGuiCol_Text), label);
    const ImVec2 toggle_min(start.x + width - 42.0f, start.y + 8.0f);
    const ImVec2 toggle_max(start.x + width - 10.0f, start.y + 26.0f);
    const ImVec4 off(0.20f, 0.20f, 0.24f, 1.0f);
    const ImVec4 on = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    const ImVec4 mixed(off.x + (on.x - off.x) * eased,
        off.y + (on.y - off.y) * eased,
        off.z + (on.z - off.z) * eased, 1.0f);

    // Snap the track and the knob to whole pixels so both rounded ends share one
    // raster grid; fractional coordinates make the capsule edges stair-step.
    const ImVec2 track_min(std::floor(toggle_min.x), std::floor(toggle_min.y));
    const ImVec2 track_max(std::ceil(toggle_max.x), std::ceil(toggle_max.y));
    draw->AddRectFilled(track_min, track_max, ImGui::GetColorU32(mixed),
        (track_max.y - track_min.y) * 0.5f);

    const float knob_radius = 6.0f;
    const float knob_x = std::floor(toggle_min.x + 9.0f + std::round(eased * 14.0f));
    const float knob_y = std::floor(toggle_min.y + 9.0f);
    draw->AddCircleFilled(ImVec2(knob_x, knob_y), knob_radius,
        ImGui::GetColorU32(ImVec4(0.94f, 0.94f, 0.97f, 1.0f)), 0);
    ImGui::PopID();
    return pressed;
}

bool gui::option_slider(const char* label, float* value, float minimum, float maximum, const char* format, ImGuiSliderFlags)
{
    return scalar_slider(label, value, minimum, maximum, format);
}

bool gui::option_slider(const char* label, int* value, int minimum, int maximum, const char* format, ImGuiSliderFlags)
{
    return scalar_slider(label, value, minimum, maximum, format);
}

bool gui::option_range(const char* label, float value[2], float minimum, float maximum, const char* format, ImGuiSliderFlags)
{
    return range_slider(label, value, minimum, maximum, format);
}

bool gui::option_range(const char* label, int value[2], int minimum, int maximum, const char* format, ImGuiSliderFlags)
{
    return range_slider(label, value, minimum, maximum, format);
}

bool gui::option_combo(const char* label, int* current_item, const char* const items[], int item_count, int popup_max_height_in_items)
{
    if (!current_item || !items || item_count <= 0) return false;
    *current_item = std::clamp(*current_item, 0, item_count - 1);
    ImGui::PushID(label);
    const ImGuiID id = ImGui::GetID("##option_combo");
    animation_state& anim = animations[id];
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = std::max(120.f, ImGui::GetContentRegionAvail().x - option_right_gutter);
    ImFont* font = ImGui::GetFont();
    constexpr float value_size = typography::control;
    constexpr float value_gutter = 20.f;
    // Use all choices, not the current value, to keep the field stable.
    float longest_value = 0.f;
    for (int index = 0; index < item_count; ++index)
        longest_value = std::max(longest_value, font->CalcTextSizeA(value_size, FLT_MAX, 0.f, items[index]).x);
    const float control_right = start.x + width - 10.f;
    const float desired_width = std::max(120.f, longest_value + value_gutter * 2.f + 8.f);
    const float control_width = std::min(desired_width, width - 20.f);
    const float label_width = font->CalcTextSizeA(typography::control, FLT_MAX, 0.f, label, visible_end(label)).x;
    const bool stacked = label_width + control_width + 28.f > width;
    const float row_height = stacked ? 60.f : toggle_height;
    const float control_left = stacked ? start.x + 10.f : control_right - control_width;
    const ImVec2 control_min(control_left, start.y + (stacked ? 29.f : 4.f));
    const ImVec2 control_max(control_right, start.y + row_height - 4.f);
    ImGui::InvisibleButton("##option_combo", ImVec2(width, row_height));
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) ImGui::OpenPopup("##option_choices");
    const bool open = ImGui::IsPopupOpen("##option_choices");
    anim.hover = approach(anim.hover, hovered || open ? 1.f : 0.f);
    const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw_option_row(draw, start, width, row_height, anim.hover);
    draw->AddRectFilled(control_min, control_max,
        ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, .045f + .035f * anim.hover)), 6.f);
    draw->AddRect(control_min, control_max, with_alpha(accent, .24f * anim.hover), 6.f);
    draw_label_clipped(draw, font, typography::control, ImVec2(start.x + 10.f, start.y + 9.f),
        stacked ? control_right : std::max(start.x + 10.f, control_left - 8.f),
        ImGui::GetColorU32(ImGuiCol_Text), label);

    // Keep equal space on both sides: reserving only the arrow's side
    // shifts short values away from the actual center of the field.
    const float value_left = control_left + value_gutter;
    const float value_right = control_right - value_gutter;
    const char* selected = items[*current_item];
    const std::string fitted = fit_option_value(font, value_size, value_right - value_left, selected);
    gui::control_draw::centered_label(draw, font, value_size,
        ImVec2(value_left, control_min.y), ImVec2(value_right, control_max.y),
        ImGui::GetColorU32(ImGuiCol_Text), fitted.c_str(), ImVec2(0.f, 3.f));
    const float arrow_x = std::floor(control_right - 11.f) + .5f;
    const float arrow_y = std::floor((control_min.y + control_max.y) * .5f) + .5f;
    const float direction = open ? -1.f : 1.f;
    const ImU32 arrow_color = open ? with_alpha(accent, 1.f)
        : ImGui::GetColorU32(ImGuiCol_Text);
    draw->PathLineTo(ImVec2(arrow_x - 3.f, arrow_y - 1.5f * direction));
    draw->PathLineTo(ImVec2(arrow_x, arrow_y + 1.5f * direction));
    draw->PathLineTo(ImVec2(arrow_x + 3.f, arrow_y - 1.5f * direction));
    draw->PathStroke(arrow_color, 0, 1.4f);
    // The full-width stacked field handles long values without floating labels.

    bool changed = false;
    if (open)
    {
        const int visible_items = std::min(item_count, popup_max_height_in_items > 0 ? popup_max_height_in_items : 8);
        const float popup_width = std::min(width - 20.f, std::max(control_right - control_left, longest_value + 58.f));
        const float popup_height = 2.f * typography::popup_padding + visible_items * typography::choice_height
            + std::max(0, visible_items - 1) * typography::popup_gap;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float popup_x = std::clamp(control_right - popup_width, viewport->WorkPos.x,
            std::max(viewport->WorkPos.x, viewport->WorkPos.x + viewport->WorkSize.x - popup_width));
        const float popup_y = start.y + row_height + popup_height > viewport->WorkPos.y + viewport->WorkSize.y
            ? std::max(viewport->WorkPos.y, start.y - popup_height) : start.y + row_height;
        ImGui::SetNextWindowPos(ImVec2(popup_x, popup_y));
        ImGui::SetNextWindowSizeConstraints(ImVec2(popup_width, 0.f), ImVec2(popup_width, popup_height));
        popup_controls::style popup_style;
        if (ImGui::BeginPopup("##option_choices", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar))
        {
          {
            typography::scoped_font popup_font(font);
            for (int index = 0; index < item_count; ++index)
            {
                ImGui::PushID(index);
                const bool item_selected = index == *current_item;
                const bool clicked = popup_controls::choice("##choice", items[index], item_selected, font, accent);
                if (clicked)
                {
                    changed = *current_item != index;
                    *current_item = index;
                }
                ImGui::PopID();
            }
          }
            ImGui::EndPopup();
        }
    }
    ImGui::PopID();
    return changed;
}

bool gui::option_color(const char* label, float color[4], ImGuiColorEditFlags)
{
    if (!color) return false;
    ImGui::PushID(label);
    const ImGuiID id = ImGui::GetID("##option_color");
    animation_state& anim = animations[id];
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = std::max(150.0f, ImGui::GetContentRegionAvail().x - option_right_gutter);
    ImGui::InvisibleButton("##option_color", ImVec2(width, toggle_height));
    if (ImGui::IsItemClicked()) anim.color_open = !anim.color_open;
    anim.hover = approach(anim.hover, ImGui::IsItemHovered() || anim.color_open ? 1.0f : 0.0f);
    anim.color_popup = approach(anim.color_popup, anim.color_open ? 1.0f : 0.0f, 18.0f);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
    draw_option_row(draw, start, width, toggle_height, anim.hover);
    draw_label_clipped(draw, ImGui::GetFont(), typography::control, ImVec2(start.x + 10.0f, start.y + 9.0f),
        start.x + width - 52.0f, text, label);
    const ImVec2 swatch_min(start.x + width - 43.0f, start.y + 7.0f);
    const ImVec2 swatch_max(start.x + width - 8.0f, start.y + 27.0f);
    draw->AddRectFilled(swatch_min, swatch_max, ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3])), 6.0f);
    draw->AddRect(swatch_min, swatch_max, IM_COL32(255, 255, 255, 38), 6.0f);

    const float eased = anim.color_popup * anim.color_popup * (3.0f - 2.0f * anim.color_popup);
    bool changed = false;
    if (eased > 0.01f)
    {
        const float panel_height = 186.0f * eased;
        const ImVec2 panel_min(start.x, start.y + toggle_height + 4.0f);
        const ImVec2 panel_max(start.x + width, panel_min.y + panel_height);
        draw->PushClipRect(panel_min, panel_max, true);
        draw->AddRectFilled(panel_min, panel_max, IM_COL32(15, 14, 20, static_cast<int>(246.0f * eased)), 8.0f);
        draw->AddRect(panel_min, panel_max, IM_COL32(255, 255, 255, static_cast<int>(13.0f * eased)), 8.0f);

        float hue = 0.0f, saturation = 0.0f, value = 0.0f;
        ImGui::ColorConvertRGBtoHSV(color[0], color[1], color[2], hue, saturation, value);
        if (saturation <= 0.0001f) hue = 0.58f;
        float hue_r = 1.0f, hue_g = 0.0f, hue_b = 0.0f;
        ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, hue_r, hue_g, hue_b);

        const ImVec2 sv_min(panel_min.x + 10.0f, panel_min.y + 10.0f);
        const ImVec2 sv_max(panel_min.x + width - 34.0f, panel_min.y + 128.0f);
        draw->AddRectFilledMultiColor(sv_min, sv_max, IM_COL32_WHITE,
            ImGui::ColorConvertFloat4ToU32(ImVec4(hue_r, hue_g, hue_b, 1.0f)), IM_COL32_BLACK, IM_COL32_BLACK);
        ImGui::SetCursorScreenPos(sv_min);
        ImGui::InvisibleButton("##sv", ImVec2(sv_max.x - sv_min.x, sv_max.y - sv_min.y));
        if (ImGui::IsItemActive())
        {
            saturation = std::clamp((ImGui::GetIO().MousePos.x - sv_min.x) / (sv_max.x - sv_min.x), 0.0f, 1.0f);
            value = 1.0f - std::clamp((ImGui::GetIO().MousePos.y - sv_min.y) / (sv_max.y - sv_min.y), 0.0f, 1.0f);
            ImGui::ColorConvertHSVtoRGB(hue, saturation, value, color[0], color[1], color[2]);
            changed = true;
        }
        draw->AddCircle(ImVec2(sv_min.x + saturation * (sv_max.x - sv_min.x),
            sv_min.y + (1.0f - value) * (sv_max.y - sv_min.y)), 5.0f, IM_COL32_WHITE, 0, 1.5f);

        const ImVec2 hue_min(sv_max.x + 8.0f, sv_min.y);
        const ImVec2 hue_max(hue_min.x + 12.0f, sv_max.y);
        static constexpr ImU32 hue_colors[] = {
            IM_COL32(255,0,0,255), IM_COL32(255,255,0,255), IM_COL32(0,255,0,255),
            IM_COL32(0,255,255,255), IM_COL32(0,0,255,255), IM_COL32(255,0,255,255), IM_COL32(255,0,0,255)
        };
        for (int index = 0; index < 6; ++index)
        {
            const float y0 = hue_min.y + (hue_max.y - hue_min.y) * (static_cast<float>(index) / 6.0f);
            const float y1 = hue_min.y + (hue_max.y - hue_min.y) * (static_cast<float>(index + 1) / 6.0f);
            draw->AddRectFilledMultiColor(ImVec2(hue_min.x, y0), ImVec2(hue_max.x, y1),
                hue_colors[index], hue_colors[index], hue_colors[index + 1], hue_colors[index + 1]);
        }
        ImGui::SetCursorScreenPos(hue_min);
        ImGui::InvisibleButton("##hue", ImVec2(hue_max.x - hue_min.x, hue_max.y - hue_min.y));
        if (ImGui::IsItemActive())
        {
            hue = std::clamp((ImGui::GetIO().MousePos.y - hue_min.y) / (hue_max.y - hue_min.y), 0.0f, 1.0f);
            ImGui::ColorConvertHSVtoRGB(hue, saturation, value, color[0], color[1], color[2]);
            changed = true;
        }
        const float hue_y = hue_min.y + hue * (hue_max.y - hue_min.y);
        draw->AddRect(ImVec2(hue_min.x - 2.0f, hue_y - 2.0f), ImVec2(hue_max.x + 2.0f, hue_y + 2.0f), IM_COL32_WHITE, 1.0f);

        const ImVec2 alpha_min(panel_min.x + 10.0f, panel_min.y + 140.0f);
        const ImVec2 alpha_max(panel_min.x + width - 10.0f, panel_min.y + 151.0f);
        const ImU32 transparent = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], 0.0f));
        const ImU32 opaque = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], 1.0f));
        draw->AddRectFilledMultiColor(alpha_min, alpha_max, transparent, opaque, opaque, transparent);
        ImGui::SetCursorScreenPos(alpha_min);
        ImGui::InvisibleButton("##alpha", ImVec2(alpha_max.x - alpha_min.x, alpha_max.y - alpha_min.y));
        if (ImGui::IsItemActive())
        {
            color[3] = std::clamp((ImGui::GetIO().MousePos.x - alpha_min.x) / (alpha_max.x - alpha_min.x), 0.0f, 1.0f);
            changed = true;
        }
        const float alpha_x = alpha_min.x + color[3] * (alpha_max.x - alpha_min.x);
        draw->AddCircleFilled(ImVec2(alpha_x, (alpha_min.y + alpha_max.y) * 0.5f), 4.0f, IM_COL32_WHITE);

        char hex[16]{};
        std::snprintf(hex, sizeof(hex), "#%02X%02X%02X  %d%%",
            static_cast<int>(color[0] * 255.0f), static_cast<int>(color[1] * 255.0f),
            static_cast<int>(color[2] * 255.0f), static_cast<int>(color[3] * 100.0f));
        draw->AddText(ImGui::GetFont(), typography::caption, ImVec2(panel_min.x + 10.0f, panel_min.y + 161.0f),
            IM_COL32(188, 184, 198, static_cast<int>(255.0f * eased)), hex);
        draw->PopClipRect();
        ImGui::SetCursorScreenPos(ImVec2(start.x, panel_max.y + 5.0f));
        ImGui::Dummy(ImVec2(0.f, 0.f));
    }
    ImGui::PopID();
    return changed;
}

void gui::option_section(const char* label)
{
    if (!label || !*label) return;
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    const ImVec2 position = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), typography::caption, position,
        IM_COL32(132, 136, 149, 255), label, visible_end(label));
    ImGui::Dummy(ImVec2(0.0f, 17.0f));
}

bool ImGui::OneStepCheckbox(const char* label, bool* value) { return gui::option_toggle(label, value); }
bool ImGui::OneStepSliderFloat(const char* label, float* value, float minimum, float maximum, const char* format, ImGuiSliderFlags flags)
{ return gui::option_slider(label, value, minimum, maximum, format, flags); }
bool ImGui::OneStepSliderInt(const char* label, int* value, int minimum, int maximum, const char* format, ImGuiSliderFlags flags)
{ return gui::option_slider(label, value, minimum, maximum, format, flags); }
bool ImGui::OneStepCombo(const char* label, int* current_item, const char* const items[], int item_count, int popup_max_height_in_items)
{ return gui::option_combo(label, current_item, items, item_count, popup_max_height_in_items); }
bool ImGui::OneStepColorEdit4(const char* label, float color[4], ImGuiColorEditFlags flags)
{ return gui::option_color(label, color, flags); }
void ImGui::OneStepSeparatorText(const char* label) { gui::option_section(label); }
