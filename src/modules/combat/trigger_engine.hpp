#pragma once
#include <cstdint>

// Pure model: no JNI, Windows, ImGui, random_device or wall-clock dependencies.
namespace trigger_engine
{
    enum class target_mode { players, players_and_mobs, all_living };
    enum class weapon_mode { any, sword, axe };
    enum class critical_mode { off, smart, only };
    enum class weapon_type { empty, sword, axe, mace, other };
    struct config
    {
        float cooldown[2]{ .92f, .98f };
        int reaction[2]{ 25, 65 };
        target_mode targets = target_mode::players;
        weapon_mode weapon = weapon_mode::any;
        critical_mode criticals = critical_mode::smart;
        bool on_click = false;
        bool swing_hand = true;
        bool click_simulation = false;
    };
    bool normalize(config& value);
    bool same_config(const config& a, const config& b);
    bool weapon_allowed(weapon_type held, weapon_mode mode);
    // Legacy pure model retained for existing regression fixtures only.
    // The live Triggerbot controller and timers are in WorldBridge.java.
    struct sampler
    {
        virtual ~sampler() = default;
        virtual float cooldown(float minimum, float maximum) = 0;
        virtual int reaction(int minimum, int maximum) = 0;
    };
    enum class phase { idle, tracking, reaction, cooldown, critical, preparing, confirmation, backoff };
    enum class action { none, prepare, cancel, retire };
    struct observation
    {
        bool eligible = false, physical_left = false, weapon_permitted = false;
        bool on_ground = true, critical_valid = false, critical_possible = false;
        bool input_delivered = false, input_cancelled = false;
        int target_id = -1, attack_ticks = -1;
        float cooldown = 0;
        std::uint64_t session = 0, settings_revision = 0, weapon_revision = 0, manual_generation = 0;
    };
    struct runtime
    {
        phase status = phase::idle;
        int target_id = -1, pending_attack_ticks = -1, last_attack_ticks = -1;
        float threshold = .95f, last_cooldown = 0;
        std::uint64_t session = 0, settings_revision = 0, weapon_revision = 0, manual_generation = 0, ticket = 0;
        std::int64_t reaction_ready = 0, critical_started = -1, confirmation_deadline = 0, backoff_until = 0;
    };
    void reset(runtime& state);
    // Invalidation cannot retract an already queued vanilla click. Keep its
    // confirmation window and retry backoff even across target reacquisition.
    void invalidate(runtime& state, std::int64_t now);
    void begin_cycle(const config& settings, runtime& state, std::int64_t now, sampler& random);
    action step(const config& settings, runtime& state, const observation& frame, std::int64_t now, sampler& random);
    void submitted(runtime& state, std::uint64_t ticket, const observation& frame, std::int64_t now);
    void submission_failed(runtime& state, std::int64_t now);
}
