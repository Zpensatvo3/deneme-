#pragma once
#include <array>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace platform::jni_detail
{
    // JNI IDs belong to a declaring class. A name/signature alone is not a key:
    // unrelated constructors and record/enum name() accessors otherwise collide.
    // Owners are the SDK's cached global class references, valid for this JNI session.
    template<class Owner, class Id, class Binding>
    class member_id_cache
    {
        struct entry { Owner owner{}; Id id{}; };
        inline static std::shared_mutex mutex_;
        inline static std::unordered_map<Owner, Id> values_;
        // Avoid a shared lock or JNI lookup for the common owner(s) on each thread.
        inline static thread_local std::array<entry, 2> recent_{};
    public:
        template<class Resolve> static Id get(Owner owner, Resolve&& resolve)
        {
            if (owner == Owner{}) return {};
            if (recent_[0].owner == owner) return recent_[0].id;
            if (recent_[1].owner == owner)
            {
                const auto found = recent_[1]; recent_[1] = recent_[0]; recent_[0] = found;
                return found.id;
            }
            Id value{};
            bool found = false;
            {
                std::shared_lock lock(mutex_);
                const auto item = values_.find(owner);
                if (item != values_.end()) { value = item->second; found = true; }
            }
            if (!found)
            {
                // Resolving a JNI member can initialize Java classes. Never hold
                // a native cache lock across that reentrant Java operation.
                value = resolve();
                std::unique_lock lock(mutex_);
                value = values_.emplace(owner, value).first->second;
            }
            recent_[1] = recent_[0]; recent_[0] = {owner, value};
            return value;
        }
    };
}
