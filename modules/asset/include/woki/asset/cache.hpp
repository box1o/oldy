#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

#include <filesystem>
#include <mutex>
#include <string>

#include "product.hpp"

namespace woki::asset {

struct ProductCacheKey {
    std::string target;
    std::string platform;
    ContentHash capability_fingerprint;
    u32 asset_type{};
    ContentHash product_hash;
};

struct ProductCacheDescriptor {
    std::filesystem::path persistent_root;
    std::string target;
    std::string platform;
    ContentHash capability_fingerprint;
};

class ProductCache {
public:
    [[nodiscard]] static Result<ProductCache> Create(std::filesystem::path root, std::size_t max_product_bytes = 256U * 1024U * 1024U);
    [[nodiscard]] static Result<ProductCache> Create(ProductCacheDescriptor descriptor, std::size_t max_product_bytes = 256U * 1024U * 1024U);
    ProductCache(ProductCache&& other) noexcept;
    ProductCache& operator=(ProductCache&&) = delete;
    [[nodiscard]] std::filesystem::path Path(ContentHash key) const;
    [[nodiscard]] std::filesystem::path Path(const ProductCacheKey& key) const;
    [[nodiscard]] Result<Product> Load(ContentHash key) const;
    [[nodiscard]] Result<Product> Load(const ProductCacheKey& key) const;
    [[nodiscard]] Result<std::vector<std::byte>> ReadRange(ContentHash key, u64 offset, std::size_t size) const;
    [[nodiscard]] Result<std::vector<std::byte>> ReadRange(const ProductCacheKey& key, u64 offset, std::size_t size) const;
    [[nodiscard]] Result<u64> Size(ContentHash key) const;
    [[nodiscard]] Result<u64> Size(const ProductCacheKey& key) const;
    [[nodiscard]] Result<void> Store(ContentHash key, const Product& product);
    [[nodiscard]] Result<void> Store(const ProductCacheKey& key, const Product& product);
    [[nodiscard]] Result<void> Remove(ContentHash key);

private:
    ProductCache(ProductCacheDescriptor descriptor, std::size_t limit)
        : descriptor_(std::move(descriptor)),
          root_(descriptor_.persistent_root),
          max_product_bytes_(limit) {}

    ProductCacheDescriptor descriptor_;
    std::filesystem::path root_;
    std::size_t max_product_bytes_{};
    // In-process single writer. Multiple processes require an external cache lock.
    mutable std::mutex writer_;
};

} // namespace woki::asset
