#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "../cache/cache.hpp"

namespace friends
{
	struct server_player
	{
		std::string name;
		bool local = false;
		bool friend_added = false;
	};

	bool is_friend(std::string_view name);
	bool is_friend(const maps::Entity& entity);
	void add(std::string_view name);
	void remove(std::string_view name);
	void toggle(std::string_view name);

	std::vector<server_player> server_players_snapshot();
	std::vector<std::string> friend_names_snapshot();
	std::string local_player_name();
	std::string status_message();

	void update_server_players(::cache& cache);
	void handle_middle_click(::cache& cache, bool active);
	void render_manager(bool embedded = false);
}
