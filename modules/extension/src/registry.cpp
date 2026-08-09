#include <string>
#include <algorithm>
#include <filesystem>
#include <system_error>

#include "woki/ext/registry.hpp"

namespace woki::ext {

namespace {

namespace fs = std::filesystem;

[[nodiscard]] Result<fs::path> RequiredPath(Result<fs::path> path) {
    if (!path) {
        return Err(path.error());
    }
    return Ok(path->lexically_normal());
}

[[nodiscard]] bool IsCandidatePackageDir(const fs::directory_entry& entry) {
    std::error_code error;
    return entry.is_directory(error) && fs::is_regular_file(entry.path() / "manifest.yaml", error);
}

[[nodiscard]] Record FailedRecord(std::string id, const fs::path& root, const Error& error) {
    Record record;
    record.id = std::move(id);
    record.package.install_root = root;
    record.package.manifest = root / "manifest.yaml";
    record.state = State::Failed;
    record.error = std::string(error.Message());
    return record;
}

} // namespace

Result<Roots> DefaultRoots() {
    auto data = RequiredPath(paths::DataDirectory("woki"));
    if (!data) {
        return Err(data.error());
    }

    auto cache = RequiredPath(paths::CacheDirectory("woki"));
    if (!cache) {
        return Err(cache.error());
    }

    Roots roots;
    roots.extensions = *data / "extensions";
    roots.data = *data / "ext-data";
    roots.cache = *cache / "ext";
    return Ok(std::move(roots));
}

Result<Roots> RootsFromBase(const fs::path& base) {
    if (base.empty()) {
        return DefaultRoots();
    }

    Roots roots;
    const fs::path normalized = fs::absolute(base).lexically_normal();
    roots.extensions = normalized / "extensions";
    roots.data = normalized / "ext-data";
    roots.cache = normalized / "cache" / "ext";
    return Ok(std::move(roots));
}

void Registry::SetRoots(Roots roots) {
    roots_ = std::move(roots);
}

Result<void> Registry::Scan() {
    records_.clear();

    if (roots_.extensions.empty() || roots_.data.empty() || roots_.cache.empty()) {
        auto defaults = DefaultRoots();
        if (!defaults) {
            return Err(defaults.error());
        }
        roots_ = std::move(*defaults);
    }

    std::error_code error;
    if (!fs::exists(roots_.extensions, error)) {
        return Ok();
    }
    if (!fs::is_directory(roots_.extensions, error)) {
        return Err(ErrorCode::ValidationInvalidState, "Extension root is not a directory: " + roots_.extensions.string());
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(roots_.extensions, error)) {
        if (error) {
            return Err(ErrorCode::FileReadError, error.message());
        }
        if (!IsCandidatePackageDir(entry)) {
            continue;
        }

        const fs::path& package_root = entry.path();
        const std::string package_id = package_root.filename().string();
        auto manifest = LoadManifest(package_root / "manifest.yaml");
        if (!manifest) {
            records_.push_back(FailedRecord(package_id, package_root, manifest.error()));
            continue;
        }

        auto valid = ValidateManifestForPackage(*manifest, package_id);
        if (!valid) {
            records_.push_back(FailedRecord(package_id, package_root, valid.error()));
            continue;
        }

        auto layout = ResolvePackageLayout(*manifest, roots_.extensions, roots_.data, roots_.cache);
        if (!layout) {
            records_.push_back(FailedRecord(package_id, package_root, layout.error()));
            continue;
        }

        auto valid_layout = ValidatePackageLayout(*layout);
        if (!valid_layout) {
            records_.push_back(FailedRecord(package_id, package_root, valid_layout.error()));
            continue;
        }

        Record record;
        record.id = manifest->id;
        record.manifest = std::move(*manifest);
        record.package = std::move(*layout);
        record.state = State::PermissionChecked;
        records_.push_back(std::move(record));
    }

    std::ranges::sort(records_, {}, &Record::id);
    return Ok();
}

Result<void> Registry::ScanSource(const fs::path& source_root) {
    records_.clear();

    if (roots_.data.empty() || roots_.cache.empty()) {
        auto defaults = DefaultRoots();
        if (!defaults) {
            return Err(defaults.error());
        }
        if (roots_.data.empty()) {
            roots_.data = defaults->data;
        }
        if (roots_.cache.empty()) {
            roots_.cache = defaults->cache;
        }
    }

    std::error_code error;
    if (!fs::exists(source_root, error)) {
        return Ok();
    }
    if (!fs::is_directory(source_root, error)) {
        return Err(ErrorCode::ValidationInvalidState, "Source extension root is not a directory: " + source_root.string());
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(source_root, error)) {
        if (error) {
            return Err(ErrorCode::FileReadError, error.message());
        }
        if (!IsCandidatePackageDir(entry)) {
            continue;
        }

        const fs::path& package_root = entry.path();
        const std::string package_name = package_root.filename().string();
        auto manifest = LoadManifest(package_root / "manifest.yaml");
        if (!manifest) {
            records_.push_back(FailedRecord(package_name, package_root, manifest.error()));
            continue;
        }

        auto valid = ValidateManifest(*manifest);
        if (!valid) {
            records_.push_back(FailedRecord(package_name, package_root, valid.error()));
            continue;
        }

        PackageLayout layout;
        layout.install_root = package_root;
        layout.manifest = package_root / "manifest.yaml";
        layout.wasm = (package_root / manifest->wasm_path).lexically_normal();
        layout.data_root = (roots_.data / manifest->id).lexically_normal();
        layout.cache_root = (roots_.cache / manifest->id).lexically_normal();

        auto valid_layout = ValidatePackageLayout(layout);
        if (!valid_layout) {
            records_.push_back(FailedRecord(manifest->id, package_root, valid_layout.error()));
            continue;
        }

        Record record;
        record.id = manifest->id;
        record.manifest = std::move(*manifest);
        record.package = std::move(layout);
        record.state = State::PermissionChecked;
        records_.push_back(std::move(record));
    }

    std::ranges::sort(records_, {}, &Record::id);
    return Ok();
}

const std::vector<Record>& Registry::Records() const noexcept {
    return records_;
}

std::vector<Record>& Registry::Records() noexcept {
    return records_;
}

} // namespace woki::ext
