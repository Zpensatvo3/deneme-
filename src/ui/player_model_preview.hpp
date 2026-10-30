#pragma once
#include <imgui/imgui.h>
#include <array>
#include <cmath>
#include <cstdint>

namespace gui::player_model_preview
{
    // Shared Player ESP / Aim Regions skin renderer; no game state is mutated.
    inline void draw(ImDrawList* draw, const ImVec2& position, std::uintptr_t skin_texture_value,
        float yaw, ImU32 body, ImU32 body_dark, float scale = 1.f)
    {
        const int first_vertex = draw->VtxBuffer.Size;
        if (skin_texture_value)
        {
            const ImTextureID skin = reinterpret_cast<ImTextureID>(skin_texture_value);
            struct face_uv
            {
                float u0;
                float v0;
                float u1;
                float v1;
            };
            float normalized_yaw = std::fmod(yaw, 6.28318530718f);
            if (normalized_yaw < 0.0f) normalized_yaw += 6.28318530718f;
            const int direction = static_cast<int>(std::floor((normalized_yaw + 0.78539816339f) / 1.57079632679f)) % 4;
            const float model_scale_x = direction == 1 || direction == 3 ? 0.70f : 1.0f;
            auto skin_part = [&](const ImVec2& minimum, const ImVec2& maximum,
                const std::array<face_uv, 4>& faces)
            {
                const float model_center = position.x + 130.0f;
                const float part_center = model_center + ((minimum.x + maximum.x) * 0.5f - model_center) * model_scale_x;
                const float half_width = (maximum.x - minimum.x) * 0.5f * model_scale_x;
                const ImVec2 scaled_min(part_center - half_width, minimum.y);
                const ImVec2 scaled_max(part_center + half_width, maximum.y);
                const face_uv& uv = faces[static_cast<std::size_t>(direction)];
                draw->AddImage(skin, scaled_min, scaled_max,
                    ImVec2(uv.u0 / 64.0f, uv.v0 / 64.0f), ImVec2(uv.u1 / 64.0f, uv.v1 / 64.0f));
            };
            const std::array<face_uv, 4> head{{ { 8, 8, 16, 16 }, { 0, 8, 8, 16 }, { 24, 8, 32, 16 }, { 16, 8, 24, 16 } }};
            const std::array<face_uv, 4> hat{{ { 40, 8, 48, 16 }, { 32, 8, 40, 16 }, { 56, 8, 64, 16 }, { 48, 8, 56, 16 } }};
            const std::array<face_uv, 4> torso{{ { 20, 20, 28, 32 }, { 16, 20, 20, 32 }, { 32, 20, 40, 32 }, { 28, 20, 32, 32 } }};
            const std::array<face_uv, 4> right_arm{{ { 44, 20, 48, 32 }, { 40, 20, 44, 32 }, { 52, 20, 56, 32 }, { 48, 20, 52, 32 } }};
            const std::array<face_uv, 4> left_arm{{ { 36, 52, 40, 64 }, { 32, 52, 36, 64 }, { 44, 52, 48, 64 }, { 40, 52, 44, 64 } }};
            const std::array<face_uv, 4> right_leg{{ { 4, 20, 8, 32 }, { 0, 20, 4, 32 }, { 12, 20, 16, 32 }, { 8, 20, 12, 32 } }};
            const std::array<face_uv, 4> left_leg{{ { 20, 52, 24, 64 }, { 16, 52, 20, 64 }, { 28, 52, 32, 64 }, { 24, 52, 28, 64 } }};
            skin_part(ImVec2(position.x + 100.0f, position.y + 101.0f), ImVec2(position.x + 160.0f, position.y + 161.0f), head);
            skin_part(ImVec2(position.x + 100.0f, position.y + 101.0f), ImVec2(position.x + 160.0f, position.y + 161.0f), hat);
            skin_part(ImVec2(position.x + 94.0f, position.y + 165.0f), ImVec2(position.x + 166.0f, position.y + 261.0f), torso);
            skin_part(ImVec2(position.x + 70.0f, position.y + 165.0f), ImVec2(position.x + 94.0f, position.y + 261.0f), right_arm);
            skin_part(ImVec2(position.x + 166.0f, position.y + 165.0f), ImVec2(position.x + 190.0f, position.y + 261.0f), left_arm);
            skin_part(ImVec2(position.x + 98.0f, position.y + 261.0f), ImVec2(position.x + 128.0f, position.y + 328.0f), right_leg);
            skin_part(ImVec2(position.x + 132.0f, position.y + 261.0f), ImVec2(position.x + 162.0f, position.y + 328.0f), left_leg);
        }
        else
        {
            draw->AddRectFilled(ImVec2(position.x + 100.0f, position.y + 101.0f), ImVec2(position.x + 160.0f, position.y + 161.0f), body, 2.0f);
            draw->AddRectFilled(ImVec2(position.x + 94.0f, position.y + 165.0f), ImVec2(position.x + 166.0f, position.y + 261.0f), body, 2.0f);
            draw->AddRectFilled(ImVec2(position.x + 70.0f, position.y + 165.0f), ImVec2(position.x + 94.0f, position.y + 261.0f), body_dark, 2.0f);
            draw->AddRectFilled(ImVec2(position.x + 166.0f, position.y + 165.0f), ImVec2(position.x + 190.0f, position.y + 261.0f), body_dark, 2.0f);
            draw->AddRectFilled(ImVec2(position.x + 98.0f, position.y + 261.0f), ImVec2(position.x + 128.0f, position.y + 328.0f), body_dark, 2.0f);
            draw->AddRectFilled(ImVec2(position.x + 132.0f, position.y + 261.0f), ImVec2(position.x + 162.0f, position.y + 328.0f), body_dark, 2.0f);
        }
        if (scale != 1.f) {
            for (int i = first_vertex; i < draw->VtxBuffer.Size; ++i) {
                auto& point = draw->VtxBuffer[i].pos;
                point.x = position.x + (point.x - position.x) * scale;
                point.y = position.y + (point.y - position.y) * scale;
            }
        }
    }
}
