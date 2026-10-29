#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Windows SMTC (GlobalSystemMediaTransportControls) polling service used by the
// Spotify HUD module.
//
// Everything that can block lives on a private worker thread: WinRT apartment
// calls, album art JPEG decoding and the lyrics HTTP request. Minecraft's render
// thread only ever reads a cheap, mutex-protected snapshot and uploads the
// decoded artwork to a GL texture.
namespace modules
{
	namespace spotify
	{
		struct lyric_line
		{
			double timestamp = 0.0;
			std::string text;
		};

		struct now_playing
		{
			bool active = false;
			bool playing = false;
			std::string title;
			std::string artist;
			std::string album;

			// Position is extrapolated from the last SMTC sample while playing,
			// so callers can use it directly without their own timing.
			double position = 0.0;
			double duration = 0.0;

			// Worker-only bookkeeping: when `position` was sampled. snapshot()
			// uses it to extrapolate, so the render thread never has to.
			std::chrono::steady_clock::time_point sampled_at{};

			// Artwork is decoded to straight 32bpp RGBA on the worker thread.
			// artwork_revision changes whenever new pixels are published.
			std::uint64_t artwork_revision = 0;
			int artwork_width = 0;
			int artwork_height = 0;
			std::vector<unsigned char> artwork;

			bool lyrics_loaded = false;
			std::vector<lyric_line> lyrics;

			// Index of the line that should be highlighted for `position`, or -1.
			int current_lyric() const;
		};

		class source
		{
		public:
			static source& instance();

			void start(bool enable_lyrics);
			void stop();
			void set_lyrics_enabled(bool enable_lyrics);

			// Ask the worker to re-scan immediately (used when the toggle flips).

			now_playing snapshot() const;
			bool is_running() const { return running.load(std::memory_order_acquire); }

		private:
			source() = default;
			~source();
			source(const source&) = delete;
			source& operator=(const source&) = delete;

			void worker();
			void poll(now_playing& local, std::string& active_key);
			void publish(now_playing& local);

			// WIC helpers, worker thread only.
			bool decode_artwork(const std::vector<unsigned char>& encoded,
				int& width, int& height, std::vector<unsigned char>& pixels) const;

			// Lyrics helpers, worker thread only.
			void refresh_lyrics(now_playing& local, const std::string& title,
				const std::string& artist, const std::string& album, double duration);

			mutable std::mutex state_mutex;
			now_playing state{};

			std::thread worker_thread;
			std::mutex wake_mutex;
			std::condition_variable wake_signal;
			std::atomic_bool running{ false };
			std::atomic_bool lyrics_enabled{ true };
			std::atomic_bool refresh_requested{ false };
		};
	}
}
