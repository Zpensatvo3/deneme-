#pragma once
#include <cstdint>
#include <functional>

namespace services::combat
{
    void claim_trigger(int target_id, std::uint64_t session);
    void release_trigger();
    bool trigger_owns_attack();
    bool try_unowned_attack(const std::function<bool()>& attempt);
}
