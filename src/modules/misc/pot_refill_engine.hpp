#pragma once
#include "settings/pot_refill_settings.hpp"
#include <array>
#include <chrono>
#include <optional>
#include <random>
#include <span>

namespace modules::pot_refill_logic
{
    using clock = std::chrono::steady_clock;
    struct slot
    {
        int inventory_index = -1;
        int screen_id = -1;
        bool empty = false;
        bool healing = false;
        int count = 0;
    };
    struct move
    {
        int source_index, source_screen_id, target_index, target_screen_id;
        bool operator==(const move&) const = default;
    };

    // Inventory indices determine the order; screen IDs are packet addresses,
    // never assumed to match the layout of PlayerScreenHandler.
    template<class Rng>
    std::optional<move> select(std::span<const slot> slots, int order, Rng& random)
    {
        std::array<const slot*, 36> indexed{};
        for (const auto& slot : slots)
        {
            if (slot.inventory_index < 0 || slot.inventory_index >= 36 || slot.screen_id < 0) continue;
            if (indexed[slot.inventory_index]) return std::nullopt; // Ambiguous handler.
            for (const auto* existing : indexed)
                if (existing && existing->screen_id == slot.screen_id) return std::nullopt;
            indexed[slot.inventory_index] = &slot;
        }
        std::array<const slot*, 27> sources{};
        std::array<const slot*, 9> targets{};
        int source_count = 0, target_count = 0;
        for (int i = 0; i < 36; ++i)
        {
            const auto* slot = indexed[i];
            if (!slot) continue;
            if (i < 9 && slot->empty) targets[target_count++] = slot;
            if (i >= 9 && !slot->empty && slot->healing && slot->count > 0) sources[source_count++] = slot;
        }
        if (!source_count || !target_count) return std::nullopt;
        const auto pick = [&](int count) {
            if (order == 1) return count - 1;
            return order == 2 ? std::uniform_int_distribution<int>(0, count - 1)(random) : 0;
        };
        const auto& source = *sources[pick(source_count)];
        const auto& target = *targets[pick(target_count)];
        return move{source.inventory_index, source.screen_id, target.inventory_index, target.screen_id};
    }

    template<class Rng>
    std::chrono::milliseconds delay(const module_settings::pot_refill& settings, Rng& random)
    {
        const int low = std::clamp(settings.delay_ms, 50, 1000);
        const int high = settings.random_delay ? std::clamp(settings.max_delay_ms, low, 1000) : low;
        return std::chrono::milliseconds(std::uniform_int_distribution<int>(low, high)(random));
    }

    enum class observation { idle, waiting, completed, timed_out };
    class engine
    {
    public:
        void reset() { *this = {}; }
        void arm(clock::time_point now, std::chrono::milliseconds delay) { ready_at_ = now + delay; }
        bool ready(clock::time_point now) const { return !blocked_ && !pending_ && now >= ready_at_; }
        bool blocked() const { return blocked_; }
        const std::optional<move>& pending() const { return pending_; }
        bool submitted(move action, clock::time_point now)
        {
            if (!ready(now)) return false;
            pending_ = action;
            submitted_at_ = now;
            deadline_ = now + std::chrono::milliseconds(1500);
            return true;
        }
        observation observe(clock::time_point now, bool expected_slots_observed)
        {
            if (!pending_) return observation::idle;
            // Always require a subsequent frame, not just clickSlot's return.
            if (expected_slots_observed && now > submitted_at_ && now < deadline_)
            {
                pending_.reset();
                return observation::completed;
            }
            if (now >= deadline_)
            {
                stop();
                return observation::timed_out;
            }
            return observation::waiting;
        }
        void stop() { pending_.reset(); blocked_ = true; }
    private:
        std::optional<move> pending_;
        clock::time_point ready_at_{}, deadline_{}, submitted_at_{};
        bool blocked_ = false;
    };
}
