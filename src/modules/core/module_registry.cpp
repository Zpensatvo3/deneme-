#include "module_registry.hpp"
#include "module_runtime_state.hpp"
#include "../../cache/cache.hpp"
#include <algorithm>

namespace
{
    std::vector<modules::module*> registered_modules;
}

bool modules::register_module(module* entry)
{
    if (!entry)
        return false;

    const auto duplicate = std::find_if(registered_modules.begin(), registered_modules.end(), [entry](module* current)
    {
        return current && std::string_view(current->get_id()) == entry->get_id();
    });
    if (duplicate != registered_modules.end())
    {
        delete entry;
        return false;
    }

    registered_modules.push_back(entry);
    return true;
}

void modules::register_modules(std::initializer_list<module*> entries)
{
    for (module* entry : entries)
        register_module(entry);
}

void modules::clear_modules(::cache& cache)
{
    for (module* current : registered_modules)
    {
        if (current && (current->enabled || current->prev_enabled) && cache.is_valid())
            stop_module(*current, cache);
        delete current;
    }
    registered_modules.clear();
}

const std::vector<modules::module*>& modules::get_modules() { return registered_modules; }

modules::module* modules::find_module_by_id(const std::string& id)
{
    const auto found = std::find_if(registered_modules.begin(), registered_modules.end(), [&id](module* current)
    {
        return current && id == current->get_id();
    });
    return found == registered_modules.end() ? nullptr : *found;
}

int modules::enabled_module_count()
{
    return static_cast<int>(std::count_if(registered_modules.begin(), registered_modules.end(), [](module* current)
    {
        return current && current->enabled;
    }));
}
