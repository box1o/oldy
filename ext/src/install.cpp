#include <chrono>
#include <iostream>
#include <filesystem>

#include <woki/ext/ext.hpp>

#include "wokiext/cli.hpp"

namespace wokiext {

namespace {

namespace fs = std::filesystem;

[[nodiscard]] woki::Result<std::string> InspectArchiveId(const fs::path& archive_path) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("wokiext-inspect-" + std::to_string(stamp));

    auto roots = woki::ext::RootsFromBase(root);
    if (!roots) {
        return woki::Err(roots.error());
    }

    auto installed = woki::ext::InstallArchive(archive_path, *roots);
    std::error_code cleanup_error;
    fs::remove_all(root, cleanup_error);
    if (!installed) {
        return woki::Err(installed.error());
    }

    return woki::Ok(installed->install_root.filename().string());
}

} // namespace

Status Install(const InstallOptions& options) {
    auto roots = woki::ext::RootsFromBase(options.root);
    if (!roots) {
        std::cerr << roots.error().Message() << '\n';
        return Status::Error;
    }

    const fs::path path = fs::absolute(options.path).lexically_normal();
    fs::path install_path = path;
    fs::path temp_archive;

    if (fs::is_directory(path)) {
        if (options.force) {
            auto manifest = woki::ext::LoadManifest(path / "manifest.yaml");
            if (!manifest) {
                std::cerr << manifest.error().Message() << '\n';
                return Status::Error;
            }
            if (fs::exists(roots->extensions / manifest->id)) {
                (void)Remove(RemoveOptions{.id = manifest->id, .root = options.root});
            }
        }

        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        temp_archive = fs::temp_directory_path() / ("wokiext-install-" + std::to_string(stamp) + ".wokiext");
        if (Bundle(BundleOptions{.path = path, .out_file = temp_archive}) != Status::Ok) {
            return Status::Error;
        }
        install_path = temp_archive;
    } else if (options.force) {
        auto id = InspectArchiveId(path);
        if (!id) {
            std::cerr << id.error().Message() << '\n';
            return Status::Error;
        }
        if (fs::exists(roots->extensions / *id)) {
            (void)Remove(RemoveOptions{.id = *id, .root = options.root});
        }
    }

    woki::Result<woki::ext::PackageLayout> installed = woki::ext::InstallArchive(install_path, *roots);

    if (!temp_archive.empty()) {
        std::error_code error;
        fs::remove(temp_archive, error);
    }

    if (!installed) {
        std::cerr << installed.error().Message() << '\n';
        return Status::Error;
    }

    std::cout << "Installed extension: " << installed->install_root << '\n';
    return Status::Ok;
}

} // namespace wokiext
