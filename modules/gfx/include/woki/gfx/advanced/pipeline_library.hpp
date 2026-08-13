#pragma once

#include <functional>

#include <woki/platform.hpp>

#include "pipeline_product.hpp"
#include <woki/gfx/handles.hpp>

namespace woki::gfx {

enum class PipelineState : u8 { Empty, Ready };

struct PipelineRecord {
    PipelineState state{PipelineState::Empty};
    u64 version{};
    ContentHash product_hash;
};

struct PipelineGeneration {
    RenderPipelineIR pipeline;
    u64 version{};
    ContentHash product_hash;
};

struct BorrowedPipeline {
    ref<const PipelineGeneration> generation;

    [[nodiscard]] const RenderPipelineIR& Get() const noexcept {
        return generation->pipeline;
    }

    [[nodiscard]] u64 Version() const noexcept {
        return generation->version;
    }
};

struct PipelineInstance {
    BorrowedPipeline pipeline;
    std::vector<ref<const RenderFeature>> features;
};

// Declares features in the canonical order stored by the compiled pipeline.
[[nodiscard]] Result<void> DeclarePipelineGraph(
    const PipelineInstance& instance,
    RenderGraphBuilder& graph,
    u32 width,
    u32 height
);
[[nodiscard]] Result<void> PreparePipelineScene(
    const PipelineInstance& instance,
    const RenderScenePreparationContext& context
);

class RenderPipelineLibrary {
public:
    [[nodiscard]] PipelineHandle Create();
    [[nodiscard]] Result<void> Destroy(PipelineHandle handle);
    [[nodiscard]] Result<void> Publish(PipelineHandle handle, const asset::Product& product);
    [[nodiscard]] Result<BorrowedPipeline> Borrow(PipelineHandle handle) const;
    [[nodiscard]] std::optional<PipelineRecord> Record(PipelineHandle handle) const;
    [[nodiscard]] Result<BorrowedPipeline> SelectSupported(PipelineHandle root, CapabilitySet capabilities) const;
    [[nodiscard]] Result<PipelineInstance> Compose(
        PipelineHandle root,
        CapabilitySet capabilities,
        const FeatureRegistry& registry,
        const FeatureServices& services = {}
    ) const;

private:
    struct Slot {
        u32 generation{1};
        PipelineRecord record;
        ref<const PipelineGeneration> current;
        bool occupied{};
    };

    std::vector<Slot> slots_;
    std::map<asset::AssetId, PipelineHandle> assets_;
    std::vector<u32> free_;
};

class PipelineDependencyGraph {
public:
    void Replace(PipelineHandle pipeline, std::span<const asset::AssetPath> dependencies);
    [[nodiscard]] std::vector<PipelineHandle> Dependents(const asset::AssetPath& dependency) const;

private:
    std::map<asset::AssetPath, std::set<PipelineHandle>> reverse_;
    std::map<PipelineHandle, std::vector<asset::AssetPath>> forward_;
};

struct PipelineReloadCandidate {
    PipelineHandle handle;
    u64 base_version{};
    Result<asset::Product> product;
};

enum class PipelineReloadOutcome : u8 { Published, Rejected, Stale };

struct PipelineReloadEvent {
    PipelineHandle handle;
    PipelineReloadOutcome outcome{PipelineReloadOutcome::Rejected};
    u64 version{};
    std::string message;
};

class PipelineReloadCoordinator {
public:
    PipelineReloadCoordinator(RenderPipelineLibrary& library, PipelineDependencyGraph& graph)
        : library_(library),
          graph_(graph) {}

    [[nodiscard]] PipelineReloadEvent Publish(PipelineReloadCandidate candidate);
    [[nodiscard]] std::vector<PipelineHandle> Invalidate(const asset::AssetPath& dependency);

private:
    RenderPipelineLibrary& library_;
    PipelineDependencyGraph& graph_;
};

} // namespace woki::gfx
