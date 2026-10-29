#include "client_dispatcher.hpp"

#include "jni_context.hpp"

#include <deque>
#include <mutex>

namespace
{
    std::mutex queue_mutex;
    std::deque<platform::client_dispatcher::task> queued_tasks;
}

void platform::client_dispatcher::post(task work)
{
    if (!work)
        return;
    std::lock_guard lock(queue_mutex);
    queued_tasks.push_back(std::move(work));
}

std::size_t platform::client_dispatcher::drain(std::size_t maximum_tasks)
{
    if (!maximum_tasks || !jni_context::bind_current_thread())
        return 0;

    std::size_t completed = 0;
    while (completed < maximum_tasks)
    {
        task work;
        {
            std::lock_guard lock(queue_mutex);
            if (queued_tasks.empty())
                break;
            work = std::move(queued_tasks.front());
            queued_tasks.pop_front();
        }

        try { work(); }
        catch (...) { jni_context::clear_exception(); }
        ++completed;
    }
    return completed;
}

void platform::client_dispatcher::clear()
{
    std::lock_guard lock(queue_mutex);
    queued_tasks.clear();
}
