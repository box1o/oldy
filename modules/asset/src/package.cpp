#include <algorithm>
#include <array>
#include <map>
#include <set>

#include <woki/asset/package.hpp>

namespace woki::asset {
namespace {
template <typename T>
void Put(std::vector<std::byte>& out, T value) {
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        out.push_back(static_cast<std::byte>(value & 0xffU));
        value >>= 8U;
    }
}

void PutId(std::vector<std::byte>& out, const AssetId& id) {
    for (u8 value : id.Bytes())
        out.push_back(static_cast<std::byte>(value));
}

void PutHash(std::vector<std::byte>& out, const ContentHash& hash) {
    for (u8 value : hash.Bytes())
        out.push_back(static_cast<std::byte>(value));
}
} // namespace

Result<AssetPackage> BuildPackage(const std::span<const Product> products, const std::span<const AssetId> roots, const PackageOptions& options) {
    if (roots.empty() || options.target.empty() || options.bundle_uri.String().empty())
        return Err(ErrorCode::InvalidArgument, "package roots, target, and bundle URI are required");
    std::map<AssetId, const Product*> available;
    for (const auto& product : products) {
        TRY_VOID(ValidateProduct(product));
        const auto [found, inserted] = available.emplace(product.asset_id, &product);
        if (!inserted && found->second->product_hash != product.product_hash)
            return Err(ErrorCode::ValidationInvalidState, "package input contains conflicting products for one asset ID");
    }
    std::set<AssetId> reachable, visiting;
    std::vector<AssetId> dependency_order;
    const auto visit = [&](const auto& self, const AssetId id) -> Result<void> {
        if (reachable.contains(id))
            return Ok();
        const auto found = available.find(id);
        if (found == available.end())
            return Err(ErrorCode::FileNotFound, "package dependency closure is missing a product");
        if (!visiting.insert(id).second)
            return Err(ErrorCode::ValidationInvalidState, "package dependency graph contains a cycle");
        for (const auto& dependency : found->second->dependencies) {
            const auto child = available.find(dependency.asset_id);
            if (child == available.end())
                return Err(ErrorCode::FileNotFound, "package dependency closure is missing product " + dependency.asset_id.String());
            if (child->second->product_hash != dependency.product_hash)
                return Err(ErrorCode::ValidationInvalidState,
                    "package dependency hash mismatch for " + dependency.asset_id.String() + ": expected "
                        + dependency.product_hash.Hex() + ", found " + child->second->product_hash.Hex());
            TRY_VOID(self(self, dependency.asset_id));
        }
        visiting.erase(id);
        reachable.insert(id);
        dependency_order.push_back(id);
        return Ok();
    };
    for (const auto root : roots)
        TRY_VOID(visit(visit, root));

    std::map<AssetId, Product> packaged;
    for (const auto id : dependency_order) {
        Product product = *available.at(id);
        for (auto& dependency : product.dependencies)
            dependency.product_hash = packaged.at(dependency.asset_id).product_hash;
        if (options.profile == PackageProfile::Shipping && std::ranges::any_of(product.chunks, [](const ProductChunk& chunk) { return chunk.semantic == ProductChunkSemantic::Debug; })) {
            std::vector<std::byte> payload;
            std::vector<ProductChunk> chunks;
            for (auto chunk : product.chunks) {
                if (chunk.semantic == ProductChunkSemantic::Debug)
                    continue;
                const u64 aligned = (static_cast<u64>(payload.size()) + chunk.alignment - 1U) & ~(static_cast<u64>(chunk.alignment) - 1U);
                payload.resize(static_cast<std::size_t>(aligned));
                const auto source = std::span(product.payload).subspan(static_cast<std::size_t>(chunk.offset), static_cast<std::size_t>(chunk.compressed_size));
                chunk.offset = aligned;
                payload.insert(payload.end(), source.begin(), source.end());
                chunks.push_back(chunk);
            }
            product.payload = std::move(payload);
            product.chunks = std::move(chunks);
        }
        product.product_hash = HashProduct(product);
        TRY_VOID(ValidateProduct(product));
        packaged.emplace(id, std::move(product));
    }

    AssetPackage result;
    const std::array magic{std::byte{'W'}, std::byte{'K'}, std::byte{'P'}, std::byte{'K'}};
    result.bundle.insert(result.bundle.end(), magic.begin(), magic.end());
    Put(result.bundle, kAssetPackageVersion);
    Put(result.bundle, u16{});
    Put(result.bundle, static_cast<u32>(reachable.size()));
    const u64 index_size = static_cast<u64>(reachable.size()) * (AssetId::kSize + ContentHash::kSize + 4 + 8 + 8);
    std::vector<std::vector<std::byte>> serialized;
    serialized.reserve(reachable.size());
    u64 offset = 12 + index_size;
    for (const auto id : reachable) {
        const auto& product = packaged.at(id);
        auto bytes = SerializeProduct(product);
        if (!bytes)
            return Err(std::move(bytes).error());
        serialized.push_back(std::move(*bytes));
        PutId(result.bundle, id);
        PutHash(result.bundle, product.product_hash);
        Put(result.bundle, product.type);
        Put(result.bundle, offset);
        Put(result.bundle, static_cast<u64>(serialized.back().size()));
        offset += serialized.back().size();
    }
    std::size_t index{};
    for (const auto id : reachable) {
        const Product& product = packaged.at(id);
        const u64 product_offset = result.bundle.size();
        result.bundle.insert(result.bundle.end(), serialized[index].begin(), serialized[index].end());
        ManifestEntry entry{product.asset_id, product.type, product.product_hash, product.schema_version, options.target, product.target_fingerprint, {options.bundle_uri, product_offset, serialized[index].size()}, {}};
        for (const auto& chunk : product.chunks)
            if (options.profile == PackageProfile::Development || chunk.semantic != ProductChunkSemantic::Debug)
                entry.available_chunks.push_back(chunk.semantic);
        TRY_VOID(result.manifest.Add(std::move(entry)));
        ++index;
    }
    return Ok(std::move(result));
}
} // namespace woki::asset
