#include "friends.hpp"
#include "../platform/jni/jni_context.hpp"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "../gui/gui.hpp"
#include "../ui/friends_page.hpp"
#include "../modules/friends/friend_manager.hpp"
namespace
{
	struct friend_state
	{
		std::mutex mutex;
		std::unordered_map<std::string, std::string> names;
		std::vector<std::string> server_players;
		std::string local_player;
		std::string status;
	};

	friend_state state;
	bool middle_was_down = false;
	std::chrono::steady_clock::time_point next_server_refresh{};

	std::string trim(std::string value)
	{
		const auto not_space = [](unsigned char character)
		{
			return !std::isspace(character);
		};
		value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
		value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
		return value;
	}

	std::string normalize(std::string_view name)
	{
		std::string result(name);
		std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});
		return result;
	}
std::string player_name(const maps::Entity& entity)
	{
		if (!entity || !entity.is_instance_of<maps::PlayerEntity>() || platform::jni_context::clear_exception())
			return {};

		maps::PlayerEntity player(entity.object_instance, false);
		maps::GameProfile profile = player.getGameProfile.call();
		maps::String name = profile ? profile.getName.call() : maps::String{};
		if (!profile || !name || platform::jni_context::clear_exception())
			return {};
		return name.to_string();
	}

}

bool friends::is_friend(std::string_view name)
{
	const std::string key = normalize(trim(std::string(name)));
	if (key.empty())
		return false;
	std::lock_guard lock(state.mutex);
	return state.names.contains(key);
}

bool friends::is_friend(const maps::Entity& entity)
{
	const std::string name = player_name(entity);
	return !name.empty() && is_friend(name);
}

void friends::add(std::string_view name)
{
	const std::string display = trim(std::string(name));
	if (display.empty())
		return;

	std::lock_guard lock(state.mutex);
	state.names[normalize(display)] = display;
	state.status = display + " added as friend";
}

void friends::remove(std::string_view name)
{
	const std::string display = trim(std::string(name));
	if (display.empty())
		return;

	std::lock_guard lock(state.mutex);
	if (state.names.erase(normalize(display)) > 0)
		state.status = display + " removed from friends";
}

void friends::toggle(std::string_view name)
{
	const std::string display = trim(std::string(name));
	if (display.empty())
		return;

	std::lock_guard lock(state.mutex);
	const std::string key = normalize(display);
	if (state.names.erase(key) > 0)
		state.status = display + " removed from friends";
	else
	{
		state.names[key] = display;
		state.status = display + " added as friend";
	}
}

std::vector<friends::server_player> friends::server_players_snapshot()
{
	std::lock_guard lock(state.mutex);
	std::vector<server_player> result;
	result.reserve(state.server_players.size());
	for (const std::string& name : state.server_players)
	{
		result.push_back({
			name,
			normalize(name) == normalize(state.local_player),
			state.names.contains(normalize(name))
		});
	}
	return result;
}

std::vector<std::string> friends::friend_names_snapshot()
{
	std::lock_guard lock(state.mutex);
	std::vector<std::string> result;
	result.reserve(state.names.size());
	for (const auto& [key, display] : state.names)
		result.push_back(display);
	std::sort(result.begin(), result.end(), [](const std::string& left, const std::string& right)
	{
		return normalize(left) < normalize(right);
	});
	return result;
}

std::string friends::local_player_name()
{
	std::lock_guard lock(state.mutex);
	return state.local_player;
}

std::string friends::status_message()
{
	std::lock_guard lock(state.mutex);
	return state.status;
}

