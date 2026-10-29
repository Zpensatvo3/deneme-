#pragma once
#include <imgui/imgui.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

struct capture_image { int width = 0, height = 0; std::vector<unsigned char> rgba; };
inline capture_image capture_helmet;

// Rasterize the real ImGui draw data, including its font atlas. No mock HTML,
// generated artwork, GPU, Minecraft or injected DLL is involved.
inline void capture_imgui(const std::string& path, float scale = 1.f, ImVec2 origin = {}, ImVec2 size = {})
{
    const auto* data = ImGui::GetDrawData();
    if (size.x <= 0 || size.y <= 0) size = data->DisplaySize;
    const int width = static_cast<int>(size.x * scale), height = static_cast<int>(size.y * scale);
    const auto transform = [&](ImVec2 p) { return ImVec2((p.x - origin.x) * scale, (p.y - origin.y) * scale); };
    unsigned char* atlas = nullptr;
    int atlas_width = 0, atlas_height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&atlas, &atlas_width, &atlas_height);
    std::vector<unsigned char> image(width * height * 3);
    for (int i = 0; i < width * height; ++i) {
        image[i * 3] = 23; image[i * 3 + 1] = 24; image[i * 3 + 2] = 29;
    }
    const auto edge = [](ImVec2 a, ImVec2 b, ImVec2 p) {
        return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
    };
    for (const auto* list : data->CmdLists)
        for (const auto& command : list->CmdBuffer) {
            if (command.UserCallback || !command.ElemCount) continue;
            const bool helmet = command.GetTexID() == reinterpret_cast<ImTextureID>(2);
            const auto* texture = helmet ? capture_helmet.rgba.data() : atlas;
            const int texture_width = helmet ? capture_helmet.width : atlas_width;
            const int texture_height = helmet ? capture_helmet.height : atlas_height;
            if (!texture || texture_width <= 0 || texture_height <= 0) continue;
            for (unsigned i = command.IdxOffset; i < command.IdxOffset + command.ElemCount; i += 3) {
                const auto& a = list->VtxBuffer[list->IdxBuffer[i] + command.VtxOffset];
                const auto& b = list->VtxBuffer[list->IdxBuffer[i + 1] + command.VtxOffset];
                const auto& c = list->VtxBuffer[list->IdxBuffer[i + 2] + command.VtxOffset];
                const auto ap = transform(a.pos), bp = transform(b.pos), cp = transform(c.pos);
                const auto clip_min = transform({command.ClipRect.x, command.ClipRect.y});
                const auto clip_max = transform({command.ClipRect.z, command.ClipRect.w});
                const float area = edge(ap, bp, cp);
                if (std::abs(area) < .00001f) continue;
                const int x0 = std::max({0, int(std::floor(std::min({ap.x, bp.x, cp.x}))), int(std::ceil(clip_min.x))});
                const int x1 = std::min({width, int(std::ceil(std::max({ap.x, bp.x, cp.x}))), int(std::floor(clip_max.x))});
                const int y0 = std::max({0, int(std::floor(std::min({ap.y, bp.y, cp.y}))), int(std::ceil(clip_min.y))});
                const int y1 = std::min({height, int(std::ceil(std::max({ap.y, bp.y, cp.y}))), int(std::floor(clip_max.y))});
                for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) {
                    const ImVec2 p(x + .5f, y + .5f);
                    const float u = edge(bp, cp, p) / area, v = edge(cp, ap, p) / area;
                    const float w = 1.f - u - v;
                    if (u < 0 || v < 0 || w < 0) continue;
                    // Same texel-center convention and GL_LINEAR filtering as
                    // the production OpenGL backend, not nearest-neighbor text.
                    const float tx = (a.uv.x * u + b.uv.x * v + c.uv.x * w) * texture_width - .5f;
                    const float ty = (a.uv.y * u + b.uv.y * v + c.uv.y * w) * texture_height - .5f;
                    const int left = static_cast<int>(std::floor(tx)), top = static_cast<int>(std::floor(ty));
                    const float fx = tx - left, fy = ty - top;
                    const auto sample = [&](int channel) {
                        const auto texel = [&](int x, int y) { return float(texture[(std::clamp(y, 0, texture_height - 1) *
                            texture_width + std::clamp(x, 0, texture_width - 1)) * 4 + channel]); };
                        return (texel(left, top) * (1.f - fx) + texel(left + 1, top) * fx) * (1.f - fy) +
                            (texel(left, top + 1) * (1.f - fx) + texel(left + 1, top + 1) * fx) * fy;
                    };
                    const auto color = [&](int shift) { return float((a.col >> shift) & 255) * u +
                        float((b.col >> shift) & 255) * v + float((c.col >> shift) & 255) * w; };
                    const float alpha = color(IM_COL32_A_SHIFT) * sample(3) / (255.f * 255.f);
                    for (int channel = 0; channel < 3; ++channel) {
                        const int shift = channel == 0 ? IM_COL32_R_SHIFT : channel == 1 ? IM_COL32_G_SHIFT : IM_COL32_B_SHIFT;
                        auto& pixel = image[(y * width + x) * 3 + channel];
                        pixel = static_cast<unsigned char>(std::clamp(pixel * (1.f - alpha) +
                            color(shift) * sample(channel) / 255.f * alpha, 0.f, 255.f));
                    }
                }
            }
        }
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << width << ' ' << height << "\n255\n";
    output.write(reinterpret_cast<const char*>(image.data()), image.size());
}
