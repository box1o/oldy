#pragma once

#include "basic.hpp"

namespace woki::ui {

View Dialog(View content, bool open = true, const Theme& theme = Theme::Default());
View Sheet(
    View content,
    bool open = true,
    bool right = true,
    f32 width = 360.0f,
    const Theme& theme = Theme::Default()
);

} // namespace woki::ui
