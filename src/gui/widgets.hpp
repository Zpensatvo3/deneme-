#pragma once

#include <imgui/imgui.h>
#include <string>

namespace gui
{
    enum class action_tone
    {
        neutral,
        accent,
        danger
    };

    // Shared action control used by Configs and future settings pages.
    bool centered_text_button(const char* label, const ImVec2& size);
    ImVec2 page_header(const char* title, const char* subtitle);
    bool action_button(const char* label, const ImVec2& size, action_tone tone = action_tone::neutral);
    bool icon_action(const char* id, const char* glyph, const char* fallback, const char* tooltip,
        bool danger = false, bool accent = false);
    void list_row_background(const ImVec2& minimum, const ImVec2& maximum, bool hovered, bool selected = false);
    void reset_widget_runtime_state();

    bool option_toggle(const char* label, bool* value);
    bool option_toggle(const char* label, bool* value, bool highlight_row);
    bool option_slider(const char* label, float* value, float minimum, float maximum,
        const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    bool option_slider(const char* label, int* value, int minimum, int maximum,
        const char* format = "%d", ImGuiSliderFlags flags = 0);
    bool option_range(const char* label, float value[2], float minimum, float maximum,
        const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    bool option_range(const char* label, int value[2], int minimum, int maximum,
        const char* format = "%d", ImGuiSliderFlags flags = 0);
    bool option_combo(const char* label, int* current_item, const char* const items[], int item_count,
        int popup_max_height_in_items = -1);
    bool option_color(const char* label, float color[4], ImGuiColorEditFlags flags = 0);
    void option_section(const char* label);
}

// Compatibility entry points used by module option renderers. Keeping the
// signatures aligned with Dear ImGui means module behavior stays untouched
// while every option receives the same OneStep presentation.
namespace ImGui
{
    bool OneStepCheckbox(const char* label, bool* value);
    bool OneStepSliderFloat(const char* label, float* value, float minimum, float maximum,
        const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    bool OneStepSliderInt(const char* label, int* value, int minimum, int maximum,
        const char* format = "%d", ImGuiSliderFlags flags = 0);
    bool OneStepSliderFloat2(const char* label, float value[2], float minimum, float maximum,
        const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    bool OneStepSliderInt2(const char* label, int value[2], int minimum, int maximum,
        const char* format = "%d", ImGuiSliderFlags flags = 0);
    bool OneStepCombo(const char* label, int* current_item, const char* const items[], int item_count,
        int popup_max_height_in_items = -1);
    bool OneStepColorEdit4(const char* label, float color[4], ImGuiColorEditFlags flags = 0);
    void OneStepSeparatorText(const char* label);
    void OneStepStaticDescription(const char* format, ...);
}
