#pragma once
#include <cstdint>
#include <optional>

namespace modules
{
    // Protected by the same mutex as its snapshot. Invalidating a session also
    // rejects work that started before the disconnect, even on same-world rejoin.
    template<class Identity>
    class snapshot_session
    {
    public:
        bool observe(const Identity& identity)
        {
            if (matches(identity)) return false;
            owner_ = identity;
            active_ = true;
            ++generation_;
            return true;
        }
        bool matches(const Identity& identity) const
        {
            return active_ && owner_ && *owner_ == identity;
        }
        std::uint64_t ticket() const { return generation_; }
        bool accepts(std::uint64_t ticket) const { return active_ && ticket == generation_; }
        void invalidate() { active_ = false; ++generation_; }
        void reset() { invalidate(); owner_.reset(); }
    private:
        std::optional<Identity> owner_;
        std::uint64_t generation_ = 0;
        bool active_ = false;
    };
}
