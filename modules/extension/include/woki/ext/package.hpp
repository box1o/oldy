#pragma once

// IWYU pragma: private, include "woki/ext/ext.hpp"

#include <filesystem>

#include "manifest.hpp"

namespace woki::ext {

struct Roots {
    std::filesystem::path extensions;
    std::filesystem::path data;
    std::filesystem::path cache;
};

[[nodiscard]] Result<Roots> RootsFromBase(const std::filesystem::path& base);

[[nodiscard]] bool IsAllowedArchiveEntry(const std::filesystem::path& relative_path, const std::filesystem::path& wasm_path);

struct PackageLayout {
    std::filesystem::path install_root;
    std::filesystem::path manifest;
    std::filesystem::path wasm;
    std::filesystem::path data_root;
    std::filesystem::path cache_root;
};

[[nodiscard]] Result<PackageLayout> ResolvePackageLayout(const Manifest& manifest, const std::filesystem::path& extensions_root, const std::filesystem::path& data_root, const std::filesystem::path& cache_root);

[[nodiscard]] Result<void> ValidatePackageLayout(const PackageLayout& layout);
[[nodiscard]] Result<PackageLayout> InstallUnpackedPackage(const std::filesystem::path& source_root, const Roots& roots);
[[nodiscard]] Result<PackageLayout> InstallArchive(const std::filesystem::path& archive_path, const Roots& roots);

} // namespace woki::ext
