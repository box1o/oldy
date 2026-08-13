#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

#include <map>

#include "product.hpp"
#include "uri.hpp"
#include "vfs.hpp"

namespace woki::asset {

inline constexpr u16 kAssetManifestVersion = 1;

struct ProductLocator {
    AssetUri uri;
    u64 offset{};
    u64 size{};
};

struct ManifestEntry {
    AssetId asset_id;
    u32 type{};
    ContentHash product_hash;
    u32 version{};
    std::string target;
    ContentHash capability_fingerprint;
    ProductLocator locator;
    std::vector<ProductChunkSemantic> available_chunks;
};

class AssetManifest {
public:
    [[nodiscard]] Result<void> Add(ManifestEntry entry);
    [[nodiscard]] const ManifestEntry* Resolve(AssetId id) const noexcept;
    [[nodiscard]] std::vector<ManifestEntry> Entries() const;
    [[nodiscard]] Result<std::vector<std::byte>> Serialize() const;
    [[nodiscard]] static Result<AssetManifest> Parse(std::span<const std::byte> bytes, std::size_t max_bytes = 64U * 1024U * 1024U);
    [[nodiscard]] static Result<AssetManifest> Load(const Vfs& vfs, const AssetUri& uri, std::size_t max_bytes = 64U * 1024U * 1024U);

private:
    std::map<AssetId, ManifestEntry> entries_;
};

} // namespace woki::asset
