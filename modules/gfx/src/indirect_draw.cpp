#include <algorithm>

#include <woki/gfx/advanced/indirect_draw.hpp>

namespace woki::gfx {

u32 GpuResourceIndex::Insert(const u64 physical_key) {
    const auto [found, inserted] = indices_.try_emplace(physical_key, static_cast<u32>(indices_.size()));
    return found->second;
}

std::optional<u32> GpuResourceIndex::Resolve(const u64 physical_key) const noexcept {
    const auto found = indices_.find(physical_key);
    return found == indices_.end() ? std::nullopt : std::optional<u32>(found->second);
}

void GpuResourceIndex::Rebuild() noexcept {
    indices_.clear();
    ++generation_;
    if (generation_ == 0)
        generation_ = 1;
}

PsoWarmupManifest BuildPsoWarmupManifest(const std::span<const PsoWarmupEntry> reachable) {
    PsoWarmupManifest result{std::vector<PsoWarmupEntry>(reachable.begin(), reachable.end())};
    std::ranges::sort(result.reachable);
    result.reachable.erase(std::unique(result.reachable.begin(), result.reachable.end()), result.reachable.end());
    return result;
}

std::string_view GpuDrivenFallbackName(const GpuDrivenFallbackReason reason) noexcept {
    switch (reason) {
        case GpuDrivenFallbackReason::None:
            return "none";
        case GpuDrivenFallbackReason::Disabled:
            return "disabled";
        case GpuDrivenFallbackReason::MissingCompute:
            return "missing-compute";
        case GpuDrivenFallbackReason::MissingIndirect:
            return "missing-indirect";
        case GpuDrivenFallbackReason::MissingIndirectCount:
            return "missing-indirect-count";
        case GpuDrivenFallbackReason::MissingStorage:
            return "missing-storage";
        case GpuDrivenFallbackReason::ProgramsUnavailable:
            return "programs-unavailable";
        case GpuDrivenFallbackReason::SceneUnavailable:
            return "gpu-scene-unavailable";
        case GpuDrivenFallbackReason::MeshStreamsUnavailable:
            return "mesh-streams-unavailable";
        case GpuDrivenFallbackReason::PipelinePending:
            return "pipeline-pending";
        case GpuDrivenFallbackReason::CapacityOverflow:
            return "capacity-overflow";
        case GpuDrivenFallbackReason::ValidationFailure:
            return "validation-failure";
    }
    return "unknown";
}

} // namespace woki::gfx
