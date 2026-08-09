#include <iostream>
#include <filesystem>

#include <woki/ext/ext.hpp>

#include "wokiext/cli.hpp"

namespace wokiext {

namespace {

namespace fs = std::filesystem;

[[nodiscard]] bool IsSafeExtensionId(std::string_view id) {
    const fs::path path{id};
    return !id.empty() && !path.has_root_path() && !path.has_parent_path() && path != "." && path != "..";
}

[[nodiscard]] bool RemovePath(const fs::path& path) {
    std::error_code error;
    fs::remove_all(path, error);
    if (error) {
        std::cerr << error.message() << ": " << path << '\n';
        return false;
    }
    return true;
}

} // namespace

Status List(const ListOptions& options) {
    auto roots = woki::ext::RootsFromBase(options.root);
    if (!roots) {
        std::cerr << roots.error().Message() << '\n';
        return Status::Error;
    }

    woki::ext::Registry registry;
    registry.SetRoots(*roots);
    auto scanned = registry.Scan();
    if (!scanned) {
        std::cerr << scanned.error().Message() << '\n';
        return Status::Error;
    }

    for (const woki::ext::Record& record : registry.Records()) {
        std::cout << record.id << " " << record.manifest.version << " ";
        if (record.state == woki::ext::State::Failed) {
            std::cout << "failed " << record.error;
        } else {
            std::cout << "ok";
        }
        std::cout << '\n';
    }

    return Status::Ok;
}

Status Remove(const RemoveOptions& options) {
    if (!IsSafeExtensionId(options.id)) {
        std::cerr << "Extension id must be a single relative path component\n";
        return Status::Usage;
    }

    auto roots = woki::ext::RootsFromBase(options.root);
    if (!roots) {
        std::cerr << roots.error().Message() << '\n';
        return Status::Error;
    }

    bool ok = RemovePath(roots->extensions / options.id);
    if (!options.keep_data) {
        ok = RemovePath(roots->data / options.id) && ok;
        ok = RemovePath(roots->cache / options.id) && ok;
    }
    if (!ok) {
        return Status::Error;
    }

    std::cout << "Removed extension: " << options.id << '\n';
    return Status::Ok;
}

} // namespace wokiext
