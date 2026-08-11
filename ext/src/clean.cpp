#include <cwctype>
#include <algorithm>

#include "cli_internal.hpp"

namespace wokiext {

namespace {

[[nodiscard]] bool SamePath(const std::filesystem::path& left, const std::filesystem::path& right) {
#ifdef _WIN32
    std::wstring first = left.native();
    std::wstring second = right.native();
    const auto normalize = [](wchar_t ch) { return ch == L'/' ? L'\\' : static_cast<wchar_t>(std::towlower(ch)); };
    std::ranges::transform(first, first.begin(), normalize);
    std::ranges::transform(second, second.begin(), normalize);
    return first == second;
#else
    return left == right;
#endif
}

} // namespace

Status Clean(Context& context, const PathOptions& options) {
    namespace fs = std::filesystem;
    fs::path root = fs::absolute(options.path).lexically_normal();
    while (root.filename() == ".")
        root = root.parent_path();
    if (root.filename().empty() && root != root.root_path())
        root = root.parent_path();

    std::error_code error;
    const fs::path canonical_root = fs::canonical(root, error);
    if (error || !SamePath(canonical_root, root)) {
        context.diagnostics.Err() << "Clean refuses a non-canonical or symbolic-link project path: " << root << '\n';
        return Status::Error;
    }
    if (!context.filesystem.IsRegularFile(root / "CMakeLists.txt") || !context.filesystem.IsRegularFile(root / "manifest.yaml")) {
        context.diagnostics.Err() << "Clean expects an extension project directory: " << root << '\n';
        return Status::Error;
    }

    const fs::path build = root / "build";
    const fs::file_status build_status = fs::symlink_status(build, error);
    if (error && error != std::errc::no_such_file_or_directory) {
        context.diagnostics.Error("Failed to inspect extension build directory: " + error.message());
        return Status::Error;
    }
    error.clear();
    if (fs::is_symlink(build_status)) {
        context.diagnostics.Err() << "Clean refuses a symbolic-link build directory: " << build << '\n';
        return Status::Error;
    }
    context.filesystem.RemoveAll(build, error);
    if (error) {
        context.diagnostics.Error("Failed to clean extension project: " + error.message());
        return Status::Error;
    }
    return Status::Ok;
}

} // namespace wokiext
