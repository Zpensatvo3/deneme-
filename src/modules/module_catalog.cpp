#define ONESTEP_UI_NO_REDIRECT
#include "modules.hpp"
#undef ONESTEP_UI_NO_REDIRECT
#include "module_catalog.hpp"
#include <stdexcept>

namespace
{
    using module_factory = modules::module* (*)();

    template <typename Module>
    modules::module* create_module()
    {
        return new Module();
    }


    struct descriptor
    {
        const char* stable_id;
        module_factory create;
    };

    const descriptor default_modules[] = {
#include <module_manager_catalog.inc>
    };
}

void modules::register_default_modules()
{
    if (manager::read()->modules.size() != std::size(default_modules))
        throw std::runtime_error("Java/native module catalog size mismatch");
    for (const descriptor& entry : default_modules)
    {
        module* instance = entry.create();
        if (!instance || std::string_view(instance->get_id()) != entry.stable_id)
        {
            delete instance;
            throw std::runtime_error("Java/native module factory ID mismatch");
        }
        if (!register_module(instance)) throw std::runtime_error("Duplicate native module factory");
    }
}
