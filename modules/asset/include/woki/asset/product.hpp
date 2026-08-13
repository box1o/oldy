#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

#include <cstddef>
#include <functional>
#include <span>
#include <vector>

#include <woki/core.hpp>

#include "id.hpp"

namespace woki::asset {

class ProductCache;
class Vfs;
class AssetUri;
struct ProductLocator;

inline constexpr u16 kProductFormatVersion = 3;
inline constexpr u16 kOldestReadableProductFormatVersion = 3;

enum class ProductCompression : u8 { None = 0, Zstd = 1 };

enum class ProductChunkSemantic : u32 {
    Generic = 0,
    Header = 1,
    MeshVertices = 2,
    MeshIndices = 3,
    Meshlets = 4,
    TextureMip = 5,
    Debug = 0xffff'fffeU,
};

struct ProductDependency {
    AssetId asset_id;
    ContentHash product_hash;
    [[nodiscard]] friend auto operator<=>(const ProductDependency&, const ProductDependency&) noexcept = default;
};

struct ProductChunk {
    ProductChunkSemantic semantic{ProductChunkSemantic::Generic};
    ProductCompression compression{ProductCompression::None};
    u32 alignment{1};
    u64 offset{};
    u64 compressed_size{};
    u64 uncompressed_size{};
    ContentHash checksum;
    [[nodiscard]] friend auto operator<=>(const ProductChunk&, const ProductChunk&) noexcept = default;
};

struct Product {
    AssetId asset_id;
    SubresourceId subresource;
    u32 type{};
    u32 schema_version{};
    u32 builder_version{};
    u32 container_version{kProductFormatVersion};
    ContentHash source_hash;
    std::vector<ProductDependency> dependencies;
    ContentHash target_fingerprint;
    std::vector<ProductChunk> chunks;
    ContentHash product_hash;
    std::vector<std::byte> payload;
};

struct ProductLimits {
    std::size_t max_payload_bytes{256U * 1024U * 1024U};
    std::size_t max_decompressed_chunk_bytes{256U * 1024U * 1024U};
    u32 max_dependencies{4096};
    u32 max_chunks{65'536};
};

[[nodiscard]] Product MakeProduct(AssetId asset_id,
    u32 type,
    u32 schema_version,
    u32 builder_version,
    ContentHash source_hash,
    std::vector<ProductDependency> dependencies,
    ContentHash target_fingerprint,
    std::vector<std::byte> payload,
    SubresourceId subresource = {});
[[nodiscard]] ContentHash HashProduct(const Product& product);
[[nodiscard]] Result<void> ValidateProduct(const Product& product, ProductLimits limits = {});
[[nodiscard]] Result<std::vector<std::byte>> SerializeProduct(const Product& product, ProductLimits limits = {});
[[nodiscard]] Result<Product> ParseProduct(std::span<const std::byte> bytes, ProductLimits limits = {});

// Container v1/v2 products must be rebuilt. Their contiguous, untyped chunks
// cannot be migrated to v3 without product-type-specific semantic knowledge.
[[nodiscard]] bool ProductRequiresRebuild(u16 container_version) noexcept;

struct ProductHeader {
    AssetId asset_id;
    SubresourceId subresource;
    u32 type{};
    u32 schema_version{};
    u32 builder_version{};
    u32 container_version{};
    ContentHash source_hash;
    ContentHash target_fingerprint;
    ContentHash product_hash;
    std::vector<ProductDependency> dependencies;
    std::vector<ProductChunk> chunks;
    u64 payload_offset{};
    u64 payload_size{};
    u64 file_size{};
};

class ProductReader {
public:
    [[nodiscard]] static Result<ProductReader> Open(const Vfs& vfs, const AssetUri& uri, ProductLimits limits = {});
    [[nodiscard]] static Result<ProductReader> Open(const Vfs& vfs, const ProductLocator& locator, ProductLimits limits = {});
    [[nodiscard]] static Result<ProductReader> Open(const ProductCache& cache, ContentHash key, ProductLimits limits = {});

    [[nodiscard]] const ProductHeader& Header() const noexcept {
        return header_;
    }

    [[nodiscard]] std::span<const ProductChunk> Chunks() const noexcept {
        return header_.chunks;
    }

    [[nodiscard]] Result<std::vector<std::byte>> ReadRange(u64 file_offset, std::size_t size) const;
    [[nodiscard]] Result<std::vector<std::byte>> ReadChunk(std::size_t index) const;
    [[nodiscard]] Result<Product> ReadProduct() const;

private:
    using RangeRead = std::function<Result<std::vector<std::byte>>(u64, std::size_t)>;

    ProductReader(ProductHeader header, ProductLimits limits, RangeRead read)
        : header_(std::move(header)),
          limits_(limits),
          read_(std::move(read)) {}

    [[nodiscard]] static Result<ProductReader> Open(u64 size, ProductLimits limits, RangeRead read);

    ProductHeader header_;
    ProductLimits limits_;
    RangeRead read_;
};

} // namespace woki::asset
