#pragma once
#include "config_json.hpp"
#include <map>

namespace modules::config_migration
{
    using fields = std::map<std::string, std::string>;
    inline std::string encode(const fields& values)
    {
        std::string result = "{";
        for (const auto& [key, value] : values)
        {
            if (result.size() > 1) result += ',';
            result += '"' + config_json::escape(key) + "\":" + value;
        }
        return result + '}';
    }
}
