#include <iostream>
#include <filesystem>

#include <woki/ext/ext.hpp>

#include "wokiext/cli.hpp"

namespace wokiext {

namespace {

namespace fs = std::filesystem;

[[nodiscard]] woki::Result<woki::ext::PackageLayout> SourceLayout(const fs::path& root) {
    auto manifest = woki::ext::LoadManifest(root / "manifest.yaml");
    if (!manifest) {
        return woki::Err(manifest.error());
    }

    auto layout = woki::ext::ResolvePackageLayout(*manifest, root.parent_path(), root / ".data", root / ".cache");
    if (!layout) {
        return woki::Err(layout.error());
    }

    layout->install_root = root;
    layout->manifest = root / "manifest.yaml";
    layout->wasm = (root / manifest->wasm_path).lexically_normal();
    return layout;
}

} // namespace

Status Verify(const PathOptions& options) {
    const fs::path root = fs::absolute(options.path).lexically_normal();
    if (!fs::is_directory(root)) {
        std::cerr << "Verify expects an unpacked extension directory: " << root << '\n';
        return Status::Error;
    }

    auto manifest = woki::ext::LoadManifest(root / "manifest.yaml");
    if (!manifest) {
        std::cerr << manifest.error().Message() << '\n';
        return Status::Error;
    }

    auto layout = SourceLayout(root);
    if (!layout) {
        std::cerr << layout.error().Message() << '\n';
        return Status::Error;
    }

    auto valid = woki::ext::ValidatePackageLayout(*layout);
    if (!valid) {
        std::cerr << valid.error().Message() << '\n';
        return Status::Error;
    }

    auto guest = woki::ext::wasm::ValidateGuestModule(layout->wasm, *manifest);
    if (!guest) {
        std::cerr << guest.error().Message() << '\n';
        return Status::Error;
    }

    std::cout << "Verified extension: " << root << '\n';
    return Status::Ok;
}

} // namespace wokiext
