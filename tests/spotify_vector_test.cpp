#include "../src/ui/spotify_vector.hpp"
#include <cassert>
#include <iostream>

int main()
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.LogFilename = nullptr;
    io.DisplaySize = {600, 400}; io.DeltaTime = 1.f / 60;
    unsigned char* pixels; int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::NewFrame();
    auto* draw = ImGui::GetBackgroundDrawList();
    for (float size : {28.f, 48.f, 160.f}) {
        const int start = draw->VtxBuffer.Size;
        gui::draw_spotify_vector(draw, {20, 20}, size);
        assert(draw->VtxBuffer.Size > start);
        for (int i = start; i < draw->VtxBuffer.Size; ++i) {
            const auto& v = draw->VtxBuffer[i];
            assert(std::isfinite(v.pos.x) && std::isfinite(v.pos.y));
            assert(v.pos.x >= 18 && v.pos.x <= 22 + size && v.pos.y >= 18 && v.pos.y <= 22 + size);
            assert(v.uv.x == ImGui::GetFontTexUvWhitePixel().x && v.uv.y == ImGui::GetFontTexUvWhitePixel().y);
        }
    }
    const int before = draw->VtxBuffer.Size;
    gui::draw_spotify_vector(draw, {}, 0);
    assert(draw->VtxBuffer.Size == before);
    assert(gui::spotify_vector_data::contours.size() == 5);
    for (const auto& p : gui::spotify_vector_data::outline)
        assert(std::isfinite(p.nx) && std::isfinite(p.ny) && p.nx * p.nx + p.ny * p.ny < 4.01f);
    ImGui::Render();
    ImGui::DestroyContext();
    std::cout << "PASS supplied SVG vector buffers, cutout contours and texture-free draw\n";
}
