#pragma once

#include "module.hpp"
#include <initializer_list>
#include <vector>

namespace modules
{
    bool register_module(module* entry);
    void register_modules(std::initializer_list<module*> modules);
    void clear_modules(::cache& cache);
    const std::vector<module*>& get_modules();
    module* find_module_by_id(const std::string& id);
    int enabled_module_count();
}
