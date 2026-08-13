#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

#include <map>
#include <set>
#include <span>
#include <vector>

#include "product.hpp"

namespace woki::asset {

class DependencyGraph {
public:
    [[nodiscard]] Result<void> Replace(AssetId asset, std::span<const ProductDependency> dependencies);
    void Remove(AssetId asset);
    [[nodiscard]] std::vector<ProductDependency> Dependencies(AssetId asset) const;
    [[nodiscard]] std::vector<AssetId> DirectDependents(AssetId asset) const;
    [[nodiscard]] std::vector<AssetId> TransitiveDependents(AssetId asset) const;
    [[nodiscard]] Result<void> ValidateAcyclic() const;

private:
    [[nodiscard]] Result<void> Validate(const std::map<AssetId, std::vector<ProductDependency>>& graph) const;
    std::map<AssetId, std::vector<ProductDependency>> forward_;
    std::map<AssetId, std::set<AssetId>> reverse_;
};

} // namespace woki::asset
