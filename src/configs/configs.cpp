#include "configs.hpp"

#include "transport.hpp"

#include <charconv>
#include <condition_variable>
#include <cctype>
#include <deque>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

namespace
{
	enum class action_type { refresh, publish, load, remove };

	struct action
	{
		action_type type = action_type::refresh;
		int id = 0;
		std::string name;
		std::string description;
		std::string author_name;
		std::string payload;
		int module_count = 0;
	};

	std::mutex state_mutex;
	std::condition_variable work_ready;
	std::deque<action> pending_actions;
	configs::snapshot current_snapshot;
	std::string pending_payload;
	std::string pending_name;
	bool running = false;
	bool stopping = false;
	std::thread worker;

	std::string json_escape(std::string_view value)
	{
		std::string output;
		output.reserve(value.size() + 8);
		for (char character : value)
		{
			switch (character)
			{
			case '\\': output += "\\\\"; break;
			case '"': output += "\\\""; break;
			case '\n': output += "\\n"; break;
			case '\r': output += "\\r"; break;
			case '\t': output += "\\t"; break;
			default:
				if (static_cast<unsigned char>(character) < 0x20) {
					constexpr char hex[] = "0123456789abcdef";
					output += "\\u00";
					output += hex[static_cast<unsigned char>(character) >> 4];
					output += hex[static_cast<unsigned char>(character) & 15];
				} else output.push_back(character);
				break;
			}
		}
		return output;
	}

	void skip_spaces(std::string_view value, std::size_t& position)
	{
		while (position < value.size() && std::isspace(static_cast<unsigned char>(value[position])))
			++position;
	}

	std::optional<std::string> json_string(std::string_view value, std::string_view key)
	{
		const std::string needle = "\"" + std::string(key) + "\"";
		std::size_t position = value.find(needle);
		if (position == std::string_view::npos) return std::nullopt;
		position += needle.size();
		skip_spaces(value, position);
		if (position >= value.size() || value[position++] != ':') return std::nullopt;
		skip_spaces(value, position);
		if (position >= value.size() || value[position++] != '"') return std::nullopt;
		std::string output;
		while (position < value.size())
		{
			const char character = value[position++];
			if (character == '"') return output;
			if (character != '\\') { output.push_back(character); continue; }
			if (position >= value.size()) return std::nullopt;
			switch (value[position++])
			{
			case '\\': output.push_back('\\'); break;
			case '"': output.push_back('"'); break;
			case 'n': output.push_back('\n'); break;
			case 'r': output.push_back('\r'); break;
			case 't': output.push_back('\t'); break;
			case '/': output.push_back('/'); break;
			default: return std::nullopt;
			}
		}
		return std::nullopt;
	}

	std::optional<int> json_int(std::string_view value, std::string_view key)
	{
		const std::string needle = "\"" + std::string(key) + "\"";
		std::size_t position = value.find(needle);
		if (position == std::string_view::npos) return std::nullopt;
		position += needle.size();
		skip_spaces(value, position);
		if (position >= value.size() || value[position++] != ':') return std::nullopt;
		skip_spaces(value, position);
		int output = 0;
		const char* begin = value.data() + position;
		const auto parsed = std::from_chars(begin, value.data() + value.size(), output);
		return parsed.ec == std::errc{} ? std::optional<int>(output) : std::nullopt;
	}

	bool json_bool(std::string_view value, std::string_view key, bool fallback = false)
	{
		const std::string needle = "\"" + std::string(key) + "\"";
		std::size_t position = value.find(needle);
		if (position == std::string_view::npos) return fallback;
		position += needle.size();
		skip_spaces(value, position);
		if (position >= value.size() || value[position++] != ':') return fallback;
		skip_spaces(value, position);
		return value.substr(position, 4) == "true";
	}

	bool response_success(const configs::transport::service_result& response, std::string& message)
	{
		if (!response.success)
		{
			message = response.message.empty() ? "Config request failed." : response.message;
			return false;
		}
		if (!json_bool(response.body, "success"))
		{
			message = json_string(response.body, "message").value_or("Config request failed.");
			return false;
		}
		return true;
	}

	std::vector<configs::public_config> parse_list(std::string_view response)
	{
		std::vector<configs::public_config> output;
		const std::size_t array_key = response.find("\"configs\"");
		if (array_key == std::string_view::npos) return output;
		std::size_t position = response.find('[', array_key);
		if (position == std::string_view::npos) return output;

		while (++position < response.size())
		{
			skip_spaces(response, position);
			if (position >= response.size() || response[position] == ']') break;
			if (response[position] == ',') continue;
			if (response[position] != '{') break;
			const std::size_t start = position++;
			int depth = 1;
			bool quoted = false;
			bool escaped = false;
			while (position < response.size() && depth > 0)
			{
				const char character = response[position++];
				if (quoted)
				{
					if (escaped) escaped = false;
					else if (character == '\\') escaped = true;
					else if (character == '"') quoted = false;
					continue;
				}
				if (character == '"') quoted = true;
				else if (character == '{') ++depth;
				else if (character == '}') --depth;
			}
			if (depth != 0) break;
			const std::string_view entry(response.data() + start, position - start);
			const auto id = json_int(entry, "id");
			const auto name = json_string(entry, "name");
			const auto author = json_string(entry, "author_name");
			if (!id || !name || !author) continue;
			configs::public_config config;
			config.id = *id;
			config.name = *name;
			config.author_name = *author;
			config.description = json_string(entry, "description").value_or("");
			config.module_count = json_int(entry, "module_count").value_or(0);
			config.downloads = json_int(entry, "downloads").value_or(0);
			config.created_at = json_string(entry, "created_at").value_or("");
			config.updated_at = json_string(entry, "updated_at").value_or("");
			// SQLite CASE returns 0/1, while future API versions may return a
			// JSON boolean. Accept both forms for forward compatibility.
			config.mine = json_int(entry, "mine").value_or(0) != 0 || json_bool(entry, "mine");
			output.push_back(std::move(config));
		}
		return output;
	}