void friends::update_server_players(::cache& cache)
{
	const auto now = std::chrono::steady_clock::now();
	if (now < next_server_refresh)
		return;
	next_server_refresh = now + std::chrono::milliseconds(500);

	if (!cache.is_valid() || !cache.instance || !cache.player || !jni::get_env())
	{
		std::lock_guard lock(state.mutex);
		state.server_players.clear();
		state.local_player.clear();
		return;
	}

	std::vector<std::string> players;
	std::unordered_set<std::string> seen;
	auto append_name = [&players, &seen](const std::string& display)
	{
		if (!display.empty() && seen.insert(normalize(display)).second)
			players.push_back(display);
	};

	// The network tab list contains everyone advertised by the server, including
	// players outside the world's current entity tracking distance.
	try
	{
		maps::ClientPlayNetworkHandler handler = cache.instance.getNetworkHandler();
		maps::Collection collection = handler ? handler.getPlayerList.call() : maps::Collection{};
		jni::array<maps::Object> array = collection ? collection.toArray.call() : jni::array<maps::Object>{ nullptr };
		if (handler && collection && array && !platform::jni_context::clear_exception())
		{
			for (const maps::Object& object : array.to_vector())
			{
				maps::PlayerListEntry entry(object.object_instance, false);
				maps::GameProfile profile = entry ? entry.getProfile.call() : maps::GameProfile{};
				maps::String name = profile ? profile.getName.call() : maps::String{};
				if (!entry || !profile || !name || platform::jni_context::clear_exception())
					continue;
				append_name(name.to_string());
			}
		}
		platform::jni_context::clear_exception();
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}

	// Merge the world's loaded player entities as a fallback for clients or
	// servers that temporarily expose an empty tab list during login/respawn.
	try
	{
		maps::List loaded_players = cache.players;
		jni::array<maps::Object> array = loaded_players
			? loaded_players.toArray.call()
			: jni::array<maps::Object>{ nullptr };
		if (loaded_players && array && !platform::jni_context::clear_exception())
		{
			for (const maps::Object& object : array.to_vector())
			{
				maps::Entity entity(object.object_instance, false);
				append_name(player_name(entity));
			}
		}
		platform::jni_context::clear_exception();
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}

	std::string local;
	try
	{
		maps::GameProfile local_profile = cache.player.getGameProfile.call();
		maps::String local_name = local_profile ? local_profile.getName.call() : maps::String{};
		local = local_name && !platform::jni_context::clear_exception()
			? local_name.to_string()
			: std::string{};
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}
	append_name(local);

	std::sort(players.begin(), players.end(), [](const std::string& left, const std::string& right)
	{
		return normalize(left) < normalize(right);
	});

	std::lock_guard lock(state.mutex);
	state.server_players = std::move(players);
	state.local_player = local;
}

void friends::handle_middle_click(::cache& cache, bool active)
{
	const bool middle_down = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
	const bool pressed = middle_down && !middle_was_down;
	middle_was_down = middle_down;

	if (!active || !pressed || !cache.is_valid() || !cache.instance || !cache.player)
		return;

	try
	{
		if (cache.instance.currentScreen.get() || platform::jni_context::clear_exception())
			return;

		maps::HitResult hit = cache.instance.crosshairTarget.get();
		maps::HitResult$Type type = hit ? hit.getType() : maps::HitResult$Type{};
		maps::HitResult$Type entity_type = maps::HitResult$Type{}.ENTITY.get();
		if (!hit || !type || !entity_type || platform::jni_context::clear_exception() ||
			!type.is_same_object(entity_type))
			return;

		maps::Entity target = maps::EntityHitResult(hit.object_instance, false).entity.get();
		if (!target || target.is_same_object(cache.player) ||
			!target.is_instance_of<maps::PlayerEntity>() || platform::jni_context::clear_exception())
			return;

		const std::string name = player_name(target);
		if (!name.empty())
			toggle(name);
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}
}

void friends::render_manager(bool embedded)
{
	gui::friends_page::render(embedded);
}

void modules::friend_manager::run(::cache& cache)
{
	try
	{
		jni::frame frame{ 160 };
		friends::update_server_players(cache);
		const HWND minecraft_window = gui::get_window();
		const bool active = !gui::draw && minecraft_window &&
			GetForegroundWindow() == minecraft_window;
		friends::handle_middle_click(cache, active);
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}
}

void modules::friend_manager::on_disable(::cache& cache)
{
	friends::handle_middle_click(cache, false);
}

void modules::friend_manager::render_options()
{
	friends::render_manager();
}
