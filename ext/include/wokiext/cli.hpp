#pragma once

#include <span>

namespace wokiext {

[[nodiscard]] int Run(std::span<const char* const> args);

} // namespace wokiext
