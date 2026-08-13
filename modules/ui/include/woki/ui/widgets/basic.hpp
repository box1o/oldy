#pragma once

#include "button.hpp"

namespace woki::ui {

View Panel(View content, const Theme& theme = Theme::Default());
View Badge(std::string label, Tone tone = Tone::Secondary, const Theme& theme = Theme::Default());
View Alert(
    std::string title,
    std::string description,
    Tone tone = Tone::Secondary,
    const Theme& theme = Theme::Default()
);
View Checkbox(std::string label, bool checked, std::function<void(bool)> change, const Theme& theme = Theme::Default());
View Switch(std::string label, bool checked, std::function<void(bool)> change, const Theme& theme = Theme::Default());
View Progress(f32 value, const Theme& theme = Theme::Default());

} // namespace woki::ui
