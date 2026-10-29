#pragma once

#include "../modules/core/module_runtime_state.hpp"
#include <unordered_map>

class cache;
namespace modules { class module; }

namespace app
{
    class module_loop
    {
    public:
        void tick(::cache& game);
    private:
        std::unordered_map<modules::module*, modules::key_edge> keys_;
    };
}
