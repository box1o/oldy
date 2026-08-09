#include "woki/ext/package.hpp"
#include "woki/ext/path_safety.hpp"

namespace woki::ext {

namespace {

[[nodiscard]] bool StartsWithPath(const std::filesystem::path& path, const std::filesystem::path& prefix) {
    auto path_it = path.begin();
    auto prefix_it = prefix.begin();
    for (; prefix_it != prefix.end(); ++prefix_it, ++path_it) {
        if (path_it == path.end() || *path_it != *prefix_it) {
            return false;
        }
    }
    return true;
}

} // namespace

bool IsAllowedArchiveEntry(const std::filesystem::path& relative_path, const std::filesystem::path& wasm_path) {
    if (relative_path.empty() || relative_path.is_absolute() || HasPathTraversal(relative_path)) {
        return false;
    }
    if (relative_path == "manifest.yaml" || relative_path == wasm_path || relative_path == "signature") {
        return true;
    }
    return StartsWithPath(relative_path, "assets") || StartsWithPath(relative_path, "extension.native");
}

} // namespace woki::ext
