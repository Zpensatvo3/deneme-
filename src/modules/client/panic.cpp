#include "panic.hpp"
#include "../../cache/cache.hpp"

#include "../../gui/gui.hpp"

void modules::panic::run(::cache&)
{
    enabled = false;
    gui::request_panic();
}
