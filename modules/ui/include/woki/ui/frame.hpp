#pragma once

#include <chrono>

#include "geometry.hpp"

namespace woki::ui {

struct Frame {
    Size viewport{};
    f32 scale{1.0f};
    std::chrono::steady_clock::time_point time{std::chrono::steady_clock::now()};
};

} // namespace woki::ui
