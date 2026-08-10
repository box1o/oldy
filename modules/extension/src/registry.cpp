#include <tuple>
#include <algorithm>
#include <filesystem>
#include <system_error>
#include <unordered_map>

#include "woki/ext/registry.hpp"

namespace woki::ext {

namespace {
namespace fs = std::filesystem;

[[nodiscard]] fs::path ConfigRoot(const Roots& roots) {
    return roots.config.empty() ? roots.data.parent_path() / "ext-config" : roots.config;
}

[[nodiscard]] Result<fs::path> RequiredPath(Result<fs::path> path) {
    if (!path) {
        return Err(path.error());
    }
    return Ok(path->lexically_normal());
}

[[nodiscard]] bool IsCandidatePackageDir(const fs::directory_entry& entry) {
    std::error_code error;
    return !entry.is_symlink(error) && entry.is_directory(error) && !fs::is_symlink(entry.path() / "manifest.yaml", error) && fs::is_regular_file(entry.path() / "manifest.yaml", error);
}
} // namespace

ExtensionPackage::ExtensionPackage(std::string id, Manifest manifest, PackageLayout layout)
    : id_(std::move(id)),
      manifest_(std::move(manifest)),
      layout_(std::move(layout)) {}

Result<ExtensionPackage> ExtensionPackage::Create(std::string id, Manifest manifest, PackageLayout layout) {
    if (auto valid = ValidateManifest(manifest); !valid)
        return Err(valid.error());
    if (id != manifest.id)
        return Err(ErrorCode::ValidationInvalidState, "Extension package id must match its manifest id.");
    if (layout.install_root.empty() || layout.data_root.empty() || layout.config_root.empty() || layout.cache_root.empty())
        return Err(ErrorCode::ValidationInvalidState, "Extension package layout roots must not be empty.");
    if (layout.manifest.lexically_normal() != (layout.install_root / "manifest.yaml").lexically_normal() || layout.wasm.lexically_normal() != (layout.install_root / manifest.wasm_path).lexically_normal()) {
        return Err(ErrorCode::ValidationInvalidState, "Extension package manifest and wasm paths must match its install root and manifest.");
    }
    return Ok(ExtensionPackage(std::move(id), std::move(manifest), std::move(layout)));
}

const std::string& ExtensionPackage::Id() const noexcept {
    return id_;
}

const Manifest& ExtensionPackage::GetManifest() const noexcept {
    return manifest_;
}

const PackageLayout& ExtensionPackage::Layout() const noexcept {
    return layout_;
}

DiscoveryFailure::DiscoveryFailure(std::string candidate_id, fs::path package_root, Error error)
    : candidate_id_(std::move(candidate_id)),
      package_root_(std::move(package_root)),
      error_(std::move(error)) {}

const std::string& DiscoveryFailure::CandidateId() const noexcept {
    return candidate_id_;
}

const fs::path& DiscoveryFailure::PackageRoot() const noexcept {
    return package_root_;
}

const Error& DiscoveryFailure::Cause() const noexcept {
    return error_;
}

Result<Roots> DefaultRoots() {
    auto data = RequiredPath(paths::DataDirectory("woki"));
    if (!data)
        return Err(data.error());
    auto cache = RequiredPath(paths::CacheDirectory("woki"));
    if (!cache)
        return Err(cache.error());
    auto config = RequiredPath(paths::ConfigDirectory("woki"));
    if (!config)
        return Err(config.error());
    return Ok(Roots{*data / "extensions", *data / "ext-data", *cache / "ext", *config / "ext"});
}

Result<Roots> RootsFromBase(const fs::path& base) {
    if (base.empty())
        return DefaultRoots();
    const fs::path normalized = fs::absolute(base).lexically_normal();
    return Ok(Roots{normalized / "extensions", normalized / "ext-data", normalized / "cache" / "ext", normalized / "config" / "ext"});
}

void Registry::Clear() noexcept {
    packages_.clear();
    failures_.clear();
}

Result<void> Registry::Add(ExtensionPackage package) {
    if (Find(package.Id()) != nullptr) {
        return Err(ErrorCode::ValidationInvalidState, "Duplicate extension id '" + package.Id() + "'.");
    }
    packages_.push_back(std::move(package));
    return Ok();
}

void Registry::AddFailure(DiscoveryFailure failure) {
    failures_.push_back(std::move(failure));
}

Result<void> Registry::Scan(const Roots& roots) {
    Registry next;
    std::error_code error;
    const bool exists = fs::exists(roots.extensions, error);
    if (error)
        return Err(ErrorCode::FileReadError, error.message());
    if (!exists) {
        *this = std::move(next);
        return Ok();
    }
    if (!fs::is_directory(roots.extensions, error)) {
        return Err(ErrorCode::ValidationInvalidState, "Extension root is not a directory: " + roots.extensions.string());
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(roots.extensions, error)) {
        if (error)
            return Err(ErrorCode::FileReadError, error.message());
        if (!IsCandidatePackageDir(entry))
            continue;
        const fs::path package_root = entry.path();
        const std::string candidate = package_root.filename().string();
        auto manifest = LoadManifest(package_root / "manifest.yaml");
        if (!manifest) {
            next.AddFailure({candidate, package_root, manifest.error()});
            continue;
        }
        auto valid = ValidateManifestForPackage(*manifest, candidate);
        if (!valid) {
            next.AddFailure({candidate, package_root, valid.error()});
            continue;
        }
        Roots resolved_roots = roots;
        resolved_roots.config = ConfigRoot(roots);
        auto layout = ResolvePackageLayout(*manifest, resolved_roots);
        if (!layout) {
            next.AddFailure({candidate, package_root, layout.error()});
            continue;
        }
        auto valid_layout = ValidatePackageLayout(*layout);
        if (!valid_layout) {
            next.AddFailure({candidate, package_root, valid_layout.error()});
            continue;
        }
        const std::string id = manifest->id;
        auto package = ExtensionPackage::Create(id, std::move(*manifest), std::move(*layout));
        if (!package) {
            next.AddFailure({id, package_root, package.error()});
            continue;
        }
        if (auto added = next.Add(std::move(*package)); !added) {
            next.AddFailure({id, package_root, added.error()});
        }
    }
    if (error)
        return Err(ErrorCode::FileReadError, error.message());
    std::ranges::sort(next.packages_, {}, &ExtensionPackage::Id);
    std::ranges::sort(next.failures_, [](const DiscoveryFailure& left, const DiscoveryFailure& right) { return std::tie(left.CandidateId(), left.PackageRoot()) < std::tie(right.CandidateId(), right.PackageRoot()); });
    *this = std::move(next);
    return Ok();
}

Result<void> Registry::ScanSource(const fs::path& source_root, const Roots& roots) {
    Registry next;
    std::error_code error;
    const bool exists = fs::exists(source_root, error);
    if (error)
        return Err(ErrorCode::FileReadError, error.message());
    if (!exists) {
        *this = std::move(next);
        return Ok();
    }
    if (!fs::is_directory(source_root, error)) {
        return Err(ErrorCode::ValidationInvalidState, "Source extension root is not a directory: " + source_root.string());
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(source_root, error)) {
        if (error)
            return Err(ErrorCode::FileReadError, error.message());
        if (!IsCandidatePackageDir(entry))
            continue;
        const fs::path package_root = entry.path();
        const std::string candidate = package_root.filename().string();
        auto manifest = LoadManifest(package_root / "manifest.yaml");
        if (!manifest) {
            next.AddFailure({candidate, package_root, manifest.error()});
            continue;
        }
        auto valid = ValidateManifest(*manifest);
        if (!valid) {
            next.AddFailure({candidate, package_root, valid.error()});
            continue;
        }
        PackageLayout layout{package_root, package_root / "manifest.yaml", (package_root / manifest->wasm_path).lexically_normal(), (roots.data / manifest->id).lexically_normal(),
            (ConfigRoot(roots) / manifest->id).lexically_normal(), (roots.cache / manifest->id).lexically_normal()};
        auto valid_layout = ValidatePackageLayout(layout);
        if (!valid_layout) {
            next.AddFailure({manifest->id, package_root, valid_layout.error()});
            continue;
        }
        const std::string id = manifest->id;
        auto package = ExtensionPackage::Create(id, std::move(*manifest), std::move(layout));
        if (!package) {
            next.AddFailure({id, package_root, package.error()});
            continue;
        }
        next.packages_.push_back(std::move(*package));
    }
    if (error)
        return Err(ErrorCode::FileReadError, error.message());
    std::unordered_map<std::string, std::size_t> counts;
    for (const ExtensionPackage& package : next.packages_)
        ++counts[package.Id()];
    for (const ExtensionPackage& package : next.packages_) {
        if (counts[package.Id()] > 1) {
            next.AddFailure({package.Id(), package.Layout().install_root, Error(ErrorCode::ValidationInvalidState, "Duplicate source extension id '" + package.Id() + "'. All colliding packages were rejected.")});
        }
    }
    std::erase_if(next.packages_, [&counts](const ExtensionPackage& package) { return counts[package.Id()] > 1; });
    std::ranges::sort(next.packages_, {}, &ExtensionPackage::Id);
    std::ranges::sort(next.failures_, [](const DiscoveryFailure& left, const DiscoveryFailure& right) { return std::tie(left.CandidateId(), left.PackageRoot()) < std::tie(right.CandidateId(), right.PackageRoot()); });
    *this = std::move(next);
    return Ok();
}

std::span<const ExtensionPackage> Registry::Packages() const noexcept {
    return packages_;
}

std::span<const DiscoveryFailure> Registry::Failures() const noexcept {
    return failures_;
}

const ExtensionPackage* Registry::Find(std::string_view id) const noexcept {
    const auto it = std::ranges::find(packages_, id, &ExtensionPackage::Id);
    return it == packages_.end() ? nullptr : &*it;
}

} // namespace woki::ext
