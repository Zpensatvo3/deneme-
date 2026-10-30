#pragma once
#include "setting_schema.hpp"
#include "manager.hpp"
#include <mutex>
#include <stdexcept>
#include <utility>

namespace modules::settings
{
    // Typed read-only cache of Java-owned settings. Editors publish manager requests.
    template<class Config>
    class store
    {
    public:
        explicit store(const char* id, std::span<const descriptor<Config>> schema = Config::schema())
            : id_(id), schema_(schema) {}
        store(const store&) = delete;
        store& operator=(const store&) = delete;
        Config read() const { return read(manager::read()); }
        template<class Editor> bool update(Editor&& editor)
        {
            const auto snapshot = manager::read(); Config candidate = read(snapshot);
            const auto before = serialize_config_fields([&](config_visitor& visitor) { visit_fields(visitor, candidate, schema_); });
            if (!std::forward<Editor>(editor)(candidate)) return false;
            const auto encoded = serialize_config_fields([&](config_visitor& visitor) { visit_fields(visitor, candidate, schema_); });
            if (before == encoded) return false;
            const auto result = manager::settings(id_, encoded, false, snapshot->revision);
            return result.success && result.revision != snapshot->revision;
        }
    private:
        Config read(const std::shared_ptr<const manager::snapshot>& snapshot) const
        {
            std::lock_guard lock(mutex_);
            if (revision_ != snapshot->revision) {
                const auto found = snapshot->modules.find(id_);
                if (found == snapshot->modules.end()) throw std::runtime_error("Missing module settings: " + id_);
                Config candidate{};
                if (!deserialize_config_fields([&](config_visitor& visitor) {
                    for (const auto& field : schema_)
                        if (!visitor.has_field(field.key) || (field.upper_key && !visitor.has_field(field.upper_key)))
                            visitor.reject("Incomplete Java settings snapshot");
                    visit_fields(visitor, candidate, schema_);
                }, found->second.settings))
                    throw std::runtime_error("Invalid module settings snapshot: " + id_);
                cached_ = std::move(candidate); revision_ = snapshot->revision;
            }
            return cached_;
        }
        const std::string id_;
        const std::span<const descriptor<Config>> schema_;
        mutable std::mutex mutex_;
        mutable Config cached_{};
        mutable std::uint64_t revision_ = ~std::uint64_t{0};
    };
}
