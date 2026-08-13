#include <algorithm>
#include <functional>

#include <woki/asset/dependency_graph.hpp>

namespace woki::asset {

Result<void> DependencyGraph::Validate(const std::map<AssetId, std::vector<ProductDependency>>& graph) const {
    enum class Mark : u8 { Visiting, Complete };
    std::map<AssetId, Mark> marks;
    std::function<bool(AssetId)> visit = [&](AssetId id) {
        if (const auto found = marks.find(id); found != marks.end())
            return found->second == Mark::Complete;
        marks[id] = Mark::Visiting;
        if (const auto found = graph.find(id); found != graph.end())
            for (const auto& dependency : found->second)
                if (!visit(dependency.asset_id))
                    return false;
        marks[id] = Mark::Complete;
        return true;
    };
    for (const auto& [id, unused] : graph)
        if (!visit(id))
            return Err(ErrorCode::ValidationInvalidState, "asset dependency cycle detected");
    return Ok();
}

Result<void> DependencyGraph::Replace(const AssetId asset, const std::span<const ProductDependency> dependencies) {
    if (!asset)
        return Err(ErrorCode::InvalidArgument, "dependency owner has no asset ID");
    std::vector<ProductDependency> ordered(dependencies.begin(), dependencies.end());
    std::ranges::sort(ordered);
    if (std::ranges::adjacent_find(ordered, {}, &ProductDependency::asset_id) != ordered.end())
        return Err(ErrorCode::InvalidArgument, "dependency list contains duplicate asset IDs");
    auto candidate = forward_;
    candidate.insert_or_assign(asset, std::move(ordered));
    TRY_VOID(Validate(candidate));
    forward_ = std::move(candidate);
    reverse_.clear();
    for (const auto& [owner, edges] : forward_)
        for (const auto& edge : edges)
            reverse_[edge.asset_id].insert(owner);
    return Ok();
}

void DependencyGraph::Remove(const AssetId asset) {
    forward_.erase(asset);
    reverse_.clear();
    for (const auto& [owner, edges] : forward_)
        for (const auto& edge : edges)
            reverse_[edge.asset_id].insert(owner);
}

std::vector<ProductDependency> DependencyGraph::Dependencies(const AssetId asset) const {
    const auto found = forward_.find(asset);
    return found == forward_.end() ? std::vector<ProductDependency>{} : found->second;
}

std::vector<AssetId> DependencyGraph::DirectDependents(const AssetId asset) const {
    const auto found = reverse_.find(asset);
    return found == reverse_.end() ? std::vector<AssetId>{} : std::vector<AssetId>(found->second.begin(), found->second.end());
}

std::vector<AssetId> DependencyGraph::TransitiveDependents(const AssetId asset) const {
    std::set<AssetId> result;
    std::vector<AssetId> pending{asset};
    while (!pending.empty()) {
        auto current = pending.back();
        pending.pop_back();
        for (auto dependent : DirectDependents(current))
            if (result.insert(dependent).second)
                pending.push_back(dependent);
    }
    return {result.begin(), result.end()};
}

Result<void> DependencyGraph::ValidateAcyclic() const {
    return Validate(forward_);
}

} // namespace woki::asset
