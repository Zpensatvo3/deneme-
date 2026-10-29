#pragma once
#include "../configs/configs.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string_view>
#include <unordered_set>

namespace gui::config_page::model
{
    enum class collection { community, mine, favourites };
    enum class order { newest, popular, name };

    inline std::string trim(std::string_view value)
    {
        const auto space = [](unsigned char c) { return std::isspace(c) != 0; };
        while (!value.empty() && space(static_cast<unsigned char>(value.front()))) value.remove_prefix(1);
        while (!value.empty() && space(static_cast<unsigned char>(value.back()))) value.remove_suffix(1);
        return std::string(value);
    }

    inline std::string fold(std::string_view value)
    {
        std::string result(value);
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
            return c < 128 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
        });
        return result;
    }

    inline std::vector<const configs::public_config*> filter(const configs::snapshot& library,
        collection tab, std::string_view query, const std::unordered_set<int>& favourites, order sort)
    {
        std::vector<const configs::public_config*> result;
        const auto needle = fold(trim(query));
        for (const auto& entry : library.entries)
        {
            if (tab == collection::community && entry.mine) continue;
            if (tab == collection::mine && !entry.mine) continue;
            if (tab == collection::favourites && !favourites.contains(entry.id)) continue;
            if (!needle.empty() && fold(entry.name + " " + entry.author_name + " " + entry.description).find(needle) == std::string::npos) continue;
            result.push_back(&entry);
        }
        std::sort(result.begin(), result.end(), [sort](const auto* a, const auto* b) {
            if (sort == order::popular && a->downloads != b->downloads) return a->downloads > b->downloads;
            if (sort == order::name) {
                const auto first = fold(a->name), second = fold(b->name);
                if (first != second) return first < second;
            }
            if (sort == order::newest) {
                const auto& first = a->updated_at.empty() ? a->created_at : a->updated_at;
                const auto& second = b->updated_at.empty() ? b->created_at : b->updated_at;
                if (first != second) return first > second;
            }
            return a->id > b->id;
        });
        return result;
    }

    template <std::size_t N>
    void copy_utf8(char (&target)[N], std::string_view source)
    {
        std::size_t size = std::min(source.size(), N - 1);
        // Never leave a partial UTF-8 character at the end of an edit buffer.
        if (size < source.size())
            while (size && (static_cast<unsigned char>(source[size]) & 0xc0) == 0x80) --size;
        std::memcpy(target, source.data(), size);
        target[size] = '\0';
    }

    inline std::string date(std::string_view value)
    {
        return value.empty() ? "--" : std::string(value.substr(0, std::min<std::size_t>(10, value.size())));
    }
}