	void update_status(std::string status, bool success)
	{
		std::lock_guard lock(state_mutex);
		current_snapshot.status = std::move(status);
		current_snapshot.busy = !pending_actions.empty();
		current_snapshot.success = success;
		++current_snapshot.revision;
	}

	bool perform_refresh(std::string& message)
	{
		configs::transport::service_result response = configs::transport::post_json("/api/v1/configs/list", "");
		if (!response_success(response, message)) return false;
		std::vector<configs::public_config> entries = parse_list(response.body);
		{
			std::lock_guard lock(state_mutex);
			current_snapshot.entries = std::move(entries);
		}
		message = "Public configs refreshed.";
		return true;
	}

	void perform_action(const action& request)
	{
		std::string message;
		if (request.type == action_type::refresh)
		{
			const bool success = perform_refresh(message);
			update_status(message, success);
			return;
		}

		if (request.type == action_type::publish)
		{
			std::string fields = "\"name\":\"" + json_escape(request.name) +
				"\",\"description\":\"" + json_escape(request.description) +
				"\",\"author_name\":\"" + json_escape(request.author_name) +
				"\",\"payload\":\"" + json_escape(request.payload) +
				"\",\"module_count\":" + std::to_string(request.module_count);
			if (request.id > 0) fields += ",\"id\":" + std::to_string(request.id);
			configs::transport::service_result response = configs::transport::post_json("/api/v1/configs/publish", fields);
			const bool success = response_success(response, message);
			if (success) {
				message = request.id > 0 ? "Config updated publicly." : "Config published publicly.";
				std::string refresh_message;
				if (!perform_refresh(refresh_message)) message += " " + refresh_message;
			}
			update_status(message, success);
			return;
		}

		if (request.type == action_type::remove)
		{
			configs::transport::service_result response = configs::transport::post_json(
				"/api/v1/configs/delete", "\"id\":" + std::to_string(request.id));
			const bool success = response_success(response, message);
			if (success) {
				message = "Config deleted.";
				std::string refresh_message;
				if (!perform_refresh(refresh_message)) message += " " + refresh_message;
			}
			update_status(message, success);
			return;
		}

		configs::transport::service_result response = configs::transport::post_json(
			"/api/v1/configs/load", "\"id\":" + std::to_string(request.id));
		bool success = response_success(response, message);
		if (success)
		{
			const auto payload = json_string(response.body, "payload");
			const auto name = json_string(response.body, "name");
			if (payload && name) {
				std::lock_guard lock(state_mutex);
				pending_payload = *payload;
				pending_name = *name;
				message = "Config downloaded. Applying it...";
			} else {
				message = "Config response is invalid.";
				success = false;
			}
		}
		update_status(message, success);
	}

	void worker_loop()
	{
		while (true)
		{
			action request;
			{
				std::unique_lock lock(state_mutex);
				work_ready.wait(lock, [] { return stopping || !pending_actions.empty(); });
				if (stopping && pending_actions.empty()) break;
				request = std::move(pending_actions.front());
				pending_actions.pop_front();
			}
			perform_action(request);
		}
	}

	void enqueue(action request)
	{
		{
			std::lock_guard lock(state_mutex);
			if (!running || stopping) return;
			pending_actions.push_back(std::move(request));
			current_snapshot.busy = true;
			current_snapshot.status = "Contacting public config library...";
		}
		work_ready.notify_one();
	}
}

void configs::initialize()
{
	std::lock_guard lock(state_mutex);
	if (running) return;
	stopping = false;
	running = true;
	current_snapshot.status = "Open Configs to browse public builds.";
	worker = std::thread(worker_loop);
}

void configs::shutdown()
{
	{
		std::lock_guard lock(state_mutex);
		if (!running) return;
		stopping = true;
	}
	work_ready.notify_all();
	if (worker.joinable()) worker.join();
	std::lock_guard lock(state_mutex);
	running = false;
	pending_actions.clear();
}

configs::snapshot configs::get_snapshot()
{
	std::lock_guard lock(state_mutex);
	return current_snapshot;
}

void configs::refresh() { enqueue({ action_type::refresh }); }

void configs::publish(int update_id, const std::string& name, const std::string& description,
	const std::string& author_name, const std::string& payload, int module_count)
{
	action request;
	request.type = action_type::publish;
	request.id = update_id;
	request.name = name;
	request.description = description;
	request.author_name = author_name;
	request.payload = payload;
	request.module_count = module_count;
	enqueue(std::move(request));
}

void configs::load(int id) { enqueue({ action_type::load, id }); }
void configs::remove(int id) { enqueue({ action_type::remove, id }); }

bool configs::consume_pending_payload(std::string& payload, std::string& config_name)
{
	std::lock_guard lock(state_mutex);
	if (pending_payload.empty()) return false;
	payload = std::move(pending_payload);
	config_name = std::move(pending_name);
	pending_payload.clear();
	pending_name.clear();
	return true;
}
