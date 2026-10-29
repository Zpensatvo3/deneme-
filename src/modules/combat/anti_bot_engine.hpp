#pragma once
#include "settings/anti_bot_settings.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace anti_bot_engine
{
    // Native-only state. Consumers may query the same player several times per
    // frame; confirmation advances with entity age, never with query count.
    class runtime
    {
        struct sample { int first_age, last_age; std::int64_t last_seen; };
        std::unordered_map<std::string, sample> missing_;
        std::int64_t next_prune_ = 0;
    public:
        void clear() { missing_.clear(); next_prune_ = 0; }
        std::size_t size() const { return missing_.size(); }
        bool observe(const module_settings::anti_bot& config, const std::string& identity,
            int age, bool reliable, bool listed, std::int64_t now)
        {
            if (!config.check_player_list || !reliable) { clear(); return false; }
            if (identity.empty()) return false;
            if (listed || age < config.spawn_grace_ticks || age < 0)
            {
                missing_.erase(identity); return false;
            }
            if (now >= next_prune_)
            {
                for (auto it = missing_.begin(); it != missing_.end();)
                    if (now - it->second.last_seen > 10000) it = missing_.erase(it); else ++it;
                next_prune_ = now + 1000;
            }
            auto it = missing_.find(identity);
            if (it == missing_.end())
            {
                if (missing_.size() >= 4096) clear(); // Bounded, conservative cache.
                missing_.emplace(identity, sample{age, age, now}); return false;
            }
            auto& state = it->second;
            if (age < state.last_age || age - state.last_age > 100 ||
                now < state.last_seen || now - state.last_seen > 5000)
            {
                state = {age, age, now}; return false;
            }
            state.last_age = age; state.last_seen = now;
            return age - state.first_age >= config.missing_list_ticks;
        }
    };
}
