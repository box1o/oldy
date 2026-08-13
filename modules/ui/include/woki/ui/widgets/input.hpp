#pragma once

#include "../theme/theme.hpp"
#include "../text/edit.hpp"
#include "../view.hpp"

namespace woki::ui {

View Input(
    Edit& edit,
    std::string placeholder,
    std::function<void(std::string_view)> change = {},
    const Theme& theme = Theme::Default()
);

} // namespace woki::ui
