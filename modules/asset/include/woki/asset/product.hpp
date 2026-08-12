#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

#include <cstddef>
#include <span>
#include <vector>

#include <woki/core.hpp>

namespace woki::asset {

inline constexpr u16 kProductFormatVersion = 1;

struct Product {
    u32 type{0};
    u32 schema_version{0};
    ContentHash source_hash;
    std::vector<ContentHash> dependency_hashes;
    ContentHash product_hash;
    std::vector<std::byte> payload;
};

struct ProductLimits {
    std::size_t max_payload_bytes{256U * 1024U * 1024U};
    u32 max_dependencies{4096};
};

[[nodiscard]] Product MakeProduct(u32 type, u32 schema_version, ContentHash source_hash, std::vector<ContentHash> dependency_hashes, std::vector<std::byte> payload);
[[nodiscard]] ContentHash HashProduct(const Product& product);
[[nodiscard]] Result<void> ValidateProduct(const Product& product, ProductLimits limits = {});
[[nodiscard]] Result<std::vector<std::byte>> SerializeProduct(const Product& product, ProductLimits limits = {});
[[nodiscard]] Result<Product> ParseProduct(std::span<const std::byte> bytes, ProductLimits limits = {});

} // namespace woki::asset
