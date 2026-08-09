#include <algorithm>

#include "woki/ext/path_safety.hpp"

namespace woki::ext {

bool HasPathTraversal(const std::filesystem::path& path) {
    return std::ranges::any_of(path, [](const std::filesystem::path& part) { return part == ".."; });
}

bool IsSafeRelativePath(const std::filesystem::path& path) {
    return !path.empty() && !path.has_root_path() && !HasPathTraversal(path);
}

} // namespace woki::ext
