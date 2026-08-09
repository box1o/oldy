#include <algorithm>

#include "woki/ext/path_safety.hpp"

namespace woki::ext {

bool HasPathTraversal(const std::filesystem::path& path) {
    return std::ranges::any_of(path, [](const std::filesystem::path& part) { return part == ".."; });
}

bool IsSafeRelativePath(const std::filesystem::path& path) {
    if (path.empty() || path.has_root_path() || HasPathTraversal(path)) {
        return false;
    }

    const std::string native = path.generic_string();
    if (native.contains('\0') || native.contains('\\') || native.contains(':')) {
        return false;
    }
    return std::ranges::none_of(path, [](const std::filesystem::path& part) { return part.empty() || part == "."; });
}

} // namespace woki::ext
