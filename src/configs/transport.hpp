#pragma once
#include <string>

namespace configs::transport
{
    struct service_result {
        bool success = false;
        int status_code = 0;
        std::string body;
        std::string message;
    };
    service_result post_json(const std::string& path, const std::string& fields);
}
