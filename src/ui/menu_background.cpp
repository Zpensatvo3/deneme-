#include "menu_background.hpp"
#include "../gui/ui_state.hpp"
#include "../gui/ui_typography.hpp"
#include <cfloat>
#include <cstdio>

bool gui::background::render(ImDrawList* draw, ImVec2 minimum, ImVec2 size, const options& raw)
{
    options value = raw;
    normalize(value);
    if (!draw || value.selected == style::classic || value.opacity == 0) return false;
    const auto image = texture(value.selected);
    if (!image.id) return false;
    const auto uv = cover_uv(image.size, size);
    const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
    draw->AddImageRounded(image.id, minimum, maximum, uv.minimum, uv.maximum,
        IM_COL32(255, 255, 255, static_cast<int>(255 * value.opacity)), 20.f);
    draw->AddRectFilled(minimum, maximum,
        IM_COL32(3, 3, 5, static_cast<int>(255 * value.dimming)), 20.f);
    return true;
}

namespace
{
    constexpr ImU32 background_primary = IM_COL32(234, 234, 241, 255);
    constexpr ImU32 background_muted = IM_COL32(139, 137, 151, 255);

    void background_label(ImVec2 at, const char* text, float size = 12.f,
        ImU32 color = background_muted, float width = FLT_MAX)
    {
        size = gui::typography::readable(size);
        const ImVec4 clip(at.x, at.y, at.x + width, at.y + size + 4);
        ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), size, at,
            ImGui::GetColorU32(ImGui::ColorConvertU32ToFloat4(color)), text, nullptr, 0.f, &clip);
    }
    void background_slider(const char* id, const char* title, float* value, float low, float high, float width)
    {
        ImGui::PushID(id);
        const auto p = ImGui::GetCursorScreenPos();
        background_label(p, title, 11.f);
        char number[16];
        std::snprintf(number, sizeof(number), "%d%%", static_cast<int>(std::lround(*value * 100)));
        const auto extent = ImGui::GetFont()->CalcTextSizeA(gui::typography::caption, FLT_MAX, 0, number);
        background_label(ImVec2(p.x + width - extent.x, p.y), number, 11.f, background_primary);
        ImGui::Dummy(ImVec2(0, 14));
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 24));
        ImGui::SetNextItemWidth(width);
        if (ImGui::SliderFloat("##value", value, low, high, "", ImGuiSliderFlags_AlwaysClamp))
            gui::ui_state::mark_preferences_dirty();
        ImGui::PopID();
    }
}

