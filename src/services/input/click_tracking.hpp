#pragma once
#include <cstdint>

namespace services::input
{
    enum class submission_status { rejected, partial, complete };
    enum class event_path { window, raw };
    enum class input_source { legacy, triggerbot };
    struct click_ticket { std::uint32_t id = 0; submission_status submission = submission_status::rejected; input_source source = input_source::legacy; };
    inline submission_status classify_submission(unsigned sent)
    {
        return sent == 2 ? submission_status::complete : sent == 1 ? submission_status::partial : submission_status::rejected;
    }
    struct click_receipt { bool delivered = false, cancelled = false; };
    // 32-bit raw-input extra information must retain the complete identity.
    // No sequence wrap: exhausted ids fail closed instead of accepting old events.
    inline constexpr std::uint32_t tracked_prefix = 0xd7000000u;
    inline constexpr std::uint32_t tracked_mask = 0xff000000u;
    inline constexpr std::uint32_t restore_prefix = 0xd6000000u;
    inline bool restore_tag(std::uintptr_t tag) { return (static_cast<std::uint32_t>(tag) & tracked_mask) == restore_prefix; }
    inline std::uintptr_t restore_ticket_tag(std::uint32_t id, bool pressed) { return restore_prefix | (id << 1) | (pressed ? 1u : 0u); }
    inline bool tracked_tag(std::uintptr_t tag) { return (static_cast<std::uint32_t>(tag) & tracked_mask) == tracked_prefix; }
    inline std::uintptr_t ticket_tag(std::uint32_t id, bool pressed) { return tracked_prefix | (id << 1) | (pressed ? 1u : 0u); }
    struct click_ledger
    {
        std::uint32_t next = 0, active = 0;
        bool cancelled = true, preserve_hold = false;
        unsigned window_edges = 0, raw_edges = 0;
        std::uint32_t reserve(bool preserve)
        {
            if ((!cancelled && active) || next == 0x7fffffu) return 0;
            active = ++next; cancelled = false; preserve_hold = preserve;
            window_edges = raw_edges = 0; return active;
        }
        void cancel(std::uint32_t id) { if (id && id == active) cancelled = true; }
        click_receipt receipt(std::uint32_t id) const
        {
            if (!id || id != active) return { false, true };
            return { window_edges == 3 || raw_edges == 3, cancelled };
        }
        bool needs_repair() const
        {
            const auto edges = window_edges | raw_edges;
            return preserve_hold ? ((edges & 2u) && !(edges & 1u)) : ((edges & 1u) && !(edges & 2u));
        }
        bool accepts_restore(std::uintptr_t tag) const
        {
            const auto id = (static_cast<std::uint32_t>(tag) & ~tracked_mask) >> 1;
            return restore_tag(tag) && id && id == active && cancelled;
        }
        bool needs_validation(std::uintptr_t tag, bool pressed, event_path path) const
        {
            const auto encoded = static_cast<std::uint32_t>(tag);
            const auto id = (encoded & ~tracked_mask) >> 1;
            if (!tracked_tag(tag) || !id || id != active || cancelled ||
                (encoded & 1u) != static_cast<unsigned>(pressed)) return false;
            if (window_edges == 3 || raw_edges == 3) return false;
            const auto edges = path == event_path::window ? window_edges : raw_edges;
            return edges == 0 || (pressed && !(edges & 1u));
        }
        bool observe(std::uintptr_t tag, bool pressed, event_path path, bool physical_left, bool valid_delivery = true)
        {
            const auto encoded = static_cast<std::uint32_t>(tag);
            const auto id = (encoded & ~tracked_mask) >> 1;
            if (!tracked_tag(tag)) return true;
            if ((encoded & 1u) != static_cast<unsigned>(pressed)) return false;
            if (!id || id != active) return false; // Never let an old UP release a newer ticket.
            if (!cancelled && !valid_delivery) { cancel(id); return false; }
            if (cancelled)
                return !pressed && !physical_left; // A stale UP may clean a released button, never a physical hold.
            auto& edges = path == event_path::window ? window_edges : raw_edges;
            const unsigned bit = pressed ? 1u : 2u;
            // A duplicate from another path is allowed through its own transport,
            // but cannot be counted as another delivered click.
            if (edges & bit) return false;
            edges |= bit; return true;
        }
    };
}
