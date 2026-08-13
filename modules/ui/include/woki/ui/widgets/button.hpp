#pragma once

#include <string>
#include <functional>

#include "../view.hpp"
#include "../theme/theme.hpp"

namespace woki::ui {

enum class Tone : u8 { Primary, Secondary, Destructive, Outline, Ghost };
enum class ControlSize : u8 { Small, Medium, Large };

View Button(
    std::string label,
    std::function<void()> action,
    Tone tone = Tone::Primary,
    ControlSize size = ControlSize::Medium,
    const Theme& theme = Theme::Default()
);

} // namespace woki::ui
