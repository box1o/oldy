#pragma once

#include <string>
#include <filesystem>

#include "manifest.hpp"

namespace woki::ext {

struct Roots {
    std::filesystem::path extensions;
    std::filesystem::path data;
    std::filesystem::path cache;
    std::filesystem::path config{};
};

[[nodiscard]] Result<Roots> RootsFromBase(const std::filesystem::path& base);
[[nodiscard]] Result<Roots> ValidateRoots(const Roots& roots);
[[nodiscard]] Result<Roots> DefaultRoots();

[[nodiscard]] bool IsAllowedArchiveEntry(const std::filesystem::path& relative_path, const std::filesystem::path& wasm_path);

struct PackageLayout {
    std::filesystem::path install_root;
    std::filesystem::path manifest;
    std::filesystem::path wasm;
    std::filesystem::path data_root;
    std::filesystem::path config_root;
    std::filesystem::path cache_root;
};

class ExtensionPackage final {
public:
    [[nodiscard]] static Result<ExtensionPackage> Create(std::string id, Manifest manifest, PackageLayout layout);
    [[nodiscard]] const std::string& Id() const noexcept;
    [[nodiscard]] const Manifest& GetManifest() const noexcept;
    [[nodiscard]] const PackageLayout& Layout() const noexcept;

private:
    ExtensionPackage(std::string id, Manifest manifest, PackageLayout layout);

    std::string id_;
    Manifest manifest_;
    PackageLayout layout_;
};

class DiscoveryFailure final {
public:
    DiscoveryFailure(std::string candidate_id, std::filesystem::path package_root, Error error);
    [[nodiscard]] const std::string& CandidateId() const noexcept;
    [[nodiscard]] const std::filesystem::path& PackageRoot() const noexcept;
    [[nodiscard]] const Error& Cause() const noexcept;

private:
    std::string candidate_id_;
    std::filesystem::path package_root_;
    Error error_;
};

enum class InstallPolicy : u8 {
    FailIfExists,
    ReplaceExisting,
};

[[nodiscard]] Result<PackageLayout> ResolvePackageLayout(const Manifest& manifest, const std::filesystem::path& extensions_root, const std::filesystem::path& data_root, const std::filesystem::path& cache_root);
[[nodiscard]] Result<PackageLayout> ResolvePackageLayout(const Manifest& manifest, const Roots& roots);

[[nodiscard]] Result<void> ValidatePackageLayout(const PackageLayout& layout);
[[nodiscard]] Result<PackageLayout> InstallUnpackedPackage(const std::filesystem::path& source_root, const Roots& roots, InstallPolicy policy = InstallPolicy::FailIfExists);
[[nodiscard]] Result<PackageLayout> InstallArchive(const std::filesystem::path& archive_path, const Roots& roots, InstallPolicy policy = InstallPolicy::FailIfExists);

} // namespace woki::ext
