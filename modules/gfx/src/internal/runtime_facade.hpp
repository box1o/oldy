#pragma once

#include <functional>

#include <woki/gfx/compute.hpp>
#include <woki/gfx/readback.hpp>
#include <woki/gfx/advanced/render_graph.hpp>
#include <woki/gfx/advanced/graph_executor.hpp>

namespace woki::gfx {

class LayoutCache;
class PipelineCache;
class ShaderLibrary;
class DeferredReleaseQueue;

struct RuntimeFacadeGraph final {
    RenderGraphBuilder builder;
    u32 width{1};
    u32 height{1};
    bool has_work{};
};

struct ResolvedReadbackTexture final {
    ref<rhi::Texture> texture;
    GraphTextureDesc descriptor;
    PixelFormat format{PixelFormat::RGBA8Unorm};
};

struct ResolvedReadbackBuffer final {
    ref<rhi::Buffer> buffer;
    GraphBufferDesc descriptor;
};

struct RuntimeFacadeAccess final {
    static void AttachComputeReleases(ComputeService& service, ref<DeferredReleaseQueue> releases);
    static Result<void> AppendCompute(ComputeService& service, RuntimeFacadeGraph& graph, rhi::Device& device, asset::AssetManager& assets, LayoutCache& layouts, PipelineCache& pipelines, rhi::SubmissionEpoch completed);
    static Result<void> AppendReadbacks(ReadbackService& service,
        ComputeService& compute,
        RuntimeFacadeGraph& graph,
        rhi::Device& device,
        const std::function<Result<ResolvedReadbackTexture>(OffscreenTargetHandle)>& resolve_offscreen);
    static Result<ResolvedReadbackBuffer> ResolveBuffer(ComputeService& service, rhi::Device& device, BufferHandle handle);
    static Result<ResolvedReadbackTexture> ResolveTexture(ComputeService& service, rhi::Device& device, GpuTextureHandle handle);
    static std::vector<GraphPass> ComputeDependencies(const ComputeService& service);
    static Result<void> BindCompute(ComputeService& service, GraphFrame& frame);
    static Result<void> BindReadbacks(ReadbackService& service, GraphFrame& frame);
    static void PublishCompute(ComputeService& service, LayoutCache& layouts, PipelineCache& pipelines, rhi::SubmissionTicket submission);
    static void PublishReadbacks(ReadbackService& service, rhi::SubmissionTicket submission);
    static void FailCompute(ComputeService& service, const Error& error);
    static void FailReadbacks(ReadbackService& service, const Error& error);
    static void PollCompute(ComputeService& service, rhi::SubmissionEpoch completed);
    static void PollReadbacks(ReadbackService& service, rhi::SubmissionEpoch completed);
    static void DeviceLostCompute(ComputeService& service) noexcept;
    static void DeviceLostReadbacks(ReadbackService& service) noexcept;
    static void CancelCompute(ComputeService& service) noexcept;
    static void CancelReadbacks(ReadbackService& service) noexcept;
};

} // namespace woki::gfx
