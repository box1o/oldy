#pragma once

// Host implementation detail. This header is not installed.

#include <filesystem>

namespace woki::ext {

[[nodiscard]] bool HasPathTraversal(const std::filesystem::path& path);
[[nodiscard]] bool IsSafeRelativePath(const std::filesystem::path& path);

} // namespace woki::ext
