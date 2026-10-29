#pragma once

#include <cstddef>
#include <functional>

namespace platform::client_dispatcher
{
    using task = std::function<void()>;

    void post(task work);
    std::size_t drain(std::size_t maximum_tasks = 128);
    void clear();
}
