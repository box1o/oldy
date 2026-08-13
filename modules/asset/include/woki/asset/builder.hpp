#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

#include <map>
#include <span>
#include <string>
#include <vector>

#include "database.hpp"
#include "product.hpp"

namespace woki::asset {

struct BuilderDescriptor {
    AssetId id;
    u32 version{};
    u32 source_type{};
    u32 product_type{};
    std::string name;
};

struct BuildRequest {
    AssetId asset_id;
    AssetUri source_uri;
    ContentHash source_hash;
    ContentHash target_fingerprint;
    u64 revision{};
};

class BuildContext {
public:
    [[nodiscard]] Result<void> AddSourceDependency(AssetId id, ContentHash source_hash);
    [[nodiscard]] Result<void> AddProductDependency(ProductDependency dependency);
    [[nodiscard]] Result<void> AddGeneratedProduct(Product product);

    [[nodiscard]] const std::vector<ProductDependency>& SourceDependencies() const noexcept {
        return source_dependencies_;
    }

    [[nodiscard]] const std::vector<ProductDependency>& ProductDependencies() const noexcept {
        return product_dependencies_;
    }

    [[nodiscard]] const std::vector<Product>& GeneratedProducts() const noexcept {
        return generated_products_;
    }

private:
    std::vector<ProductDependency> source_dependencies_;
    std::vector<ProductDependency> product_dependencies_;
    std::vector<Product> generated_products_;
};

struct BuildOutput {
    Product product;
    std::vector<Product> generated_products;
    std::vector<ProductDependency> source_dependencies;
    std::vector<ProductDependency> product_dependencies;
};

class AssetBuilder {
public:
    virtual ~AssetBuilder() = default;
    [[nodiscard]] virtual const BuilderDescriptor& Descriptor() const noexcept = 0;
    [[nodiscard]] virtual Result<Product> Build(const BuildRequest& request, BuildContext& context, std::span<const std::byte> source) const = 0;
};

class BuilderRegistry {
public:
    [[nodiscard]] Result<void> Register(ref<const AssetBuilder> builder);
    [[nodiscard]] const AssetBuilder* Find(AssetId builder_id) const noexcept;
    [[nodiscard]] const AssetBuilder* FindForSource(u32 source_type) const noexcept;
    [[nodiscard]] Result<BuildOutput> Build(AssetId builder_id, const BuildRequest& request, std::span<const std::byte> source) const;

private:
    std::map<AssetId, ref<const AssetBuilder>> builders_;
    std::map<u32, AssetId> source_builders_;
};

[[nodiscard]] ContentHash BuildKey(const BuilderDescriptor& builder, const BuildRequest& request, std::span<const ProductDependency> dependencies);

} // namespace woki::asset
