#include <optional>
#include <filesystem>

#include <woki/ext/package.hpp>

#include "cli_internal.hpp"

namespace wokiext {

namespace fs = std::filesystem;

Status Install(Context& context, const InstallOptions& options) {
    auto roots = woki::ext::RootsFromBase(options.root);
    if (!roots) {
        context.diagnostics.Error(roots.error().Message());
        return Status::Error;
    }

    const fs::path path = fs::absolute(options.path).lexically_normal();
    fs::path install_path = path;
    std::optional<TemporaryDirectory> temporary;

    if (fs::is_directory(path)) {
        auto created = TemporaryDirectory::Create("wokiext-install-");
        if (!created) {
            context.diagnostics.Error(created.error());
            return Status::Error;
        }
        temporary.emplace(std::move(*created));
        install_path = temporary->Path() / "package.wokiext";
        if (Bundle(context, BundleOptions{.path = path, .out_file = install_path, .executable = {}}) != Status::Ok) {
            return Status::Error;
        }
    }

    const auto policy = options.force ? woki::ext::InstallPolicy::ReplaceExisting : woki::ext::InstallPolicy::FailIfExists;
    woki::Result<woki::ext::PackageLayout> installed = woki::ext::InstallArchive(install_path, *roots, policy);

    if (!installed) {
        context.diagnostics.Error(installed.error().Message());
        return Status::Error;
    }

    context.diagnostics.Out() << "Installed extension: " << installed->install_root << '\n';
    return Status::Ok;
}

} // namespace wokiext
