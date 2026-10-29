#include "module_lifecycle.hpp"
#include "module_catalog.hpp"
#include "core/module_registry.hpp"
#include "../meta_jni.hpp"

void modules::init()
{
    jni::frame frame{};
	register_default_modules();
}

void modules::shutdown(::cache& cache)
{
    jni::frame frame{};
    clear_modules(cache);
}
