#pragma once

#include "setting_schema.hpp"
#include <mutex>
#include <utility>

namespace modules::settings
{
    template<class Config>
    class store
    {
    public:
        explicit store(Config initial = {}) : value_(std::move(initial)) {}
        store(const store&) = delete;
        store& operator=(const store&) = delete;

        Config read() const
        {
            std::lock_guard lock(mutex_);
            return value_;
        }

        // Edit a local candidate while serializing publishers. A rejection or
        // exception never changes live settings; readers only observe full copies.
        template<class Editor>
        bool update(Editor&& editor)
        {
            std::lock_guard lock(mutex_);
            Config candidate = value_;
            if (!std::forward<Editor>(editor)(candidate) || candidate == value_) return false;
            value_ = std::move(candidate);
            return true;
        }

        bool visit(config_visitor& visitor, std::span<const descriptor<Config>> schema)
        {
            std::lock_guard lock(mutex_);
            if constexpr (requires { Config::visit(visitor, value_); })
                return Config::visit(visitor, value_);
            else return settings::visit(visitor, value_, schema);
        }

    private:
        mutable std::mutex mutex_;
        Config value_;
    };
}
