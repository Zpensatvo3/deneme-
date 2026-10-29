#pragma once

#include "config_fields.hpp"
#include <algorithm>
#include <cmath>
#include <span>
#include <vector>
#include <string>
#include <utility>
#include <type_traits>

namespace modules::settings
{
    struct label
    {
        const char* english;
    };
    enum class control { toggle, number, range, choice, integer, color, real_pair, integer_pair, string_list };

    // Data only: config keys, member bindings, limits and display metadata.
    template<class Config>
    struct descriptor
    {
        control type{};
        const char* key = nullptr;
        label title{};
        bool Config::* boolean = nullptr;
        float Config::* scalar = nullptr;
        float Config::* upper = nullptr;
        int Config::* selection = nullptr;
        int Config::* integer_value = nullptr;
        int Config::* minimum_from = nullptr;
        int integer_minimum = 0, integer_maximum = 0, integer_display_minimum = 0, integer_display_maximum = 0;
        const char* upper_key = nullptr;
        float minimum = 0, maximum = 0;
        float number_display_minimum = 0, number_display_maximum = 0;
        const char* format = "%.1f";
        std::span<const label> choices{};
        bool Config::* visible_if = nullptr;
        bool visible = true;
        float (Config::* float_array)[4] = nullptr;
        float (Config::* float_pair)[2] = nullptr;
        int (Config::* int_pair)[2] = nullptr;
        std::vector<std::string> Config::* strings = nullptr;
        std::span<const int> choice_values{};
        int (*enum_read)(const Config&) = nullptr;
        void (*enum_write)(Config&, int) = nullptr;
        bool (*condition)(const Config&) = nullptr;
        float display_scale = 1.f;

        descriptor when(bool (*predicate)(const Config&)) const { auto d = *this; d.condition = predicate; return d; }
        descriptor hidden() const { auto d = *this; d.visible = false; return d; }
        descriptor scaled(float factor) const { auto d = *this; d.display_scale = factor; return d; }
        descriptor display_limits(float low, float high) const
        {
            auto d = *this; d.number_display_minimum = low; d.number_display_maximum = high; return d;
        }
        static descriptor color(const char* key, label title, float (Config::* member)[4])
        {
            descriptor d; d.type = control::color; d.key = key; d.title = title; d.float_array = member; return d;
        }
        static descriptor real_pair(const char* key, label title, float (Config::* member)[2],
            float minimum, float maximum, const char* format = "%.2f")
        {
            auto d = number(key, title, nullptr, minimum, maximum, format);
            d.type = control::real_pair; d.float_pair = member; return d;
        }
        static descriptor integer_pair(const char* key, label title, int (Config::* member)[2],
            int minimum, int maximum, const char* format = "%d")
        {
            auto d = integer(key, title, nullptr, minimum, maximum, format);
            d.type = control::integer_pair; d.int_pair = member; return d;
        }
        static descriptor string_list(const char* key, label title, std::vector<std::string> Config::* member)
        {
            descriptor d; d.type = control::string_list; d.key = key; d.title = title;
            d.strings = member; d.visible = false; return d;
        }
        template<auto Member>
        static descriptor enumeration(const char* key, label title, std::span<const label> values,
            std::span<const int> stored_values = {})
        {
            auto d = choice(key, title, nullptr, values);
            d.choice_values = stored_values;
            d.enum_read = [](const Config& c) { return static_cast<int>(c.*Member); };
            d.enum_write = [](Config& c, int value) {
                using type = std::remove_cvref_t<decltype(c.*Member)>;
                c.*Member = static_cast<type>(value);
            };
            return d;
        }
        int selected(const Config& c) const { return enum_read ? enum_read(c) : c.*selection; }
        void select(Config& c, int value) const { if (enum_write) enum_write(c, value); else c.*selection = value; }

        static descriptor toggle(const char* key, label title, bool Config::* member, bool visible = true)
        {
            descriptor d; d.type = control::toggle; d.key = key; d.title = title;
            d.boolean = member; d.visible = visible; return d;
        }
        static descriptor integer(const char* key, label title, int Config::* member,
            int minimum, int maximum, const char* format = "%d", int Config::* minimum_from = nullptr)
        {
            descriptor d; d.type = control::integer; d.key = key; d.title = title;
            d.integer_value = member; d.integer_minimum = d.integer_display_minimum = minimum;
            d.integer_maximum = d.integer_display_maximum = maximum;
            d.minimum_from = minimum_from; d.format = format; return d;
        }
        descriptor display_maximum(int value) const
        {
            auto d = *this; d.integer_display_maximum = value; return d;
        }
        descriptor display_minimum(int value) const
        {
            auto d = *this; d.integer_display_minimum = value; return d;
        }
        static descriptor number(const char* key, label title, float Config::* member,
            float minimum, float maximum, const char* format = "%.1f", bool Config::* visible_if = nullptr)
        {
            descriptor d; d.type = control::number; d.key = key; d.title = title;
            d.scalar = member; d.minimum = d.number_display_minimum = minimum; d.maximum = d.number_display_maximum = maximum;
            d.format = format; d.visible_if = visible_if; return d;
        }
        static descriptor range(const char* low_key, const char* high_key, label title,
            float Config::* low, float Config::* high, float minimum, float maximum)
        {
            auto d = number(low_key, title, low, minimum, maximum);
            d.type = control::range; d.upper = high; d.upper_key = high_key; return d;
        }
        static descriptor choice(const char* key, label title, int Config::* member,
            std::span<const label> values)
        {
            descriptor d; d.type = control::choice; d.key = key; d.title = title;
            d.selection = member; d.choices = values; return d;
        }
    };

