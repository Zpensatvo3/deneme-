#pragma once
#include <imgui/imgui.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace gui::item_icons
{
    struct quad
    {
        std::array<ImVec2, 4> position{}, uv{};
        std::uintptr_t texture = 0; // Borrowed atlas, never deleted by the UI.
        ImU32 tint = IM_COL32_WHITE;
        float depth = 0.f;
    };

    // Vanilla's GUI transform is T * Rx * Ry * Rz * S. No game objects are retained.
    template<class Model> std::vector<quad> project(const Model& model)
    {
        std::vector<quad> result;
        for (const auto& data : model)
        {
            if (!data.texture) continue;
            quad item;
            std::array<std::array<float, 3>, 4> vertices{};
            bool valid = true;
            for (int point = 0; point < 4; ++point)
            {
                auto& vertex = vertices[point];
                for (int axis = 0; axis < 3; ++axis)
                    vertex[axis] = (data.position[point][axis] - .5f) * data.scale[axis];
                for (int axis = 2; axis >= 0; --axis)
                {
                    const int a = (axis + 1) % 3, b = (axis + 2) % 3;
                    const float angle = data.rotation[axis] * .01745329252f;
                    const float first = vertex[a], second = vertex[b];
                    vertex[a] = first * std::cos(angle) - second * std::sin(angle);
                    vertex[b] = first * std::sin(angle) + second * std::cos(angle);
                }
                for (int axis = 0; axis < 3; ++axis) vertex[axis] += data.translation[axis];
                item.position[point] = {.5f + vertex[0], .5f - vertex[1]};
                item.uv[point] = {data.uv[point][0], data.uv[point][1]};
                item.depth += vertex[2] * .25f;
                valid &= std::isfinite(vertex[0]) && std::isfinite(vertex[1]) && std::isfinite(vertex[2])
                    && std::isfinite(item.uv[point].x) && std::isfinite(item.uv[point].y)
                    && std::abs(vertex[0]) < 8.f && std::abs(vertex[1]) < 8.f;
            }
            if (!valid) continue;
            std::array<float, 3> a{}, b{}, normal{};
            for (int axis = 0; axis < 3; ++axis)
            {
                a[axis] = vertices[1][axis] - vertices[0][axis];
                b[axis] = vertices[2][axis] - vertices[0][axis];
            }
            for (int axis = 0; axis < 3; ++axis)
                normal[axis] = a[(axis + 1) % 3] * b[(axis + 2) % 3] - a[(axis + 2) % 3] * b[(axis + 1) % 3];
            if (normal[2] <= .000001f) continue;
            const float length = std::sqrt(normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2]);
            const float light = data.side_lit ? .65f + .35f * std::clamp(
                (-.2f*normal[0] + normal[1] + .7f*normal[2]) / (length * std::sqrt(1.53f)), 0.f, 1.f) : 1.f;
            item.tint = IM_COL32(static_cast<int>(((data.tint >> 16) & 255) * light),
                static_cast<int>(((data.tint >> 8) & 255) * light), static_cast<int>((data.tint & 255) * light), 255);
            item.texture = data.texture;
            result.push_back(item);
        }
        std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.depth < b.depth; });
        return result;
    }

    inline void draw(ImDrawList* list, const std::vector<quad>& icon, ImVec2 minimum, ImVec2 maximum)
    {
        list->PushClipRect(minimum, maximum, true);
        for (const auto& item : icon)
        {
            const auto point = [&](int index) { return ImVec2(minimum.x + item.position[index].x * (maximum.x - minimum.x),
                minimum.y + item.position[index].y * (maximum.y - minimum.y)); };
            list->AddImageQuad(reinterpret_cast<ImTextureID>(item.texture), point(0), point(1), point(2), point(3),
                item.uv[0], item.uv[1], item.uv[2], item.uv[3], item.tint);
        }
        list->PopClipRect();
    }
}