void gui::background::render_settings(ImU32 accent)
{
    ImGui::PushID("menu_background");
    auto& value = ui_state::get_preferences().menu_background;
    const auto import = status();
    const bool choosing = import == import_state::choosing;
    const float width = ImGui::GetContentRegionAvail().x;
    auto* draw = ImGui::GetWindowDrawList();

    const auto heading = ImGui::GetCursorScreenPos();
    background_label(ImVec2(heading.x, heading.y + 8), "Menu background", gui::typography::control, background_primary, std::max(1.f, width - 132));
    ImGui::SetCursorScreenPos(ImVec2(heading.x + width - 126, heading.y));
    ImGui::BeginDisabled(choosing);
    const auto button = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton("##import", ImVec2(126, 34))) begin_import();
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", "PNG / JPG, up to 16 MB");
    draw->AddRectFilled(button, ImVec2(button.x + 126, button.y + 34), IM_COL32(255, 255, 255, hovered ? 15 : 6), 8.f);
    draw->AddRect(button, ImVec2(button.x + 126, button.y + 34), IM_COL32(255, 255, 255, hovered ? 30 : 18), 8.f);
    background_label(ImVec2(button.x + 25, button.y + 9), "Add image", gui::typography::caption, background_primary);
    const auto center = ImVec2(button.x + 13, button.y + 17);
    draw->AddLine(ImVec2(center.x - 3.5f, center.y), ImVec2(center.x + 3.5f, center.y), background_muted, 1.2f);
    draw->AddLine(ImVec2(center.x, center.y - 3.5f), ImVec2(center.x, center.y + 3.5f), background_muted, 1.2f);
    ImGui::EndDisabled();
    ImGui::SetCursorScreenPos(ImVec2(heading.x, heading.y + 38));

    const char* names[] = { "Classic", "Citadel", "Candy Dusk", "Nebula", "Aurora", "Custom image" };
    constexpr float gap = 10.f;
    const int columns = std::clamp(static_cast<int>((width + gap) / 180.f), 1, 3);
    const ImVec2 size(std::max(1.f, (width - gap * (columns - 1)) / columns), 94.f);
    const auto start = ImGui::GetCursorScreenPos();
    for (int i = 0; i < static_cast<int>(style::count); ++i) {
        ImGui::PushID(i);
        const auto preset = static_cast<style>(i);
        const auto image = preset == style::classic ? texture_view{} : texture(preset);
        const bool empty_custom = preset == style::custom && !image.id;
        const bool available = preset == style::classic || image.id || empty_custom;
        const float x = start.x + (i % columns) * (size.x + gap);
        const float y = start.y + (i / columns) * (size.y + gap);
        const ImVec2 p(std::floor(x), std::floor(y));
        const ImVec2 maximum(std::floor(x + size.x), std::floor(y + size.y));
        ImGui::SetCursorScreenPos(p);
        ImGui::BeginDisabled(!available || (empty_custom && choosing));
        if (ImGui::InvisibleButton("##select", ImVec2(maximum.x - p.x, maximum.y - p.y))) {
            if (empty_custom) begin_import();
            else if (value.selected != preset) {
                value.selected = preset;
                ui_state::mark_preferences_dirty();
            }
        }
        const bool hover = ImGui::IsItemHovered();
        ImGui::EndDisabled();
        const bool selected = value.selected == preset;
        // Draw the frame as a filled ring. The thumbnail and footer stay inside
        // it, so fractional stroke widths cannot overlap their textured edges.
        const float border = selected ? 2.f : 1.f;
        const ImVec2 inside(p.x + border, p.y + border);
        const ImVec2 inside_max(maximum.x - border, maximum.y - border);
        const ImVec2 image_max(inside_max.x, inside_max.y - 26.f);
        draw->AddRectFilled(p, maximum, selected ? accent : IM_COL32(hover ? 58 : 36, hover ? 55 : 34, hover ? 70 : 44, 255), 7.f);
        draw->AddRectFilled(inside, inside_max, IM_COL32(21, 20, 28, 255), 7.f - border);
        if (image.id) {
            const auto uv = cover_uv(image.size, ImVec2(image_max.x - inside.x, image_max.y - inside.y));
            draw->AddImageRounded(image.id, inside, image_max, uv.minimum, uv.maximum,
                IM_COL32_WHITE, 7.f - border, ImDrawFlags_RoundCornersTop);
            draw->AddRectFilled(inside, image_max, IM_COL32(0, 0, 0, hover ? 8 : 24),
                7.f - border, ImDrawFlags_RoundCornersTop);
        } else if (preset == style::classic) {
            // A miniature classic menu, instead of an empty black thumbnail.
            const ImVec2 mini(p.x + 17, p.y + 12), mini_max(maximum.x - 17, maximum.y - 33);
            draw->AddRectFilled(mini, mini_max, IM_COL32(9, 9, 14, 255), 4.f);
            draw->AddRectFilled(mini, ImVec2(mini.x + size.x * .18f, mini_max.y), IM_COL32(29, 26, 39, 255), 4.f, ImDrawFlags_RoundCornersLeft);
            draw->AddRectFilled(ImVec2(mini.x + 6, mini.y + 10), ImVec2(mini.x + size.x * .18f - 6, mini.y + 15), accent, 2.f);
            for (int row = 0; row < 3; ++row)
                draw->AddRectFilled(ImVec2(mini.x + size.x * .18f + 8, mini.y + 8 + row * 10),
                    ImVec2(mini_max.x - 8, mini.y + 13 + row * 10), IM_COL32(52, 48, 65, 255), 2.f);
        } else if (empty_custom) {
            const ImVec2 c(p.x + size.x * .5f, p.y + 31);
            draw->AddCircle(c, 12.f, IM_COL32(255, 255, 255, 27), 0, 1.f);
            draw->AddLine(ImVec2(c.x - 4, c.y), ImVec2(c.x + 4, c.y), background_muted, 1.3f);
            draw->AddLine(ImVec2(c.x, c.y - 4), ImVec2(c.x, c.y + 4), background_muted, 1.3f);
        }
        draw->AddRectFilled(ImVec2(inside.x, image_max.y), inside_max, IM_COL32(12, 11, 17, 255),
            7.f - border, ImDrawFlags_RoundCornersBottom);
        background_label(ImVec2(p.x + 10, maximum.y - 22), names[i], gui::typography::caption,
            available ? background_primary : background_muted, size.x - 20);
        if (selected) {
            const ImVec2 c(maximum.x - 14, p.y + 14);
            draw->AddCircleFilled(c, 7.f, accent);
            const ImVec2 check[] = { {c.x - 3, c.y}, {c.x - 1, c.y + 2}, {c.x + 3, c.y - 3} };
            draw->AddPolyline(check, 3, IM_COL32_WHITE, 0, 1.4f);
        }
        if (!available && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", "This background could not be loaded.");
        ImGui::PopID();
    }
    const int rows = (static_cast<int>(style::count) + columns - 1) / columns;
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + rows * (size.y + gap) - gap + 14));

    const auto controls = ImGui::GetCursorScreenPos();
    const bool side_by_side = width >= 360.f;
    const float control_width = side_by_side ? (width - 20) * .5f : width;
    ImGui::BeginDisabled(value.selected == style::classic);
    background_slider("opacity", "Image opacity", &value.opacity, 0.f, 1.f, control_width);
    if (side_by_side) ImGui::SetCursorScreenPos(ImVec2(controls.x + control_width + 20, controls.y));
    background_slider("dimming", "Darkening", &value.dimming, .25f, .85f, control_width);
    ImGui::EndDisabled();
    ImGui::Dummy(ImVec2(0, 3));

    const char* message = nullptr;
    bool error = false;
    if (choosing) message = "Choose an image in the file window.";
    else if (import == import_state::failed) {
        message = "Could not load image. Use PNG/JPG, up to 8192 px per side and 16 megapixels.";
        error = true;
    } else if (import == import_state::save_failed) {
        message = "Could not save image. Your previous background is unchanged.";
        error = true;
    } else if (value.selected == style::custom && !texture(style::custom).id) {
        message = "Saved image is missing. Classic is used until you add an image again.";
        error = true;
    }
    if (message) {
        ImGui::PushStyleColor(ImGuiCol_Text, error ? IM_COL32(240, 112, 128, 255) : background_muted);
        ImGui::PushTextWrapPos(0);
        ImGui::TextUnformatted(message);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }
    ImGui::PopID();
}
