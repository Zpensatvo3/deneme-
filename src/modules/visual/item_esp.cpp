#include "item_esp.hpp"
#include "item_esp_geometry.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../../services/game/client_context.hpp"
#include "../../gui/gui.hpp"
#include "../ui_redirect.hpp"

void modules::item_esp::render_overlay()
{
    if (!enabled || !gui::runtime_ready.load(std::memory_order_acquire) || gui::should_unload || !platform::jni_context::bind_current_thread()) return;
    struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } cleanup;
    try
    {
        jni::frame frame{96};
        services::game::client_context context;
        if (!services::game::capture_client_context(context, true, false)) return;
        const auto config = settings_.read();
        if (!config.show_boxes && !config.show_names && !config.show_count && !config.show_distance) return;
        auto& adapter = platform::adapter::get();
        const float delta = std::clamp(adapter.get_tick_delta(context.instance), 0.f, 1.f);
        const auto source = adapter.get_camera_data(context.instance, context.player, delta);
        if (platform::jni_context::clear_exception() || !source.valid) return;
        const auto* viewport = ImGui::GetMainViewport();
        const item_esp_geometry::camera camera{{source.pos.x, source.pos.y, source.pos.z},
            source.yaw, source.pitch, source.fov, viewport->Size.x, viewport->Size.y, viewport->Pos.x, viewport->Pos.y};
        auto entities = adapter.get_entities(context.world);
        auto iterator = entities ? entities.iterator.call() : maps::Iterator{};
        if (platform::jni_context::clear_exception() || !iterator) return;
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        const ImVec4 color = config.use_theme_color ? gui::get_dynamic_color().Value :
            ImVec4(config.color[0], config.color[1], config.color[2], config.color[3]);
        const ImU32 line = ImGui::ColorConvertFloat4ToU32(color);
        const ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, color.w * config.fill_opacity));
        while (iterator.hasNext.call() == JNI_TRUE)
        {
            if (platform::jni_context::clear_exception()) return;
            jni::frame entity_frame{64};
            auto object = iterator.next.call();
            if (platform::jni_context::clear_exception()) return;
            if (!object || !object.is_instance_of<maps::ItemEntity>()) continue;
            maps::ItemEntity entity(object.object_instance, false);
            if (entity.isAlive.call() != JNI_TRUE) { platform::jni_context::clear_exception(); continue; }
            auto stack = entity.getStack.call();
            if (platform::jni_context::clear_exception() || !stack) continue;
            const bool empty = stack.isEmpty.call() == JNI_TRUE;
            const int count = stack.getCount.call();
            if (platform::jni_context::clear_exception() || empty || count <= 0) continue;
            auto bounds = entity.get_bounding_box();
            auto lerped = entity.getLerpedPos.call(delta);
            const auto current = entity.get_position();
            if (platform::jni_context::clear_exception() || !bounds || !lerped) continue;
            const auto position = lerped.to_vector3d();
            const double dx = position.x - current.x, dy = position.y - current.y, dz = position.z - current.z;
            const item_esp_geometry::point low{bounds.minX.get() + dx, bounds.minY.get() + dy, bounds.minZ.get() + dz};
            const item_esp_geometry::point high{bounds.maxX.get() + dx, bounds.maxY.get() + dy, bounds.maxZ.get() + dz};
            if (platform::jni_context::clear_exception()) continue;
            item_esp_geometry::rectangle box;
            if (!item_esp_geometry::project_box(camera, low, high, box)) continue;
            std::string name;
            if (config.show_names)
            {
                auto text = stack.getName.call();
                auto value = text ? text.getString.call() : maps::String{};
                if (platform::jni_context::clear_exception()) continue;
                name = value ? value.to_string() : "Item";
                if (platform::jni_context::clear_exception()) continue;
            }
            if (config.show_boxes)
            {
                draw->AddRectFilled(ImVec2(float(box.left), float(box.top)), ImVec2(float(box.right), float(box.bottom)), fill, 1.f);
                draw->AddRect(ImVec2(float(box.left), float(box.top)), ImVec2(float(box.right), float(box.bottom)), line, 1.f, 0, config.line_thickness);
            }
            const double distance = std::hypot(position.x - camera.position.x,
                position.y - camera.position.y, position.z - camera.position.z);
            const auto label = item_esp_geometry::label(name, count, distance, config.show_names, config.show_count, config.show_distance);
            if (label.empty()) continue;
            const ImVec2 size = ImGui::CalcTextSize(label.c_str());
            const float x = float((box.left + box.right - size.x) * .5);
            const float y = float(box.top - size.y - 4);
            draw->AddRectFilled(ImVec2(x - 4, y - 2), ImVec2(x + size.x + 4, y + size.y + 2), IM_COL32(7, 9, 13, 200), 3.f);
            draw->AddText(ImVec2(x, y), line, label.c_str());
        }
    }
    catch (...) { platform::jni_context::clear_exception(); }
}

void modules::item_esp::render_options()
{
    gui::settings::render(settings_, module_settings::item_esp::schema());
}
