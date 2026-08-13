#pragma once

#include <map>
#include <functional>
#include <thread>

#include "deferred_release.hpp"
#include "material.hpp"
#include "texture_cache.hpp"

namespace woki::gfx {

struct PipelineTargetSignature final {
    std::vector<rhi::TextureFormat> colors;
    rhi::TextureFormat depth{rhi::TextureFormat::Undefined};
    u32 samples{1};
    [[nodiscard]] friend auto operator<=>(const PipelineTargetSignature&, const PipelineTargetSignature&) noexcept = default;
};

struct PipelineOverride final {
    u32 id{};
    u64 bits{};
    [[nodiscard]] friend auto operator<=>(const PipelineOverride&, const PipelineOverride&) noexcept = default;
};

struct GraphicsPipelineKey final {
    ContentHash shader_product;
    ContentHash shader_variant;
    u64 shader_generation{};
    StringId vertex_entry;
    StringId fragment_entry;
    u64 vertex_schema_id{};
    ContentHash render_state;
    PipelineTargetSignature targets;
    ContentHash pipeline_layout;
    std::vector<PipelineOverride> overrides;
    [[nodiscard]] friend auto operator<=>(const GraphicsPipelineKey&, const GraphicsPipelineKey&) noexcept = default;
};

struct ComputePipelineKey final {
    ContentHash shader_product;
    ContentHash shader_variant;
    u64 shader_generation{};
    StringId compute_entry;
    ContentHash pipeline_layout;
    std::vector<PipelineOverride> overrides;
    [[nodiscard]] friend auto operator<=>(const ComputePipelineKey&, const ComputePipelineKey&) noexcept = default;
};
enum class PipelineRequestState : u8 { Pending, Ready, Failed };

template <typename Pipeline>
struct PipelineRequest final {
    PipelineRequestState state{PipelineRequestState::Pending};
    ref<Pipeline> pipeline;
    std::string diagnostic;
};

class PipelineCache final {
public:
    using GraphicsFactory = std::function<Result<ref<rhi::RenderPipeline>>()>;
    using ComputeFactory = std::function<Result<ref<rhi::ComputePipeline>>()>;

    // Every API mutates cache or retirement state and must run on the constructing thread.
    PipelineCache()
        : owner_(std::this_thread::get_id()) {}

    [[nodiscard]] Result<PipelineRequest<rhi::RenderPipeline>> Request(GraphicsPipelineKey key, GraphicsFactory create);
    [[nodiscard]] Result<PipelineRequest<rhi::ComputePipeline>> Request(ComputePipelineKey key, ComputeFactory create);
    [[nodiscard]] PipelineRequest<rhi::RenderPipeline> Find(const GraphicsPipelineKey& key) const;
    [[nodiscard]] PipelineRequest<rhi::ComputePipeline> Find(const ComputePipelineKey& key) const;
    [[nodiscard]] Result<void> MarkUsed(GraphicsPipelineKey key, rhi::SubmissionTicket submission);
    [[nodiscard]] Result<void> MarkUsed(ComputePipelineKey key, rhi::SubmissionTicket submission);
    [[nodiscard]] Result<size_t> InvalidateShader(ContentHash shader_product, DeferredReleaseQueue& releases);

    void Clear() noexcept {
        graphics_.clear();
        compute_.clear();
    }

private:
    template <typename T>
    struct Entry {
        PipelineRequest<T> request;
        rhi::SubmissionTicket last_used;
    };

    std::thread::id owner_;
    std::map<GraphicsPipelineKey, Entry<rhi::RenderPipeline>> graphics_;
    std::map<ComputePipelineKey, Entry<rhi::ComputePipeline>> compute_;
};

} // namespace woki::gfx
