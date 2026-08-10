#include "cli_internal.hpp"

namespace wokiext {

Status Clean(Context& context, const PathOptions& options) {
    std::filesystem::path root = options.path.lexically_normal();
    while (root.filename() == ".")
        root = root.parent_path();
    if (root.filename().empty() && root != root.root_path())
        root = root.parent_path();

    std::error_code error;
    if (context.filesystem.IsSymlink(root, error) || error) {
        context.diagnostics.Err() << "Clean refuses a symbolic-link project root: " << root << '\n';
        return Status::Error;
    }
    if (!context.filesystem.IsRegularFile(root / "CMakeLists.txt") || !context.filesystem.IsRegularFile(root / "manifest.yaml")) {
        context.diagnostics.Err() << "Clean expects an extension project directory: " << root << '\n';
        return Status::Error;
    }

    context.filesystem.RemoveAll(root / "build", error);
    if (error) {
        context.diagnostics.Error("Failed to clean extension project: " + error.message());
        return Status::Error;
    }
    return Status::Ok;
}

} // namespace wokiext
