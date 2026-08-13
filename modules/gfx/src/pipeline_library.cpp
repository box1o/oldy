#include <woki/gfx/advanced/pipeline_library.hpp>

namespace woki::gfx {

PipelineHandle RenderPipelineLibrary::Create() {
    if (!free_.empty()) {
        const u32 index = free_.back();
        free_.pop_back();
        slots_[index].occupied = true;
        return PipelineHandle::Create(index, slots_[index].generation);
    }
    const u32 index = static_cast<u32>(slots_.size());
    slots_.push_back({});
    slots_.back().occupied = true;
    return PipelineHandle::Create(index, slots_.back().generation);
}

Result<void> RenderPipelineLibrary::Destroy(const PipelineHandle handle) {
    if (!handle.IsValid() || handle.Index() >= slots_.size() || !slots_[handle.Index()].occupied
        || slots_[handle.Index()].generation != handle.Generation())
        return Err(ErrorCode::InvalidArgument, "pipeline handle is stale or invalid");
    Slot& slot = slots_[handle.Index()];
    if (slot.current)
        assets_.erase(slot.current->pipeline.asset_id);
    slot.current.reset();
    slot.record = {};
    slot.occupied = false;
    ++slot.generation;
    if (slot.generation == 0)
        ++slot.generation;
    free_.push_back(handle.Index());
    return Ok();
}

Result<void> RenderPipelineLibrary::Publish(const PipelineHandle handle, const asset::Product& product) {
    if (!handle.IsValid() || handle.Index() >= slots_.size() || !slots_[handle.Index()].occupied
        || slots_[handle.Index()].generation != handle.Generation())
        return Err(ErrorCode::InvalidArgument, "pipeline handle is stale or invalid");
    TRY_VOID(asset::ValidateProduct(product));
    if (product.type != kPipelineProductType || product.schema_version != kPipelineProductVersion)
        return Err(ErrorCode::ParseInvalidFormat, "asset product is not a render pipeline product");
    auto pipeline = ParsePipelineProduct(product.payload);
    if (!pipeline)
        return Err(std::move(pipeline).error());
    if (pipeline->root_source_hash != product.source_hash)
        return Err(ErrorCode::ParseInvalidFormat, "pipeline root source hash is inconsistent");
    if (pipeline->asset_id != product.asset_id)
        return Err(ErrorCode::ParseInvalidFormat, "pipeline product identity is inconsistent");
    // Pipeline IR dependencies identify authored source files and their hashes
    // for deterministic recompilation. Product dependencies contain only
    // exact edges to other cooked products and are validated independently by
    // ValidateProduct().
    Slot& slot = slots_[handle.Index()];
    const u64 version = slot.record.version + 1;
    const auto old_id = slot.current ? std::optional(slot.current->pipeline.asset_id) : std::nullopt;
    if (const auto existing = assets_.find(pipeline->asset_id); existing != assets_.end() && existing->second != handle)
        return Err(ErrorCode::InvalidArgument, "pipeline asset ID is already published by another handle");
    slot.current = createRef<const PipelineGeneration>(
        PipelineGeneration{std::move(*pipeline), version, product.product_hash}
    );
    slot.record = {PipelineState::Ready, version, product.product_hash};
    if (old_id && *old_id != slot.current->pipeline.asset_id)
        assets_.erase(*old_id);
    assets_.insert_or_assign(slot.current->pipeline.asset_id, handle);
    return Ok();
}

std::optional<PipelineRecord> RenderPipelineLibrary::Record(const PipelineHandle handle) const {
    if (!handle.IsValid() || handle.Index() >= slots_.size() || !slots_[handle.Index()].occupied
        || slots_[handle.Index()].generation != handle.Generation())
        return std::nullopt;
    return slots_[handle.Index()].record;
}

Result<BorrowedPipeline> RenderPipelineLibrary::Borrow(const PipelineHandle handle) const {
    const auto record = Record(handle);
    if (!record || record->state != PipelineState::Ready)
        return Err(ErrorCode::ValidationInvalidState, "pipeline is not ready");
    return Ok(BorrowedPipeline{slots_[handle.Index()].current});
}

Result<BorrowedPipeline> RenderPipelineLibrary::SelectSupported(PipelineHandle root, CapabilitySet capabilities) const {
    std::ranges::sort(capabilities);
    capabilities.erase(std::ranges::unique(capabilities).begin(), capabilities.end());
    std::set<asset::AssetId> visited;
    while (true) {
        auto current = Borrow(root);
        if (!current)
            return Err(std::move(current).error());
        if (!visited.insert(current->Get().asset_id).second)
            return Err(ErrorCode::ValidationInvalidState, "pipeline fallback selection cycle detected");
        const FallbackRule* selected{};
        for (const auto& fallback : current->Get().fallbacks)
            if (std::ranges::any_of(fallback.missing_capabilities, [&](StringId id) {
                    return !std::ranges::binary_search(capabilities, id);
                })) {
                selected = &fallback;
                break;
            }
        if (!selected) {
            if (std::ranges::none_of(current->Get().required_capabilities, [&](StringId id) {
                    return !std::ranges::binary_search(capabilities, id);
                }))
                return current;
            else
                return Err(
                    ErrorCode::ValidationInvalidState,
                    "pipeline requires unsupported capabilities and has no applicable fallback"
                );
        }
        const auto found = assets_.find(selected->target);
        if (found == assets_.end())
            return Err(
                ErrorCode::ValidationInvalidState,
                "pipeline fallback is unresolved: " + selected->path.String()
            );
        root = found->second;
    }
}

Result<PipelineInstance> RenderPipelineLibrary::Compose(
    const PipelineHandle root,
    CapabilitySet capabilities,
    const FeatureRegistry& registry,
    const FeatureServices& services
) const {
    PipelineInstance result;
    TRY_ASSIGN(result.pipeline, SelectSupported(root, std::move(capabilities)));
    for (const auto& request : result.pipeline.Get().features) {
        const auto* metadata = registry.Metadata(request.id);
        if (!metadata || metadata->version != request.factory_version || metadata->scope != request.scope
            || metadata->multiplicity != request.multiplicity)
            return Err(ErrorCode::ValidationInvalidState, "pipeline feature metadata does not match runtime registry");
        ref<const RenderFeature> instance;
        if (services.acquire)
            TRY_ASSIGN(instance, services.acquire(request.config));
        else
            TRY_ASSIGN(instance, registry.Create(request.config, services));
        result.features.push_back(std::move(instance));
    }
    return Ok(std::move(result));
}

void PipelineDependencyGraph::Replace(
    const PipelineHandle pipeline,
    const std::span<const asset::AssetPath> dependencies
) {
    auto old = forward_.find(pipeline);
    if (old != forward_.end())
        for (const auto& path : old->second) {
            auto found = reverse_.find(path);
            if (found != reverse_.end()) {
                found->second.erase(pipeline);
                if (found->second.empty())
                    reverse_.erase(found);
            }
        }
    auto& next = forward_[pipeline];
    next.assign(dependencies.begin(), dependencies.end());
    std::ranges::sort(next);
    next.erase(std::ranges::unique(next).begin(), next.end());
    for (const auto& path : next)
        reverse_[path].insert(pipeline);
}

std::vector<PipelineHandle> PipelineDependencyGraph::Dependents(const asset::AssetPath& dependency) const {
    const auto found = reverse_.find(dependency);
    return found == reverse_.end() ? std::vector<PipelineHandle>{}
                                   : std::vector<PipelineHandle>{found->second.begin(), found->second.end()};
}

PipelineReloadEvent PipelineReloadCoordinator::Publish(PipelineReloadCandidate candidate) {
    const auto current = library_.Record(candidate.handle);
    if (!current || current->version != candidate.base_version)
        return {candidate.handle,
            PipelineReloadOutcome::Stale,
            current ? current->version : 0,
            "reload candidate was built from a stale generation"};
    if (!candidate.product)
        return {candidate.handle,
            PipelineReloadOutcome::Rejected,
            current->version,
            std::string(candidate.product.error().Message())};
    auto published = library_.Publish(candidate.handle, *candidate.product);
    if (!published)
        return {candidate.handle,
            PipelineReloadOutcome::Rejected,
            current->version,
            std::string(published.error().Message())};
    auto borrowed = library_.Borrow(candidate.handle);
    if (!borrowed)
        return {candidate.handle,
            PipelineReloadOutcome::Rejected,
            current->version,
            std::string(borrowed.error().Message())};
    std::vector<asset::AssetPath> paths;
    for (const auto& dependency : borrowed->Get().dependencies)
        paths.push_back(dependency.path);
    graph_.Replace(candidate.handle, paths);
    return {candidate.handle, PipelineReloadOutcome::Published, borrowed->Version(), {}};
}

std::vector<PipelineHandle> PipelineReloadCoordinator::Invalidate(const asset::AssetPath& dependency) {
    return graph_.Dependents(dependency);
}

} // namespace woki::gfx
