#include "block_esp.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../../platform/adapter/item_model.hpp"
#include "block_icon_bindings.hpp"
#include "../../services/game/loaded_chunks.hpp"
#include "../ui_redirect.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_set>
#include <vector>
#include <array>
#include <unordered_map>
#include <optional>

#include <imgui/imgui.h>

#include "../../gui/gui.hpp"


namespace
{
	struct scanned_block
	{
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
		std::string key;
	};

	std::uint64_t block_key(int x, int y, int z)
	{
		return (static_cast<std::uint64_t>(x) & 0x3FFFFFFULL) << 38 |
			(static_cast<std::uint64_t>(z) & 0x3FFFFFFULL) << 12 |
			(static_cast<std::uint64_t>(y) & 0xFFFULL);
	}

    bool scan_section_blocks(const maps::ChunkSection& section, const maps::DefaultedRegistry& registry,
        const std::unordered_set<std::string>& selected, int bx, int by, int bz,
        std::vector<scanned_block>& matches)
    {
        auto container = section.getBlockStateContainer.call();
        auto copy = container ? container.copy.call() : maps::PalettedContainer{};
        auto data = copy ? copy.data.get() : maps::PaletteData{};
        auto palette = data ? data.palette.call() : maps::Palette{};
        auto storage = data ? data.storage.call() : maps::PaletteStorage{};
        if (platform::jni_context::clear_exception() || !palette || !storage) return false;
        jintArray ids = jni::get_env()->NewIntArray(4096);
        if (platform::jni_context::clear_exception() || !ids) return false;
        storage.writePaletteIndices.call(jni::array<jint>(ids, false));
        if (platform::jni_context::clear_exception()) return false;
        std::array<jint, 4096> cells{};
        jni::get_env()->GetIntArrayRegion(ids, 0, 4096, cells.data());
        if (platform::jni_context::clear_exception()) return false;
        std::unordered_map<int, std::string> names;
        for (int index = 0; index < 4096; ++index)
        {
            const int id = cells[index];
            auto it = names.find(id);
            if (it == names.end())
            {
                jni::frame cell_frame{16};
                auto object = palette.get.call(id);
                maps::BlockState state(object.object_instance, false);
                auto block = state ? state.getBlock.call() : maps::Block{};
                auto identifier = block ? registry.getId.call(maps::Object(block.object_instance, false)) : maps::Identifier{};
                auto text = identifier ? identifier.toString.call() : maps::String{};
                if (platform::jni_context::clear_exception() || !text) return false;
                std::string name = text.to_string();
                if (platform::jni_context::clear_exception()) return false;
                if (!selected.contains(name)) name.clear();
                it = names.emplace(id, std::move(name)).first;
            }
            if (!it->second.empty())
                matches.push_back({double(bx + (index & 15)), double(by + (index >> 8)),
                    double(bz + ((index >> 4) & 15)), it->second});
        }
        return true;
    }

