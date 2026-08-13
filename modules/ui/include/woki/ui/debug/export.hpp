#pragma once

#include <string>

#include "../tree.hpp"

namespace woki::ui {

[[nodiscard]] std::string ExportTree(const Element& root);

} // namespace woki::ui
