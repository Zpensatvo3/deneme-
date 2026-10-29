#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "spotify_source.hpp"

#include <Windows.h>
#include <wincodec.h>
#include <winhttp.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string_view>

#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "winhttp.lib")

namespace
{
	using namespace winrt::Windows::Media::Control;

	constexpr auto poll_interval = std::chrono::milliseconds(400);
	constexpr int artwork_max_size = 640;
	constexpr const char* lyrics_user_agent =
		"Thedorius-Lite/1.0 (local overlay; https://lrclib.net)";

	// ---------------------------------------------------------------- helpers

	bool contains_ignore_case(const winrt::hstring& haystack, const char* needle)
	{
		const std::string lowered = winrt::to_string(haystack);
		std::string subject = lowered;
		std::string pattern = needle;
		std::transform(subject.begin(), subject.end(), subject.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::transform(pattern.begin(), pattern.end(), pattern.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return subject.find(pattern) != std::string::npos;
	}

	bool is_spotify_session(const GlobalSystemMediaTransportControlsSession& session)
	{
		if (!session) return false;
		try
		{
			const auto id = session.SourceAppUserModelId();
			if (contains_ignore_case(id, "spotify")) return true;
		}
		catch (...) {}
		return false;
	}

	void trim(std::string& value)
	{
		const auto first = value.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) { value.clear(); return; }
		const auto last = value.find_last_not_of(" \t\r\n");
		value = value.substr(first, last - first + 1);
	}

	std::string url_encode(std::string_view value)
	{
		static const char* digits = "0123456789ABCDEF";
		std::string encoded;
		encoded.reserve(value.size() * 3);
		for (const unsigned char character : value)
		{
			const bool unreserved =
				(character >= 'A' && character <= 'Z') ||
				(character >= 'a' && character <= 'z') ||
				(character >= '0' && character <= '9') ||
				character == '-' || character == '_' || character == '.' || character == '~';
			if (unreserved) encoded.push_back(static_cast<char>(character));
			else
			{
				encoded.push_back('%');
				encoded.push_back(digits[character >> 4]);
				encoded.push_back(digits[character & 0x0F]);
			}
		}
		return encoded;
	}

	std::wstring widen(std::string_view value)
	{
		if (value.empty()) return {};
		const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(),
			static_cast<int>(value.size()), nullptr, 0);
		std::wstring wide(static_cast<std::size_t>(size < 0 ? 0 : size), L'\0');
		if (size > 0)
		{
			MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
				wide.data(), size);
		}
		return wide;
	}

	// ------------------------------------------------------------ HTTP (WinHTTP)

	class internet_handle
	{
	public:
		internet_handle() = default;
		explicit internet_handle(HINTERNET value) noexcept : value_(value) {}
		~internet_handle() { reset(); }

		internet_handle(const internet_handle&) = delete;
		internet_handle& operator=(const internet_handle&) = delete;

		internet_handle(internet_handle&& other) noexcept
			: value_(other.release()) {}

		internet_handle& operator=(internet_handle&& other) noexcept
		{
			if (this != &other)
			{
				reset();
				value_ = other.release();
			}
			return *this;
		}

		HINTERNET get() const { return value_; }
		explicit operator bool() const { return value_ != nullptr; }

		HINTERNET release() noexcept
		{
			HINTERNET value = value_;
			value_ = nullptr;
			return value;
		}

		void reset(HINTERNET value = nullptr)
		{
			if (value_) WinHttpCloseHandle(value_);
			value_ = value;
		}

	private:
		HINTERNET value_ = nullptr;
	};

	bool http_get(const std::string& host, const std::string& path, std::string& out)
	{
		internet_handle session(WinHttpOpen(L"Thedorius-Lite/1.0",
			WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
		if (!session) return false;

		const DWORD timeout = 8000;
		WinHttpSetTimeouts(session.get(), static_cast<int>(timeout),
			static_cast<int>(timeout), static_cast<int>(timeout), static_cast<int>(timeout));

		const std::wstring wide_host = widen(host);
		const std::wstring wide_path = widen(path);

		internet_handle connection(WinHttpConnect(session.get(), wide_host.c_str(),
			INTERNET_DEFAULT_HTTPS_PORT, 0));
		if (!connection) return false;

		internet_handle request(WinHttpOpenRequest(connection.get(), L"GET", wide_path.c_str(),
			nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
		if (!request) return false;

		const std::wstring headers = L"User-Agent: " + widen(lyrics_user_agent) + L"\r\nAccept: application/json\r\n";
		if (!WinHttpSendRequest(request.get(), headers.c_str(),
			static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) return false;
		if (!WinHttpReceiveResponse(request.get(), nullptr)) return false;

		DWORD status = 0;
		DWORD status_size = sizeof(status);
		if (!WinHttpQueryHeaders(request.get(),
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX) ||
			status != 200)
			return false;

		out.clear();
		char buffer[4096];
		for (;;)
		{
			DWORD available = 0;
			if (!WinHttpQueryDataAvailable(request.get(), &available)) return false;
			if (available == 0) break;
			const DWORD chunk = (std::min)(available, static_cast<DWORD>(sizeof(buffer)));
			DWORD read = 0;
			if (!WinHttpReadData(request.get(), buffer, chunk, &read)) return false;
			if (read == 0) break;
			if (out.size() + read > 2u * 1024u * 1024u) return false;
			out.append(buffer, buffer + read);
		}
		return !out.empty();
	}

	// --------------------------------------------------------- JSON (lrclib)

	// Minimal scanner: lrclib only needs "syncedLyrics", an array of
	// [milliseconds, "line"] pairs. Avoids pulling a JSON dependency into the DLL.
	struct json_reader
	{
		const std::string& text;
		std::size_t cursor = 0;

		explicit json_reader(const std::string& source) : text(source) {}

		void skip_space()
		{
			while (cursor < text.size() &&
				(text[cursor] == ' ' || text[cursor] == '\t' ||
					text[cursor] == '\r' || text[cursor] == '\n'))
				++cursor;
		}

		bool consume(char expected)
		{
			skip_space();
			if (cursor >= text.size() || text[cursor] != expected) return false;
			++cursor;
			return true;
		}

		bool peek(char expected)
		{
			skip_space();
			return cursor < text.size() && text[cursor] == expected;
		}

		bool parse_number(double& out)
		{
			skip_space();
			const std::size_t begin = cursor;
			if (cursor < text.size() && (text[cursor] == '-' || text[cursor] == '+')) ++cursor;
			while (cursor < text.size() &&
				((text[cursor] >= '0' && text[cursor] <= '9') || text[cursor] == '.' ||
					text[cursor] == 'e' || text[cursor] == 'E' || text[cursor] == '-' ||
					text[cursor] == '+'))
				++cursor;
			if (cursor == begin) return false;
			out = std::strtod(text.substr(begin, cursor - begin).c_str(), nullptr);
			return true;
		}

		static void append_utf8(std::string& out, unsigned int codepoint)
		{
			if (codepoint <= 0x7F) out.push_back(static_cast<char>(codepoint));
			else if (codepoint <= 0x7FF)
			{
				out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
				out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
			}
			else if (codepoint <= 0xFFFF)
			{
				out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
				out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
			}
			else
			{
				out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
				out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
			}
		}

		bool parse_hex4(unsigned int& out)
		{
			if (cursor + 4 > text.size()) return false;
			unsigned int value = 0;
			for (int index = 0; index < 4; ++index)
			{
				const char character = text[cursor++];
				value <<= 4;
				if (character >= '0' && character <= '9') value |= static_cast<unsigned>(character - '0');
				else if (character >= 'a' && character <= 'f') value |= static_cast<unsigned>(character - 'a' + 10);
				else if (character >= 'A' && character <= 'F') value |= static_cast<unsigned>(character - 'A' + 10);
				else return false;
			}
			out = value;
			return true;
		}

		bool parse_string(std::string& out)
		{
			if (!consume('"')) return false;
			out.clear();
			while (cursor < text.size())
			{
				const char character = text[cursor++];
				if (character == '"') return true;
				if (character != '\\') { out.push_back(character); continue; }
				if (cursor >= text.size()) return false;
				const char escape = text[cursor++];
				switch (escape)
				{
				case '"': out.push_back('"'); break;
				case '\\': out.push_back('\\'); break;
				case '/': out.push_back('/'); break;
				case 'b': out.push_back('\b'); break;
				case 'f': out.push_back('\f'); break;
				case 'n': out.push_back('\n'); break;
				case 'r': out.push_back('\r'); break;
				case 't': out.push_back('\t'); break;
				case 'u':
				{
					unsigned int codepoint = 0;
					if (!parse_hex4(codepoint)) return false;
					if (codepoint >= 0xD800 && codepoint <= 0xDBFF &&
						cursor + 1 < text.size() && text[cursor] == '\\' && text[cursor + 1] == 'u')
					{
						cursor += 2;
						unsigned int low = 0;
						if (parse_hex4(low))
							codepoint = 0x10000u + ((codepoint - 0xD800u) << 10) + (low - 0xDC00u);
					}
					append_utf8(out, codepoint);
					break;
				}
				default: out.push_back(escape); break;
				}
			}
			return false;
		}
	};

	std::string clean_title(std::string_view raw)
	{
		std::string s(raw);
		trim(s);

		// Look for dash-separated suffixes like " - 2020 Remaster", " - feat. ...", " - Radio Edit"
		const std::size_t pos_dash = s.find(" - ");
		if (pos_dash != std::string::npos)
		{
			std::string suffix = s.substr(pos_dash + 3);
			std::string lower = suffix;
			std::transform(lower.begin(), lower.end(), lower.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (lower.find("remaster") != std::string::npos ||
				lower.find("feat") != std::string::npos ||
				lower.find("ft.") != std::string::npos ||
				lower.find("edit") != std::string::npos ||
				lower.find("version") != std::string::npos ||
				lower.find("mix") != std::string::npos ||
				lower.find("live") != std::string::npos ||
				lower.find("deluxe") != std::string::npos ||
				lower.find("bonus") != std::string::npos ||
				lower.find("acoustic") != std::string::npos ||
				lower.find("anniversary") != std::string::npos ||
				lower.find("original") != std::string::npos ||
				lower.find("mono") != std::string::npos ||
				lower.find("stereo") != std::string::npos ||
				lower.find("instrumental") != std::string::npos)
			{
				s = s.substr(0, pos_dash);
				trim(s);
			}
		}

		// Strip parentheses or brackets containing secondary tags like (feat. ...), [Remastered]
		auto strip_tags = [&](char open_ch, char close_ch)
		{
			std::size_t open_pos = s.find(open_ch);
			while (open_pos != std::string::npos)
			{
				const std::size_t close_pos = s.find(close_ch, open_pos + 1);
				if (close_pos == std::string::npos) break;
				std::string inside = s.substr(open_pos + 1, close_pos - open_pos - 1);
				std::string lower = inside;
				std::transform(lower.begin(), lower.end(), lower.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				if (lower.find("feat") != std::string::npos ||
					lower.find("ft.") != std::string::npos ||
					lower.find("with") != std::string::npos ||
					lower.find("remaster") != std::string::npos ||
					lower.find("deluxe") != std::string::npos ||
					lower.find("version") != std::string::npos ||
					lower.find("edit") != std::string::npos ||
					lower.find("bonus") != std::string::npos ||
					lower.find("live") != std::string::npos ||
					lower.find("acoustic") != std::string::npos ||
					lower.find("official") != std::string::npos ||
					lower.find("video") != std::string::npos)
				{
					s.erase(open_pos, close_pos - open_pos + 1);
					trim(s);
					open_pos = s.find(open_ch);
				}
				else
				{
					open_pos = s.find(open_ch, close_pos + 1);
				}
			}
		};

		strip_tags('(', ')');
		strip_tags('[', ']');
		trim(s);
		return s.empty() ? std::string(raw) : s;
	}

	std::string clean_artist(std::string_view raw)
	{
		std::string s(raw);
		trim(s);
		const std::size_t cut = s.find_first_of(",;/");
		if (cut != std::string::npos)
		{
			s = s.substr(0, cut);
			trim(s);
		}
		std::string lower = s;
		std::transform(lower.begin(), lower.end(), lower.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		const std::size_t feat_pos = lower.find(" feat");
		if (feat_pos != std::string::npos)
		{
			s = s.substr(0, feat_pos);
			trim(s);
		}
		const std::size_t ft_pos = lower.find(" ft");
		if (ft_pos != std::string::npos)
		{
			s = s.substr(0, ft_pos);
			trim(s);
		}
		return s.empty() ? std::string(raw) : s;
	}

	// LRC timestamps are "[mm:ss.xx]" or "[hh:mm:ss.xx]".
	bool parse_lrc_timestamp(std::string_view tag, double& out)
	{
		if (tag.empty()) return false;
		if (tag.front() == '[') tag.remove_prefix(1);
		if (!tag.empty() && tag.back() == ']') tag.remove_suffix(1);
		if (tag.empty()) return false;

		// Metadata lines like "ar:Artist" or "ti:Title" start with letters, not digits
		if (!std::isdigit(static_cast<unsigned char>(tag.front()))) return false;

		std::vector<std::string_view> parts;
		std::size_t start = 0;
		while (start < tag.size())
		{
			const std::size_t colon = tag.find(':', start);
			if (colon == std::string_view::npos)
			{
				parts.push_back(tag.substr(start));
				break;
			}
			parts.push_back(tag.substr(start, colon - start));
			start = colon + 1;
		}

		if (parts.size() < 2 || parts.size() > 3) return false;

		auto parse_num = [](std::string_view sv, double& val) -> bool
		{
			if (sv.empty()) return false;
			std::string str(sv);
			for (char& ch : str) { if (ch == ',') ch = '.'; }
			char* end_ptr = nullptr;
			val = std::strtod(str.c_str(), &end_ptr);
			return end_ptr != str.c_str();
		};

		double hours = 0.0;
		double minutes = 0.0;
		double seconds = 0.0;

		if (parts.size() == 2)
		{
			// mm:ss or mm:ss.xx
			if (!parse_num(parts[0], minutes)) return false;
			if (!parse_num(parts[1], seconds)) return false;
		}
		else if (parts.size() == 3)
		{
			// hh:mm:ss or hh:mm:ss.xx
			if (!parse_num(parts[0], hours)) return false;
			if (!parse_num(parts[1], minutes)) return false;
			if (!parse_num(parts[2], seconds)) return false;
		}

		out = hours * 3600.0 + minutes * 60.0 + seconds;
		return true;
	}

	// lrclib ships synced lyrics as a raw LRC block inside a JSON string.
	bool parse_lrc_block(const std::string& block, std::vector<modules::spotify::lyric_line>& out)
	{
		std::vector<modules::spotify::lyric_line> parsed;
		std::size_t cursor = 0;
		while (cursor < block.size())
		{
			std::size_t line_end = block.find('\n', cursor);
			if (line_end == std::string::npos) line_end = block.size();
			std::string_view raw(block.data() + cursor, line_end - cursor);
			cursor = line_end + 1;

			// A line may carry several timestamps, e.g. "[00:12.00][01:30.00] chorus".
			std::vector<double> stamps;
			std::size_t offset = 0;
			while (offset < raw.size() && raw[offset] == '[')
			{
				const std::size_t close = raw.find(']', offset + 1);
				if (close == std::string_view::npos) break;
				double timestamp = 0.0;
				if (parse_lrc_timestamp(raw.substr(offset + 1, close - offset - 1), timestamp))
					stamps.push_back(timestamp);
				offset = close + 1;
			}
			if (stamps.empty()) continue; // metadata tags such as [ar:Artist]

			std::string text(raw.substr(offset));
			trim(text);
			if (text.empty()) continue;
			for (const double timestamp : stamps)
				parsed.push_back({ timestamp, text });
		}

		if (parsed.empty()) return false;

		std::stable_sort(parsed.begin(), parsed.end(),
			[](const modules::spotify::lyric_line& a, const modules::spotify::lyric_line& b)
			{
				return a.timestamp < b.timestamp;
			});

		out = std::move(parsed);
		return true;
	}

	bool parse_plain_lyrics(const std::string& block, double duration,
		std::vector<modules::spotify::lyric_line>& out)
	{
		std::vector<std::string> lines;
		std::size_t cursor = 0;
		while (cursor < block.size())
		{
			std::size_t line_end = block.find('\n', cursor);
			if (line_end == std::string::npos) line_end = block.size();
			std::string line(block.data() + cursor, line_end - cursor);
			cursor = line_end + 1;
			trim(line);
			if (!line.empty()) lines.push_back(std::move(line));
		}
		if (lines.empty()) return false;

		out.clear();
		out.reserve(lines.size());
		const double song_dur = duration > 10.0 ? duration : (static_cast<double>(lines.size()) * 4.0);
		const double step = song_dur / static_cast<double>(lines.size() + 1);
		for (std::size_t i = 0; i < lines.size(); ++i)
		{
			out.push_back({ static_cast<double>(i) * step, std::move(lines[i]) });
		}
		return !out.empty();
	}

	// Older lrclib responses encoded the same data as [[ms, "line"], ...].
	bool parse_synced_array(json_reader& reader, std::vector<modules::spotify::lyric_line>& out)
	{
		std::vector<modules::spotify::lyric_line> parsed;
		while (!reader.consume(']'))
		{
			if (!reader.consume('[')) return false;
			double timestamp = 0.0;
			reader.parse_number(timestamp);
			if (!reader.consume(',')) return false;
			std::string line;
			if (!reader.parse_string(line)) return false;
			if (!reader.consume(']')) return false;

			modules::spotify::lyric_line entry;
			entry.timestamp = timestamp / 1000.0;
			entry.text = std::move(line);
			trim(entry.text);
			if (!entry.text.empty()) parsed.push_back(std::move(entry));

			if (reader.consume(',')) continue;
			if (!reader.consume(']')) return false;
		}

		if (parsed.empty()) return false;
		out = std::move(parsed);
		return true;
	}

	bool find_and_parse_synced_lyrics(const std::string& document, std::size_t& search_from,
		std::vector<modules::spotify::lyric_line>& out)
	{
		const std::string key = "\"syncedLyrics\"";
		const std::size_t key_position = document.find(key, search_from);
		if (key_position == std::string::npos) return false;

		json_reader reader(document);
		reader.cursor = key_position + key.size();
		search_from = reader.cursor;
		if (!reader.consume(':')) return true;

		std::vector<modules::spotify::lyric_line> parsed;
		if (reader.peek('"'))
		{
			std::string block;
			if (reader.parse_string(block))
			{
				parse_lrc_block(block, parsed);
			}
		}
		else if (reader.consume('['))
		{
			parse_synced_array(reader, parsed);
		}

		search_from = reader.cursor;
		if (!parsed.empty())
		{
			out = std::move(parsed);
		}
		return true;
	}

	bool parse_plain_lyrics_from_json(const std::string& document, std::size_t search_from,
		double duration, std::vector<modules::spotify::lyric_line>& out)
	{
		const std::string key = "\"plainLyrics\"";
		const std::size_t key_position = document.find(key, search_from);
		if (key_position == std::string::npos) return false;

		json_reader reader(document);
		reader.cursor = key_position + key.size();
		if (!reader.consume(':')) return false;

		if (reader.peek('"'))
		{
			std::string block;
			if (reader.parse_string(block))
			{
				return parse_plain_lyrics(block, duration, out);
			}
		}
		return false;
	}

	std::string make_track_query(const std::string& title, const std::string& artist,
		const std::string& album, const char* prefix)
	{
		std::string query = prefix;
		query += "track_name=" + url_encode(title);
		if (!artist.empty()) query += "&artist_name=" + url_encode(artist);
		if (!album.empty()) query += "&album_name=" + url_encode(album);
		return query;
	}
}

int modules::spotify::now_playing::current_lyric() const
{
	if (lyrics.empty()) return -1;
	int index = -1;
	for (std::size_t scan = 0; scan < lyrics.size(); ++scan)
	{
		if (lyrics[scan].timestamp <= position + 0.05) index = static_cast<int>(scan);
		else break;
	}
	return index;
}

modules::spotify::source& modules::spotify::source::instance()
{
	static source singleton;
	return singleton;
}

modules::spotify::source::~source()
{
	stop();
}

void modules::spotify::source::start(bool enable_lyrics)
{
	lyrics_enabled.store(enable_lyrics, std::memory_order_relaxed);
	if (running.load(std::memory_order_acquire)) return;

	running.store(true, std::memory_order_release);
	refresh_requested.store(true, std::memory_order_release);
	try
	{
		worker_thread = std::thread([this] { worker(); });
	}
	catch (...)
	{
		running.store(false, std::memory_order_release);
	}
}

void modules::spotify::source::stop()
{
	if (!running.exchange(false, std::memory_order_acq_rel)) return;
	wake_signal.notify_all();
	if (worker_thread.joinable()) worker_thread.join();

	std::lock_guard lock(state_mutex);
	// Keep the revision counter moving across restarts so a later re-enable
	// cannot mistake a freshly decoded cover for the one already on the GPU.
	const std::uint64_t revision = state.artwork_revision;
	state = now_playing{};
	state.artwork_revision = revision;
}

void modules::spotify::source::set_lyrics_enabled(bool enable_lyrics)
{
	if (lyrics_enabled.exchange(enable_lyrics, std::memory_order_relaxed) == enable_lyrics) return;
	refresh_requested.store(true, std::memory_order_release);
	wake_signal.notify_all();
}


modules::spotify::now_playing modules::spotify::source::snapshot() const
{
	std::lock_guard lock(state_mutex);
	now_playing copy = state;
	// SMTC only samples the position when it changes; extrapolate between polls.
	if (copy.active && copy.playing)
	{
		const double elapsed = std::chrono::duration<double>(
			std::chrono::steady_clock::now() - copy.sampled_at).count();
		if (elapsed > 0.0 && elapsed < 5.0)
		{
			copy.position += elapsed;
			if (copy.duration > 0.0 && copy.position > copy.duration)
				copy.position = copy.duration;
		}
	}

	static double last_snapshot_pos = 0.0;
	static std::string last_snapshot_track;
	if (last_snapshot_track != copy.title)
	{
		last_snapshot_track = copy.title;
		last_snapshot_pos = copy.position;
	}
	else if (copy.playing && copy.active)
	{
		// Guarantee strictly non-decreasing position during normal playback,
		// only allowing backward jumps if it's a deliberate user seek (> 1.5 seconds)
		if (copy.position < last_snapshot_pos && (last_snapshot_pos - copy.position) < 1.5)
		{
			copy.position = last_snapshot_pos;
		}
		else
		{
			last_snapshot_pos = copy.position;
		}
	}
	else
	{
		last_snapshot_pos = copy.position;
	}

	return copy;
}

void modules::spotify::source::publish(now_playing& local)
{
	local.sampled_at = std::chrono::steady_clock::now();
	std::lock_guard lock(state_mutex);
	state = local;
}

void modules::spotify::source::worker()
{
	// SMTC requires a COM/WinRT apartment; this thread owns its own.
	bool apartment_ready = false;
	try
	{
		winrt::init_apartment(winrt::apartment_type::multi_threaded);
		apartment_ready = true;
	}
	catch (...)
	{
		try
		{
			winrt::init_apartment(winrt::apartment_type::single_threaded);
			apartment_ready = true;
		}
		catch (...) { apartment_ready = false; }
	}

	now_playing local;
	std::string active_key;

	while (running.load(std::memory_order_acquire))
	{
		if (apartment_ready)
		{
			try { poll(local, active_key); }
			catch (...) { local.active = false; }
		}

		try { publish(local); }
		catch (...) { break; }

		std::unique_lock lock(wake_mutex);
		wake_signal.wait_for(lock, poll_interval, [this]
		{
			return !running.load(std::memory_order_acquire) ||
				refresh_requested.load(std::memory_order_acquire);
		});
		refresh_requested.store(false, std::memory_order_release);
	}

	if (apartment_ready)
	{
		try { winrt::uninit_apartment(); }
		catch (...) { }
	}
}

void modules::spotify::source::poll(now_playing& local, std::string& active_key)
{
	const auto manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();

	GlobalSystemMediaTransportControlsSession session{ nullptr };
	if (const auto current = manager.GetCurrentSession())
	{
		if (is_spotify_session(current)) session = current;
	}
	if (!session)
	{
		for (const auto& candidate : manager.GetSessions())
		{
			if (is_spotify_session(candidate)) { session = candidate; break; }
		}
	}
	// Fallback 1: Use current active session
	if (!session)
	{
		try
		{
			if (const auto current = manager.GetCurrentSession())
				session = current;
		}
		catch (...) {}
	}
	// Fallback 2: Find any candidate session that is currently playing
	if (!session)
	{
		try
		{
			for (const auto& candidate : manager.GetSessions())
			{
				if (candidate && candidate.GetPlaybackInfo().PlaybackStatus() ==
					GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing)
				{
					session = candidate;
					break;
				}
			}
		}
		catch (...) {}
	}
	// Fallback 3: Take first available session
	if (!session)
	{
		try
		{
			auto sessions = manager.GetSessions();
			if (sessions.Size() > 0)
				session = sessions.GetAt(0);
		}
		catch (...) {}
	}

	if (!session)
	{
		local.active = false;
		local.playing = false;
		active_key.clear();
		return;
	}

	const auto properties = session.TryGetMediaPropertiesAsync().get();
	std::string title = winrt::to_string(properties.Title());
	std::string artist = winrt::to_string(properties.Artist());
	std::string album = winrt::to_string(properties.AlbumTitle());
	trim(title);
	trim(artist);
	trim(album);
	if (title.empty()) title = "Unknown track";

	const auto playback = session.GetPlaybackInfo();
	const bool playing =
		playback.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;

	const auto timeline = session.GetTimelineProperties();
	const double base_position = static_cast<double>(timeline.Position().count()) / 1e7;
	const double duration = static_cast<double>(timeline.EndTime().count()) / 1e7;
	const auto last_updated = timeline.LastUpdatedTime();

	const auto now = std::chrono::steady_clock::now();
	const std::string key = title + "\x1f" + artist;
	const bool track_changed = (key != active_key);

	double real_position = base_position;
	if (playing && last_updated.time_since_epoch().count() > 0)
	{
		const auto now_winrt = winrt::clock::now();
		const int64_t diff = now_winrt.time_since_epoch().count() - last_updated.time_since_epoch().count();
		if (diff > 0)
		{
			const double elapsed = static_cast<double>(diff) / 1e7;
			real_position = base_position + elapsed;
		}
	}

	if (duration > 0.0 && real_position > duration) real_position = duration;
	if (real_position < 0.0) real_position = 0.0;

	// In normal playback, ensure monotonic progress.
	// Only accept a backward jump if it's a genuine deliberate seek (> 1.5 seconds)
	if (playing && !track_changed && local.active)
	{
		if (real_position < local.position && (local.position - real_position) < 1.5)
		{
			real_position = local.position;
		}
	}

	local.active = true;
	local.playing = playing;
	local.title = title;
	local.artist = artist;
	local.album = album;
	local.duration = (std::max)(0.0, duration);
	local.position = real_position;
	local.sampled_at = now;

	// Everything below is per-track work: only run it when the song changed.
	if (!track_changed) return;
	active_key = key;

	local.lyrics.clear();
	local.lyrics_loaded = false;

	// Album art: read the SMTC thumbnail once per track.
	local.artwork.clear();
	local.artwork_width = 0;
	local.artwork_height = 0;
	try
	{
		const auto reference = properties.Thumbnail();
		if (reference)
		{
			const auto stream = reference.OpenReadAsync().get();
			const uint32_t stream_size = static_cast<uint32_t>(stream.Size());
			if (stream_size > 0 && stream_size < 8u * 1024u * 1024u)
			{
				std::vector<unsigned char> encoded(stream_size);
				winrt::Windows::Storage::Streams::DataReader reader(stream);
				reader.LoadAsync(stream_size).get();
				reader.ReadBytes(winrt::array_view<unsigned char>(encoded));

				int width = 0;
				int height = 0;
				std::vector<unsigned char> pixels;
				if (decode_artwork(encoded, width, height, pixels) && width > 0 && height > 0)
				{
					local.artwork = std::move(pixels);
					local.artwork_width = width;
					local.artwork_height = height;
				}
			}
		}
	}
	catch (...) { }

	// Bump unconditionally: when a cover fails to decode the renderer must
	// still drop the previous track's art instead of keeping it on screen.
	++local.artwork_revision;

	if (lyrics_enabled.load(std::memory_order_relaxed))
	{
		refresh_lyrics(local, title, artist, album, local.duration);
	}
}

bool modules::spotify::source::decode_artwork(const std::vector<unsigned char>& encoded,
	int& width, int& height, std::vector<unsigned char>& pixels) const
{
	if (encoded.empty() || encoded.size() > 8u * 1024u * 1024u) return false;

	IWICImagingFactory* factory = nullptr;
	IWICStream* stream = nullptr;
	IWICBitmapDecoder* decoder = nullptr;
	IWICBitmapFrameDecode* frame = nullptr;
	IWICBitmapScaler* scaler = nullptr;
	IWICFormatConverter* converter = nullptr;
	UINT output_width = 0;
	UINT output_height = 0;
	bool success = false;

	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&factory))) || !factory)
		return false;
	if (FAILED(factory->CreateStream(&stream)) || !stream ||
		FAILED(stream->InitializeFromMemory(
			const_cast<BYTE*>(encoded.data()), static_cast<DWORD>(encoded.size()))) ||
		FAILED(factory->CreateDecoderFromStream(stream, nullptr,
			WICDecodeMetadataCacheOnLoad, &decoder)) || !decoder ||
		FAILED(decoder->GetFrame(0, &frame)) || !frame)
		goto cleanup;

	{
		UINT source_width = 0;
		UINT source_height = 0;
		if (FAILED(frame->GetSize(&source_width, &source_height)) ||
			source_width == 0 || source_height == 0)
			goto cleanup;

		// Cap the texture: the HUD draws the art at roughly 96 px, so anything
		// beyond ~320 px is wasted VRAM and upload time.
		const UINT longest = (std::max)(source_width, source_height);
		const double factor = longest > static_cast<UINT>(artwork_max_size)
			? static_cast<double>(artwork_max_size) / static_cast<double>(longest)
			: 1.0;
		output_width = static_cast<UINT>(std::lround(source_width * factor));
		output_height = static_cast<UINT>(std::lround(source_height * factor));
		if (output_width == 0) output_width = 1;
		if (output_height == 0) output_height = 1;
	}

	if (FAILED(factory->CreateBitmapScaler(&scaler)) || !scaler ||
		FAILED(scaler->Initialize(frame, output_width, output_height,
			WICBitmapInterpolationModeHighQualityCubic)) ||
		FAILED(factory->CreateFormatConverter(&converter)) || !converter ||
		FAILED(converter->Initialize(scaler, GUID_WICPixelFormat32bppRGBA,
			WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
		goto cleanup;

	pixels.resize(static_cast<std::size_t>(output_width) * output_height * 4u);
	if (FAILED(converter->CopyPixels(nullptr, output_width * 4u,
		static_cast<UINT>(pixels.size()), pixels.data())))
	{
		pixels.clear();
		goto cleanup;
	}

	width = static_cast<int>(output_width);
	height = static_cast<int>(output_height);
	success = true;

cleanup:
	if (converter) converter->Release();
	if (scaler) scaler->Release();
	if (frame) frame->Release();
	if (decoder) decoder->Release();
	if (stream) stream->Release();
	if (factory) factory->Release();
	if (!success) { pixels.clear(); width = 0; height = 0; }
	return success;
}

void modules::spotify::source::refresh_lyrics(now_playing& local, const std::string& title,
	const std::string& artist, const std::string& album, double duration)
{
	if (!lyrics_enabled.load(std::memory_order_relaxed)) return;

	const std::string c_title = clean_title(title);
	const std::string c_artist = clean_artist(artist);

	std::vector<lyric_line> parsed;
	std::string document;

	// Cascade 1: Exact get with clean title and clean artist
	std::string path = "/api/get?" + make_track_query(c_title, c_artist, "", "");
	if (http_get("lrclib.net", path, document))
	{
		std::size_t search_from = 0;
		if (find_and_parse_synced_lyrics(document, search_from, parsed) && !parsed.empty())
		{
			local.lyrics = std::move(parsed);
			local.lyrics_loaded = true;
			return;
		}
	}

	// Cascade 2: Exact get with raw title and raw artist
	if (c_title != title || c_artist != artist)
	{
		document.clear();
		path = "/api/get?" + make_track_query(title, artist, "", "");
		if (http_get("lrclib.net", path, document))
		{
			std::size_t search_from = 0;
			if (find_and_parse_synced_lyrics(document, search_from, parsed) && !parsed.empty())
			{
				local.lyrics = std::move(parsed);
				local.lyrics_loaded = true;
				return;
			}
		}
	}

	// Cascade 3: Search with clean artist and clean title
	document.clear();
	path = "/api/search?q=" + url_encode(c_artist + " " + c_title);
	if (!http_get("lrclib.net", path, document))
	{
		// Cascade 4: Search with just clean title
		document.clear();
		path = "/api/search?q=" + url_encode(c_title);
		if (!http_get("lrclib.net", path, document))
		{
			local.lyrics.clear();
			local.lyrics_loaded = true;
			return;
		}
	}

	// Scan candidate entries for synced lyrics
	std::size_t search_from = 0;
	while (search_from < document.size())
	{
		parsed.clear();
		const std::size_t prev = search_from;
		if (!find_and_parse_synced_lyrics(document, search_from, parsed)) break;
		if (!parsed.empty())
		{
			local.lyrics = std::move(parsed);
			local.lyrics_loaded = true;
			return;
		}
		if (search_from <= prev) break;
	}

	// Cascade 5: If no synced lyrics, fall back to plain lyrics
	parsed.clear();
	if (parse_plain_lyrics_from_json(document, 0, duration, parsed) && !parsed.empty())
	{
		local.lyrics = std::move(parsed);
		local.lyrics_loaded = true;
		return;
	}

	local.lyrics.clear();
	local.lyrics_loaded = true;
}
