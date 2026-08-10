#include <filesystem>

#include <woki/ext/package.hpp>
#include <woki/ext/registry.hpp>

#include "cli_internal.hpp"

namespace wokiext {

namespace {

namespace fs = std::filesystem;

[[nodiscard]] bool IsSafeExtensionId(std::string_view id) {
    const fs::path path{id};
    return !id.empty() && !path.has_root_path() && !path.has_parent_path() && path != "." && path != "..";
}

[[nodiscard]] bool RemovePath(Context& context, const fs::path& path) {
    std::error_code error;
    fs::remove_all(path, error);
    if (error) {
        context.diagnostics.Err() << error.message() << ": " << path << '\n';
        return false;
    }
    return true;
}

} // namespace

Status List(Context& context, const ListOptions& options) {
    auto roots = woki::ext::RootsFromBase(options.root);
    if (!roots) {
        context.diagnostics.Error(roots.error().Message());
        return Status::Error;
    }

    woki::ext::Registry registry;
    auto scanned = registry.Scan(*roots);
    if (!scanned) {
        context.diagnostics.Error(scanned.error().Message());
        return Status::Error;
    }

    for (const woki::ext::ExtensionPackage& package : registry.Packages()) {
        context.diagnostics.Out() << package.Id() << " " << package.GetManifest().version << " ok\n";
    }
    for (const woki::ext::DiscoveryFailure& failure : registry.Failures()) {
        context.diagnostics.Out() << failure.CandidateId() << " failed " << failure.Cause().Message() << '\n';
    }

    return Status::Ok;
}

Status Remove(Context& context, const RemoveOptions& options) {
    if (!IsSafeExtensionId(options.id)) {
        context.diagnostics.Error("Extension id must be a single relative path component");
        return Status::Usage;
    }

    auto roots = woki::ext::RootsFromBase(options.root);
    if (!roots) {
        context.diagnostics.Error(roots.error().Message());
        return Status::Error;
    }

    bool ok = RemovePath(context, roots->extensions / options.id);
    if (!options.keep_data) {
        ok = RemovePath(context, roots->data / options.id) && ok;
        ok = RemovePath(context, roots->config / options.id) && ok;
        ok = RemovePath(context, roots->cache / options.id) && ok;
    }
    if (!ok) {
        return Status::Error;
    }

    context.diagnostics.Out() << "Removed extension: " << options.id;
    if (options.keep_data)
        context.diagnostics.Out() << " (kept data, config, and cache)";
    context.diagnostics.Out() << '\n';
    return Status::Ok;
}

} // namespace wokiext
