#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

#include "manifest.hpp"

namespace woki::asset {

inline constexpr u16 kAssetPackageVersion = 1;

enum class PackageProfile : u8 { Development, Shipping };

struct PackageOptions {
    PackageProfile profile{PackageProfile::Shipping};
    std::string target;
    ContentHash capability_fingerprint;
    AssetUri bundle_uri;
};

struct AssetPackage {
    std::vector<std::byte> bundle;
    AssetManifest manifest;
};

// Products may contain more than the requested roots. Only exact, hash-matching
// ProductDependency reachability is emitted. Cycles and missing closure fail.
[[nodiscard]] Result<AssetPackage> BuildPackage(std::span<const Product> products, std::span<const AssetId> roots, const PackageOptions& options);

} // namespace woki::asset
