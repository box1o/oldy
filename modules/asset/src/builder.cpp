#include <algorithm>

#include <woki/asset/builder.hpp>

namespace woki::asset {
namespace {
Result<void> Add(std::vector<ProductDependency>& values, ProductDependency value) {
    if (!value.asset_id)
        return Err(ErrorCode::InvalidArgument, "dependency has no asset ID");
    const auto found = std::ranges::lower_bound(values, value);
    if (found != values.end() && found->asset_id == value.asset_id)
        return found->product_hash == value.product_hash ? Ok() : Result<void>(Err(ErrorCode::InvalidArgument, "dependency has conflicting hashes"));
    values.insert(found, std::move(value));
    return Ok();
}

template <typename T>
void Put(std::vector<std::byte>& out, T value) {
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        out.push_back(static_cast<std::byte>(value & 0xffU));
        value >>= 8U;
    }
}

void PutId(std::vector<std::byte>& out, const AssetId& id) {
    for (u8 byte : id.Bytes())
        out.push_back(static_cast<std::byte>(byte));
}

void PutHash(std::vector<std::byte>& out, const ContentHash& hash) {
    for (u8 byte : hash.Bytes())
        out.push_back(static_cast<std::byte>(byte));
}
} // namespace

Result<void> BuildContext::AddSourceDependency(const AssetId id, const ContentHash hash) {
    return Add(source_dependencies_, {id, hash});
}

Result<void> BuildContext::AddProductDependency(ProductDependency dependency) {
    return Add(product_dependencies_, std::move(dependency));
}

Result<void> BuildContext::AddGeneratedProduct(Product product) {
    if (!product.asset_id)
        return Err(ErrorCode::InvalidArgument, "generated product has no asset ID");
    if (std::ranges::any_of(generated_products_, [&](const Product& current) { return current.asset_id == product.asset_id && current.subresource == product.subresource; }))
        return Err(ErrorCode::ValidationInvalidState, "generated product identity is duplicated");
    TRY_VOID(ValidateProduct(product));
    generated_products_.push_back(std::move(product));
    return Ok();
}

Result<void> BuilderRegistry::Register(ref<const AssetBuilder> builder) {
    if (!builder || !builder->Descriptor().id || builder->Descriptor().name.empty())
        return Err(ErrorCode::InvalidArgument, "builder descriptor is incomplete");
    const auto& descriptor = builder->Descriptor();
    if (builders_.contains(descriptor.id) || source_builders_.contains(descriptor.source_type))
        return Err(ErrorCode::InvalidArgument, "builder ID or source type is already registered");
    source_builders_.emplace(descriptor.source_type, descriptor.id);
    builders_.emplace(descriptor.id, std::move(builder));
    return Ok();
}

const AssetBuilder* BuilderRegistry::Find(const AssetId id) const noexcept {
    const auto found = builders_.find(id);
    return found == builders_.end() ? nullptr : found->second.get();
}

const AssetBuilder* BuilderRegistry::FindForSource(const u32 type) const noexcept {
    const auto found = source_builders_.find(type);
    return found == source_builders_.end() ? nullptr : Find(found->second);
}

Result<BuildOutput> BuilderRegistry::Build(const AssetId id, const BuildRequest& request, const std::span<const std::byte> source) const {
    const auto* builder = Find(id);
    if (!builder)
        return Err(ErrorCode::FileNotFound, "asset builder is not registered");
    if (Sha256(source) != request.source_hash)
        return Err(ErrorCode::ValidationInvalidState, "build source hash does not match request");
    BuildContext context;
    auto product = builder->Build(request, context, source);
    if (!product)
        return Err(std::move(product).error());
    const auto& descriptor = builder->Descriptor();
    if (product->asset_id != request.asset_id || product->source_hash != request.source_hash || product->target_fingerprint != request.target_fingerprint || product->builder_version != descriptor.version
        || product->type != descriptor.product_type)
        return Err(ErrorCode::ValidationInvalidState, "builder output does not match request or descriptor");
    TRY_VOID(ValidateProduct(*product));
    return Ok(BuildOutput{std::move(*product), context.GeneratedProducts(), context.SourceDependencies(), context.ProductDependencies()});
}

ContentHash BuildKey(const BuilderDescriptor& builder, const BuildRequest& request, const std::span<const ProductDependency> dependencies) {
    std::vector<std::byte> bytes;
    PutId(bytes, builder.id);
    Put(bytes, builder.version);
    Put(bytes, builder.source_type);
    Put(bytes, builder.product_type);
    PutId(bytes, request.asset_id);
    PutHash(bytes, request.source_hash);
    PutHash(bytes, request.target_fingerprint);
    Put(bytes, static_cast<u64>(dependencies.size()));
    for (const auto& dependency : dependencies) {
        PutId(bytes, dependency.asset_id);
        PutHash(bytes, dependency.product_hash);
    }
    return Sha256(bytes);
}
} // namespace woki::asset
