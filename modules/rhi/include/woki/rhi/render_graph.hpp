#pragma once

#include <unordered_map>

#include <woki/core.hpp>

#include "render_graph/builder.hpp"
#include "render_graph/context.hpp"
#include "render_graph/internal.hpp"
#include "render_graph/resources.hpp"
#include "render_graph/bind_group_builder.hpp"

namespace woki::rhi {

class RenderGraphFrame final {
public:
    RenderGraphFrame(RenderGraphFrame&& other) noexcept;
    RenderGraphFrame& operator=(RenderGraphFrame&& other) noexcept;
    RenderGraphFrame(const RenderGraphFrame&) = delete;
    RenderGraphFrame& operator=(const RenderGraphFrame&) = delete;
    ~RenderGraphFrame();

    void Bind(PerFrameSlot slot, ref<TextureView> view);
    [[nodiscard]] Result<void> Execute();

private:
    friend class RenderGraph;

    RenderGraphFrame(ref<RenderGraph> graph, u32 width, u32 height);
    void ReleaseFrame() noexcept;

    ref<RenderGraph> graph_{};
    u32 width_{0};
    u32 height_{0};

    scope<CommandEncoder> encoder_{};
    std::unordered_map<u32, ref<TextureView>> per_frame_views_{};
    bool executed_{false};
};

class RenderGraph final : public ref_from_this<RenderGraph> {
private:
    struct ConstructionKey final {};

public:
    RenderGraph(ConstructionKey, ref<Device> device, render_graph::detail::GraphBlueprint blueprint, u32 width, u32 height);

    [[nodiscard]] static Result<ref<RenderGraph>> Create(ref<Device> device, render_graph::detail::GraphBlueprint blueprint, u32 width, u32 height);

    [[nodiscard]] Result<RenderGraphFrame> BeginFrame(u32 width, u32 height);
    [[nodiscard]] Result<void> RebuildForResize(u32 width, u32 height);

private:
    friend class RenderGraphBuilder;
    friend class RenderGraphFrame;

    struct RuntimeResource final {
        render_graph::detail::ResourceRecord blueprint{};
        scope<Texture> texture{};
        scope<TextureView> view{};
        scope<TextureView> depth_sample_view{};
        u32 pool_index{kInvalidGraphResource};
    };

    [[nodiscard]] Result<void> AllocateRuntimeResources(u32 width, u32 height);
    void ReleaseTransientPool();
    [[nodiscard]] Result<void> AcquireTransientResource(RuntimeResource& runtime, u32 width, u32 height);
    [[nodiscard]] Texture* ResolveTexture(u32 resource_id);
    [[nodiscard]] TextureView* ResolveView(u32 resource_id);
    [[nodiscard]] TextureView* ResolveSampleView(u32 resource_id, SampleMode mode);
    [[nodiscard]] Result<void> ExecuteRenderPass(u32 pass_index, CommandEncoder& encoder, u32 width, u32 height, const std::unordered_map<u32, ref<TextureView>>& per_frame_views);
    [[nodiscard]] Result<void> ExecuteCopyPass(u32 pass_index, CommandEncoder& encoder, u32 width, u32 height);

    ref<Device> device_{};
    render_graph::detail::GraphBlueprint blueprint_{};
    u32 width_{0};
    u32 height_{0};
    std::vector<RuntimeResource> runtime_resources_{};
    std::vector<render_graph::detail::PooledTransientTexture> transient_pool_{};
    u32 active_frame_count_{0};
};

} // namespace woki::rhi