    template<class Config>
    bool normalize(Config& candidate, std::span<const descriptor<Config>> schema)
    {
        for (const auto& d : schema)
        {
            if (d.type == control::color || d.type == control::real_pair)
            {
                auto* values = d.type == control::color ? candidate.*d.float_array : candidate.*d.float_pair;
                const int count = d.type == control::color ? 4 : 2;
                for (int i = 0; i < count; ++i) {
                    if (!std::isfinite(values[i])) return false;
                    values[i] = std::clamp(values[i], d.type == control::color ? 0.f : d.minimum,
                        d.type == control::color ? 1.f : d.maximum);
                }
                if (count == 2 && values[0] > values[1]) std::swap(values[0], values[1]);
            }
            else if (d.type == control::integer_pair)
            {
                auto& values = candidate.*d.int_pair;
                for (auto& value : values) value = std::clamp(value, d.integer_minimum, d.integer_maximum);
                if (values[0] > values[1]) std::swap(values[0], values[1]);
            }
            else if (d.type == control::integer)
            {
                const int minimum = d.minimum_from
                    ? std::clamp(candidate.*d.minimum_from, d.integer_minimum, d.integer_maximum)
                    : d.integer_minimum;
                candidate.*d.integer_value = std::clamp(candidate.*d.integer_value, minimum, d.integer_maximum);
            }
            else if (d.type == control::choice)
            {
                const int value = d.selected(candidate);
                if (d.choice_values.empty()) {
                    if (value < 0 || static_cast<std::size_t>(value) >= d.choices.size()) return false;
                } else if (std::find(d.choice_values.begin(), d.choice_values.end(), value) == d.choice_values.end()) return false;
            }
            else if (d.type == control::number || d.type == control::range)
            {
                auto& value = candidate.*d.scalar;
                if (!std::isfinite(value)) return false;
                value = std::clamp(value, d.minimum, d.maximum);
                if (d.type == control::range)
                {
                    auto& upper = candidate.*d.upper;
                    if (!std::isfinite(upper)) return false;
                    upper = std::clamp(upper, d.minimum, d.maximum);
                    if (value > upper) std::swap(value, upper);
                }
            }
        }
        return true;
    }

    template<class Config>
    void visit_fields(config_visitor& visitor, Config& candidate, std::span<const descriptor<Config>> schema)
    {
        for (const auto& d : schema)
        {
            switch (d.type)
            {
            case control::color: visitor.field(d.key, candidate.*d.float_array); break;
            case control::real_pair: visitor.field(d.key, candidate.*d.float_pair); break;
            case control::integer_pair: visitor.field(d.key, candidate.*d.int_pair); break;
            case control::string_list: visitor.field(d.key, candidate.*d.strings); break;
            case control::integer: visitor.field(d.key, candidate.*d.integer_value); break;
            case control::toggle: visitor.field(d.key, candidate.*d.boolean); break;
            case control::number: visitor.field(d.key, candidate.*d.scalar); break;
            case control::range:
                visitor.field(d.key, candidate.*d.scalar);
                visitor.field(d.upper_key, candidate.*d.upper);
                break;
            case control::choice:
            {
                int value = d.selected(candidate);
                visitor.enum_value(d.key, value);
                if (visitor.importing()) d.select(candidate, value);
                break;
            }
            }
        }
    }

    template<class Config>
    bool visit(config_visitor& visitor, Config& live, std::span<const descriptor<Config>> schema,
        const char* error = "Invalid module settings.")
    {
        Config candidate = live;
        const bool importing = visitor.importing();
        if (importing) visitor.stage_values(true);
        visit_fields(visitor, candidate, schema);
        if (!importing) return false;
        if (!normalize(candidate, schema)) visitor.reject(error);
        visitor.stage_values(false);
        if (!visitor.reading() || !visitor.valid()) return false;
        live = candidate;
        return true;
    }
}
