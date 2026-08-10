#include <filesystem>

#include <woki/ext/package.hpp>
#include <woki/ext/manifest.hpp>
#include <woki/ext/wasm/guest_module.hpp>

#include "cli_internal.hpp"

namespace wokiext {

namespace {

namespace fs = std::filesystem;

[[nodiscard]] woki::Result<woki::ext::PackageLayout> SourceLayout(const fs::path& root) {
    auto manifest = woki::ext::LoadManifest(root / "manifest.yaml");
    if (!manifest) {
        return woki::Err(manifest.error());
    }

    const fs::path state = root.parent_path() / ".woki-verify-state" / manifest->id;
    return woki::Ok(woki::ext::PackageLayout{
        .install_root = root,
        .manifest = root / "manifest.yaml",
        .wasm = (root / manifest->wasm_path).lexically_normal(),
        .data_root = state / "data",
        .config_root = state / "config",
        .cache_root = state / "cache",
    });
}

} // namespace

Status Verify(Context& context, const PathOptions& options) {
    const fs::path root = fs::absolute(options.path).lexically_normal();
    if (!fs::is_directory(root)) {
        context.diagnostics.Err() << "Verify expects an unpacked extension directory: " << root << '\n';
        return Status::Error;
    }

    auto manifest = woki::ext::LoadManifest(root / "manifest.yaml");
    if (!manifest) {
        context.diagnostics.Error(manifest.error().Message());
        return Status::Error;
    }

    auto layout = SourceLayout(root);
    if (!layout) {
        context.diagnostics.Error(layout.error().Message());
        return Status::Error;
    }

    auto valid = woki::ext::ValidatePackageLayout(*layout);
    if (!valid) {
        context.diagnostics.Error(valid.error().Message());
        return Status::Error;
    }

    auto guest = woki::ext::wasm::ValidateGuestModule(layout->wasm, *manifest);
    if (!guest) {
        context.diagnostics.Error(guest.error().Message());
        return Status::Error;
    }

    context.diagnostics.Out() << "Verified extension: " << root << '\n';
    return Status::Ok;
}

} // namespace wokiext