    struct view_point { double x, y, z; };
    view_point operator+(view_point a, view_point b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
    view_point operator-(view_point a, view_point b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
    view_point operator*(view_point a, double scale) { return {a.x*scale, a.y*scale, a.z*scale}; }
    double dot(view_point a, view_point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
    bool finite(view_point p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }

    struct view_plane
    {
        view_point normal;
        double offset;
        double box_radius = 0.;
        double distance(view_point p) const { return dot(normal, p)+offset; }
    };

    struct block_view
    {
        view_point camera, axis_x, axis_y, axis_z;
        ImVec2 origin, size;
        double focal = 0.;
        std::array<view_point, 8> corner_offsets;
        std::array<view_plane, 5> planes;

        view_point to_view(view_point p) const
        {
            p = p-camera;
            return axis_x*p.x+axis_y*p.y+axis_z*p.z;
        }

        ImVec2 project(view_point p) const
        {
            return {static_cast<float>(origin.x+size.x*.5+p.x*focal/p.z),
                static_cast<float>(origin.y+size.y*.5-p.y*focal/p.z)};
        }
    };

    bool camera_snapshot(const services::game::client_context& context, block_view& view)
    {
        auto& adapter = platform::adapter::get();
        const float delta = adapter.get_tick_delta(context.instance);
        if (platform::jni_context::clear_exception() || !std::isfinite(delta)) return false;
        const auto camera = adapter.get_camera_data(context.instance, context.player, std::clamp(delta, 0.f, 1.f));
        if (platform::jni_context::clear_exception() || !camera.valid ||
            !std::isfinite(camera.yaw) || !std::isfinite(camera.pitch) ||
            !std::isfinite(camera.fov) || camera.fov <= 1.f || camera.fov >= 179.f) return false;
        view.camera = {camera.pos.x, camera.pos.y, camera.pos.z};
        const auto* viewport = ImGui::GetMainViewport();
        view.origin = viewport->Pos;
        view.size = viewport->Size;
        if (!finite(view.camera) || !std::isfinite(view.origin.x) || !std::isfinite(view.origin.y) ||
            !std::isfinite(view.size.x) || !std::isfinite(view.size.y) ||
            view.size.x <= 0.f || view.size.y <= 0.f) return false;

        // One camera transform per frame; each block is an axis-aligned unit cube.
        constexpr double pi = 3.14159265358979323846;
        const double yaw = camera.yaw*pi/180., pitch = camera.pitch*pi/180.;
        const double sy = std::sin(yaw), cy = std::cos(yaw), sp = std::sin(pitch), cp = std::cos(pitch);
        view.axis_x = {-cy, -sy*sp, -sy*cp};
        view.axis_y = {0., cp, -sp};
        view.axis_z = {-sy, cy*sp, cy*cp};
        const double tangent_y = std::tan(camera.fov*pi/360.);
        const double tangent_x = tangent_y*view.size.x/view.size.y;
        view.focal = view.size.y/(2.*tangent_y);
        view.planes = {{{{0., 0., 1.}, -.08}, {{1., 0., tangent_x}, 0.},
            {{-1., 0., tangent_x}, 0.}, {{0., 1., tangent_y}, 0.}, {{0., -1., tangent_y}, 0.}}};
        for (auto& plane : view.planes)
            plane.box_radius = .5*(std::abs(dot(plane.normal, view.axis_x))+
                std::abs(dot(plane.normal, view.axis_y))+std::abs(dot(plane.normal, view.axis_z)));
        for (int i = 0; i < 8; ++i)
            view.corner_offsets[i] = view.axis_x*((i & 4) ? .5 : -.5)+
                view.axis_y*((i & 2) ? .5 : -.5)+view.axis_z*((i & 1) ? .5 : -.5);
        return true;
    }

    struct visible_block
    {
        view_point center;
        unsigned clip_mask = 0;
    };

    // A quad gains at most one vertex at each of the five clipping planes.
    // Fixed storage avoids per-face allocations when many blocks are selected.
    struct clipped_face
    {
        std::array<view_point, 10> points{};
        int count = 0;
    };

    clipped_face clip_face(const clipped_face& input, const view_plane& plane)
    {
        clipped_face output;
        if (!input.count) return output;
        view_point a = input.points[input.count-1];
        double da = plane.distance(a);
        for (int i = 0; i < input.count; ++i)
        {
            const view_point b = input.points[i];
            const double db = plane.distance(b);
            if ((da >= 0.) != (db >= 0.)) output.points[output.count++] = a+(b-a)*(da/(da-db));
            if (db >= 0.) output.points[output.count++] = b;
            a = b;
            da = db;
        }
        return output;
    }

    bool clip_edge(view_point& a, view_point& b, const block_view& view, unsigned clip_mask)
    {
        for (std::size_t i = 0; i < view.planes.size(); ++i)
        {
            if (!(clip_mask & (1u << i))) continue;
            const double da = view.planes[i].distance(a), db = view.planes[i].distance(b);
            if (da < 0. && db < 0.) return false;
            if ((da < 0.) != (db < 0.))
            {
                const view_point intersection = a+(b-a)*(da/(da-db));
                if (da < 0.) a = intersection; else b = intersection;
            }
        }
        return true;
    }

    void draw_block(ImDrawList* draw, const block_view& view, const visible_block& block,
        ImVec4 color, float fill_opacity, float line_thickness)
    {
        std::array<view_point, 8> corners;
        for (std::size_t i = 0; i < corners.size(); ++i) corners[i] = block.center+view.corner_offsets[i];
        static constexpr int faces[6][4] = {
            {0, 1, 3, 2}, {4, 6, 7, 5}, // -X, +X
            {0, 4, 5, 1}, {2, 3, 7, 6}, // -Y, +Y
            {0, 2, 6, 4}, {1, 5, 7, 3}  // -Z, +Z
        };
        if (fill_opacity > 0.f)
        {
            struct face_order { int index; double depth; };
            std::array<face_order, 6> order;
            for (int i = 0; i < 6; ++i)
                order[i] = {i, corners[faces[i][0]].z+corners[faces[i][1]].z+
                    corners[faces[i][2]].z+corners[faces[i][3]].z};
            std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.depth > b.depth; });
            static constexpr float shade[6] = {.82f, .82f, .65f, 1.f, .92f, .92f};
            for (const auto& face : order)
            {
                clipped_face polygon;
                polygon.count = 4;
                for (int i = 0; i < 4; ++i) polygon.points[i] = corners[faces[face.index][i]];
                for (std::size_t i = 0; i < view.planes.size() && polygon.count >= 3; ++i)
                    if (block.clip_mask & (1u << i)) polygon = clip_face(polygon, view.planes[i]);
                if (polygon.count < 3) continue;
                std::array<ImVec2, 10> points;
                for (int i = 0; i < polygon.count; ++i) points[i] = view.project(polygon.points[i]);
                double area = 0.;
                for (int i = 0; i < polygon.count; ++i)
                {
                    const auto a = points[i], b = points[(i+1)%polygon.count];
                    area += static_cast<double>(a.x)*b.y-static_cast<double>(b.x)*a.y;
                }
                if (std::abs(area) < 1.e-6) continue;
                // ImGui's antialiased convex fill requires clockwise screen vertices.
                if (area < 0.) std::reverse(points.begin(), points.begin()+polygon.count);
                const float light = shade[face.index];
                const ImVec4 fill(color.x*light, color.y*light, color.z*light, color.w*fill_opacity);
                draw->AddConvexPolyFilled(points.data(), polygon.count, ImGui::ColorConvertFloat4ToU32(fill));
            }
        }
        static constexpr int edges[12][2] = {
            {0, 1}, {0, 2}, {0, 4}, {1, 3}, {1, 5}, {2, 3},
            {2, 6}, {3, 7}, {4, 5}, {4, 6}, {5, 7}, {6, 7}
        };
        const ImU32 line = ImGui::ColorConvertFloat4ToU32(color);
        for (const auto& edge : edges)
        {
            auto a = corners[edge[0]], b = corners[edge[1]];
            if (clip_edge(a, b, view, block.clip_mask))
                draw->AddLine(view.project(a), view.project(b), line, line_thickness);
        }
    }

	std::string display_block_name(const std::string& identifier)
	{
		std::string name = identifier;
		const std::size_t separator = name.find(':');
		if (separator != std::string::npos) name.erase(0, separator + 1);
		bool capitalize = true;
		for (char& character : name)
		{
			if (character == '_') { character = ' '; capitalize = true; continue; }
			if (capitalize && character >= 'a' && character <= 'z') character = static_cast<char>(character - 'a' + 'A');
			capitalize = false;
		}
		return name;
	}
}

void modules::block_esp::run(::cache& cache)
{
    auto settings_snapshot = settings_.read();
	if (!cache.is_valid() || !cache.player || !cache.world || !jni::get_env()) { clear_snapshot(); return; }
    const auto now = std::chrono::steady_clock::now();
    if (now < next_scan) return;
    next_scan = now + std::chrono::milliseconds(50);
	try
	{
		jni::frame frame{ 4096 };
        std::uint64_t generation;
        {
            std::scoped_lock lock(marker_mutex);
            if (session_.observe({cache.world, cache.player})) { markers.clear(); scan_index = 0; scan_section = 0; }
            generation = session_.ticket();
        }
		maps::DefaultedRegistry block_registry = maps::Registries{}.BLOCK.get();
		if (!block_registry || platform::jni_context::clear_exception()) return;
		if (!registry_loaded)
		{
			std::vector<std::string> catalog;
			maps::Iterator iterator = block_registry.iterator.call();
			while (iterator && iterator.hasNext.call() == JNI_TRUE && !platform::jni_context::clear_exception())
			{
				maps::Object block = iterator.next.call();
				maps::Object block_object(block.object_instance, false);
				maps::Identifier identifier = block ? block_registry.getId.call(block_object) : maps::Identifier{};
				maps::String text = identifier && !platform::jni_context::clear_exception() ? identifier.toString.call() : maps::String{};
				if (text && !platform::jni_context::clear_exception()) catalog.push_back(text.to_string());
				platform::jni_context::clear_exception();
			}
			std::sort(catalog.begin(), catalog.end());
			catalog.erase(std::unique(catalog.begin(), catalog.end()), catalog.end());
			{
				std::scoped_lock lock(marker_mutex);
				available_blocks = std::move(catalog);
				registry_loaded = true;
			}
		}

		std::vector<std::string> selected_snapshot;
		{
			std::scoped_lock lock(marker_mutex);
			selected_snapshot = settings_snapshot.selected_blocks;
		}
        const std::unordered_set<std::string> selected(selected_snapshot.begin(), selected_snapshot.end());
        services::game::loaded_chunks chunks;
        if (!services::game::capture_loaded_chunks(cache.world, chunks)) { clear_snapshot(); return; }
        std::size_t cursor, section_cursor;
        {
            std::scoped_lock lock(marker_mutex);
            cursor = scan_index; section_cursor = scan_section;
        }
        std::vector<scanned_block> scanned;
        std::unordered_set<std::uint64_t> refreshed;
        const std::size_t count = chunks.slots.size();
        // Time-slice work, not distance: every loaded chunk and all its vertical
        // sections are eventually visited, even while the player is moving.
        const auto scan_started = std::chrono::steady_clock::now();
        for (int visited = 0, nonempty = 0; count && visited < 128 && nonempty < 16; ++visited)
        {
            if (visited && std::chrono::steady_clock::now() - scan_started >= std::chrono::milliseconds(4)) break;
            jni::frame section_frame{64};
            cursor %= count;
            const auto& slot = chunks.slots[cursor];
            auto chunk = services::game::read_loaded_chunk(chunks, slot);
            if (!chunk) { cursor = (cursor + 1) % count; section_cursor = 0; continue; }
            auto sections = chunk.getSectionArray.call();
            const int bottom = chunk.getBottomY.call();
            if (platform::jni_context::clear_exception() || !sections) { clear_snapshot(); return; }
            const int length = sections.get_length();
            if (platform::jni_context::clear_exception()) { clear_snapshot(); return; }
            if (section_cursor >= static_cast<std::size_t>(length))
            {
                cursor = (cursor + 1) % count; section_cursor = 0; continue;
            }
            const int base_y = bottom + static_cast<int>(section_cursor) * 16;
            maps::ChunkSection section(jni::get_env()->GetObjectArrayElement(
                static_cast<jobjectArray>(sections.object_instance), static_cast<jsize>(section_cursor)), false);
            ++section_cursor;
            const auto section_key = block_key(slot.x * 16, base_y, slot.z * 16);
            if (!section) { refreshed.insert(section_key); continue; }
            const bool empty = section.isEmpty.call() == JNI_TRUE;
            if (platform::jni_context::clear_exception()) { clear_snapshot(); return; }
            if (empty || selected.empty()) { refreshed.insert(section_key); continue; }
            ++nonempty;
            std::vector<scanned_block> section_matches;
            if (!scan_section_blocks(section, block_registry, selected, slot.x * 16, base_y,
                slot.z * 16, section_matches)) { clear_snapshot(); return; }
            refreshed.insert(section_key);
            scanned.insert(scanned.end(), section_matches.begin(), section_matches.end());
        }

        // Reject results for an obsolete selection as well as an obsolete world.
        if (settings_.read().selected_blocks != selected_snapshot) return;
        std::scoped_lock lock(marker_mutex);
        if (!session_.accepts(generation)) return;
        markers.erase(std::remove_if(markers.begin(), markers.end(), [&](const marker& old)
        {
            const int x = static_cast<int>(std::floor(old.x / 16.));
            const int z = static_cast<int>(std::floor(old.z / 16.));
            const int y = static_cast<int>(std::floor(old.y / 16.)) * 16;
            return !chunks.keys.contains(services::game::chunk_key(x, z)) ||
                refreshed.contains(block_key(x * 16, y, z * 16)) || !selected.contains(old.key);
        }), markers.end());
        for (const scanned_block& cell : scanned)
            if (markers.size() < 10000)
                markers.push_back({cell.x, cell.y, cell.z, cell.key});
        scan_index = count ? cursor % count : 0;
        scan_section = count ? section_cursor : 0;
	}
	catch (...) { platform::jni_context::clear_exception(); clear_snapshot(); }
}

void modules::block_esp::render_overlay()
{
    auto settings_snapshot = settings_.read();
    if (!platform::jni_context::bind_current_thread()) { clear_snapshot(); return; }
    jni::frame render_frame{96};
    services::game::client_context context;
    if (!services::game::capture_client_context(context, true, false)) { clear_snapshot(); return; }
    block_view view;
    {
        jni::frame frame{48};
        if (!camera_snapshot(context, view)) { clear_snapshot(); return; }
    }

	std::vector<marker> draw_markers;
	{
		std::scoped_lock lock(marker_mutex);
        if (session_.observe({context.world, context.player})) {
            markers.clear(); scan_index = 0; scan_section = 0; return;
        }
		draw_markers = markers;
	}
	if (draw_markers.empty()) return;

    if (!std::isfinite(settings_snapshot.fill_opacity) || !std::isfinite(settings_snapshot.line_thickness)) return;
    const std::unordered_set<std::string> selected(settings_snapshot.selected_blocks.begin(), settings_snapshot.selected_blocks.end());
    std::vector<visible_block> visible;
    visible.reserve(draw_markers.size());
    for (const marker& entry : draw_markers)
    {
        if (!selected.contains(entry.key)) continue;
        const auto center = view.to_view({entry.x+.5, entry.y+.5, entry.z+.5});
        if (!finite(center)) continue;
        unsigned clip_mask = 0;
        bool outside = false;
        for (std::size_t i = 0; i < view.planes.size(); ++i)
        {
            const auto& plane = view.planes[i];
            const double distance = plane.distance(center);
            if (distance < -plane.box_radius) { outside = true; break; }
            if (distance < plane.box_radius) clip_mask |= 1u << i;
        }
        if (!outside) visible.push_back({center, clip_mask});
    }
    // Painter's order lets nearer translucent boxes cover farther boxes.
    std::sort(visible.begin(), visible.end(), [](const auto& a, const auto& b) { return a.center.z > b.center.z; });
    ImVec4 color = gui::get_dynamic_color().Value;
    color.w = .92f;
    const float fill_opacity = std::clamp(settings_snapshot.fill_opacity, 0.f, .35f);
    const float line_thickness = std::clamp(settings_snapshot.line_thickness, .5f, 3.f);
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    draw->PushClipRect(view.origin, ImVec2(view.origin.x+view.size.x, view.origin.y+view.size.y), true);
    for (const auto& block : visible) draw_block(draw, view, block, color, fill_opacity, line_thickness);
    draw->PopClipRect();
}

void modules::block_esp::clear_snapshot()
{
    std::scoped_lock lock(marker_mutex);
    if (jni::get_env()) session_.reset(); else session_.invalidate();
    markers.clear(); scan_index = 0; scan_section = 0;
}

void modules::block_esp::on_disable(::cache&)
{
    clear_snapshot();
}

void modules::block_esp::render_options()
{
    gui::settings::edit(settings_, module_settings::block_esp::schema(), [&](auto& candidate) {
	bool needs_catalog = false;
	{
		std::scoped_lock lock(marker_mutex);
		needs_catalog = !registry_loaded;
	}
	if (needs_catalog && platform::jni_context::bind_current_thread())
	{
		try
		{
			jni::frame frame{ 2048 };
			maps::DefaultedRegistry block_registry = maps::Registries{}.BLOCK.get();
			std::vector<std::string> catalog;
			maps::Iterator iterator = block_registry ? block_registry.iterator.call() : maps::Iterator{};
			while (iterator && iterator.hasNext.call() == JNI_TRUE && !platform::jni_context::clear_exception())
			{
				maps::Object block = iterator.next.call();
				maps::Identifier identifier = block ? block_registry.getId.call(block) : maps::Identifier{};
				maps::String text = identifier && !platform::jni_context::clear_exception() ? identifier.toString.call() : maps::String{};
				if (text && !platform::jni_context::clear_exception()) catalog.push_back(text.to_string());
				platform::jni_context::clear_exception();
			}
			if (!catalog.empty())
			{
				std::sort(catalog.begin(), catalog.end());
				catalog.erase(std::unique(catalog.begin(), catalog.end()), catalog.end());
				std::scoped_lock lock(marker_mutex);
				available_blocks = std::move(catalog);
				registry_loaded = true;
			}
		}
		catch (...) { platform::jni_context::clear_exception(); }
	}

    gui::settings::render(candidate, module_settings::block_esp::schema());

	std::vector<std::string> catalog;
	{
		std::scoped_lock lock(marker_mutex);
		catalog = available_blocks;
	}
    if (picker_catalog_.size() != catalog.size())
    {
        picker_catalog_.clear();
        for (const auto& key : catalog)
        {
            const auto label = display_block_name(key);
            picker_catalog_.push_back({key,label,gui::block_picker::lowercase(key+" "+label)});
        }
    }
    const int frame_number = ImGui::GetFrameCount();
    if (picker_frame_ + 1 != frame_number) block_icons_.clear();
    picker_frame_ = frame_number;
    // LRU pruning happens before drawing, never while a cached icon is referenced.
    while (block_icons_.size() > 192)
    {
        auto oldest = std::min_element(block_icons_.begin(),block_icons_.end(),
            [](const auto& a,const auto& b) { return a.second.last_used < b.second.last_used; });
        block_icons_.erase(oldest);
    }
    std::optional<services::game::client_context> context;
    bool context_checked = false, game_ready = false;
    int reads_left = 2; // Only visible tiles; never rebuild the entire registry in a frame.
    const double now = ImGui::GetTime();
    const auto icon = [&](const std::string& key, ImDrawList* draw, ImVec2 minimum, ImVec2 maximum)
    {
        auto found = block_icons_.find(key);
        if (reads_left > 0 && (found == block_icons_.end() || found->second.refresh_at <= now))
        {
            --reads_left;
            std::vector<gui::item_icons::quad> quads;
            try
            {
                // Warm, unchanged tiles do not read the game at all.
                if (!context_checked)
                {
                    context_checked = true;
                    if (platform::jni_context::bind_current_thread())
                    {
                        context.emplace();
                        game_ready = services::game::capture_client_context(*context,true,false);
                    }
                }
                if (game_ready)
                {
                    jni::frame icon_frame{128};
                    const auto identifier = block_icon_bindings::Identifier{}.parse.call(maps::String::create(key.c_str()));
                    const auto registry = maps::Registries{}.BLOCK.get();
                    const auto object = identifier && registry && !platform::jni_context::clear_exception()
                        ? block_icon_bindings::Registry(registry.object_instance,false).get.call(identifier) : maps::Object{};
                    const auto item = object && !platform::jni_context::clear_exception()
                        ? maps::Block(object.object_instance,false).asItem.call() : maps::Item{};
                    const auto stack = item && !platform::jni_context::clear_exception()
                        ? block_icon_bindings::Item(item.object_instance,false).default_stack.call() : maps::ItemStack{};
                    if (stack && !platform::jni_context::clear_exception() && stack.isEmpty.call() != JNI_TRUE
                        && !platform::jni_context::clear_exception())
                    {
                        const auto translation = item.getTranslationKey.call();
                        if (translation && !platform::jni_context::clear_exception())
                            quads = gui::item_icons::project(platform::adapter::read_item_model(context->instance,
                                maps::World(context->world.object_instance,false),
                                maps::LivingEntity(context->player.object_instance,false),stack,translation.to_string()));
                    }
                }
            }
            catch (...) { quads.clear(); }
            if (platform::jni_context::clear_exception()) quads.clear();
            auto& cached = block_icons_[key];
            cached.quads = std::move(quads);
            cached.refresh_at = now + 2.; // Refresh borrowed atlas/model data after resource changes.
            found = block_icons_.find(key);
        }
        if (found != block_icons_.end()) found->second.last_used = frame_number;
        if (found != block_icons_.end() && !found->second.quads.empty())
            gui::item_icons::draw(draw,found->second.quads,minimum,maximum);
        else
        {
            // Blocks without an inventory item (air, fluids, etc.) remain selectable.
            const ImVec2 center((minimum.x+maximum.x)*.5f,(minimum.y+maximum.y)*.5f);
            const ImU32 color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
            draw->AddQuad({center.x,center.y-9.f},{center.x+9.f,center.y-4.f},
                {center.x,center.y+1.f},{center.x-9.f,center.y-4.f},color,1.4f);
            draw->AddLine({center.x-9.f,center.y-4.f},{center.x-9.f,center.y+5.f},color,1.4f);
            draw->AddLine({center.x+9.f,center.y-4.f},{center.x+9.f,center.y+5.f},color,1.4f);
            draw->AddLine({center.x,center.y+1.f},{center.x,center.y+10.f},color,1.4f);
            draw->AddLine({center.x-9.f,center.y+5.f},{center.x,center.y+10.f},color,1.4f);
            draw->AddLine({center.x+9.f,center.y+5.f},{center.x,center.y+10.f},color,1.4f);
        }
    };
    if (gui::block_picker::render(picker_catalog_,candidate.selected_blocks,picker_state_,
        gui::ui_font,ImGui::GetStyleColorVec4(ImGuiCol_CheckMark),icon))
    {
        std::scoped_lock lock(marker_mutex);
        markers.clear(); scan_index = 0; scan_section = 0;
    }

    });
}
