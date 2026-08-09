#pragma once

#include <woki/core.hpp>
#include <woki/platform.hpp>

namespace woki {

struct Context {
    Window& window;
    bool& running;
};

} // namespace woki
