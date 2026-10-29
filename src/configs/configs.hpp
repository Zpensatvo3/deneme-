#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace configs
{
	struct public_config
	{
		int id = 0;
		std::string name;
		std::string author_name;
		std::string description;
		int module_count = 0;
		int downloads = 0;
		std::string created_at;
		std::string updated_at;
		bool mine = false;
	};

	struct snapshot
	{
		std::vector<public_config> entries;
		bool busy = false;
		std::string status;
		bool success = true;
		std::uint64_t revision = 0; // Completed requests, not frame or queue updates.
	};

	void initialize();
	void shutdown();
	snapshot get_snapshot();
	void refresh();
	void publish(int update_id, const std::string& name, const std::string& description,
		const std::string& author_name, const std::string& payload, int module_count);
	void load(int id);
	void remove(int id);

	// Called by the menu thread. A downloaded config is never applied from the
	// networking worker, avoiding races with in-game module execution.
	bool consume_pending_payload(std::string& payload, std::string& config_name);
}
