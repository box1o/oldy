#pragma once

#include <string_view>

#include "graph_compile.hpp"
#include "deferred_release.hpp"

namespace woki::gfx {

// Narrow recording seam for executor tests and non-RHI command integrations.
// Resource validation and callback dispatch remain owned by GraphExecutor.
class GraphCommandBackend {
public:
    virtual ~GraphCommandBackend() = default;
    [[nodiscard]] virtual Result<void> BeginFrame(u32 width, u32 height) = 0;
    [[nodiscard]] virtual Result<void> BeginPass(std::string_view name, PassKind kind) = 0;
    virtual void EndPass() noexcept = 0;
    [[nodiscard]] virtual Result<void> Copy(std::string_view pass_name) = 0;
    [[nodiscard]] virtual Result<void> Finish() = 0;
    [[nodiscard]] virtual Result<rhi::SubmissionTicket> Submit() = 0;
    [[nodiscard]] virtual rhi::SubmissionEpoch CompletedSubmission() const noexcept = 0;
};

// Native integrations can implement timestamps through ordinary RHI query sets.
// The graph never reaches through to a backend API; pass labels are always emitted
// by RHI render/compute descriptors even when timestamps are unavailable.
class GraphGpuProfiler {
public:
    virtual ~GraphGpuProfiler() = default;
    virtual void BeginFrame(rhi::CommandEncoder&) = 0;
    virtual void BeginPass(rhi::CommandEncoder&, std::string_view, PassKind) = 0;
    virtual void EndPass(rhi::CommandEncoder&, std::string_view, PassKind) = 0;
    virtual void EndFrame(rhi::CommandEncoder&) = 0;
};

struct GraphTextureBinding final {
    GraphTexture resource;
    ref<rhi::Texture> texture;
    ref<rhi::TextureView> view;
    GraphTextureDesc signature;
    GraphTextureViewDesc view_signature;
};

struct GraphBufferBinding final {
    GraphBuffer resource;
    ref<rhi::Buffer> buffer;
    GraphBufferDesc signature;
};

// Two-slot temporal history. Omitted previous bindings use the last successfully
// submitted current slot; current becomes previous only after Queue::Submit succeeds.
struct GraphTemporalTextureBinding final {
    GraphTemporalTexture resource;
    ref<rhi::Texture> previous_texture;
    ref<rhi::TextureView> previous_view;
    ref<rhi::Texture> current_texture;
    ref<rhi::TextureView> current_view;
    GraphTextureDesc signature;
    GraphTextureViewDesc view_signature;
};

class GraphFrame final {
public:
    GraphFrame(GraphFrame&&) noexcept;
    GraphFrame& operator=(GraphFrame&&) noexcept;
    GraphFrame(const GraphFrame&) = delete;
    GraphFrame& operator=(const GraphFrame&) = delete;
    ~GraphFrame();

    [[nodiscard]] Result<void> Bind(GraphTextureBinding binding);
    [[nodiscard]] Result<void> Bind(GraphBufferBinding binding);
    [[nodiscard]] Result<void> Bind(GraphTemporalTextureBinding binding);
    [[nodiscard]] Result<rhi::SubmissionTicket> Execute();

private:
    friend class GraphExecutor;
    explicit GraphFrame(std::shared_ptr<graph_detail::ExecutorState> state);
    void Release() noexcept;
    std::shared_ptr<graph_detail::ExecutorState> state_;
    bool executed_{};
};

// Owns a bounded pool of physical transient sets. A submitted set is reused only
// after the backend completion watermark reaches its ticket. Recording is owner-thread
// serialized; failed callbacks end an open pass, discard the encoder and never submit.
// Supply a shared release queue when replacing executors before device teardown; the
// default executor-owned queue assumes pending work is idle when the executor is destroyed.
class GraphExecutor final {
public:
    GraphExecutor(ref<rhi::Device> device, CompiledRenderGraph graph, ref<DeferredReleaseQueue> releases = {});
    GraphExecutor(std::shared_ptr<GraphCommandBackend> backend, CompiledRenderGraph graph, ref<DeferredReleaseQueue> releases = {});
    GraphExecutor(GraphExecutor&&) = delete;
    GraphExecutor& operator=(GraphExecutor&&) = delete;
    ~GraphExecutor();

    [[nodiscard]] Result<GraphFrame> Begin(u32 width, u32 height);

    [[nodiscard]] const CompiledRenderGraph& Graph() const noexcept;

private:
    friend class GraphFrame;
    [[nodiscard]] static Result<void> Bind(const std::shared_ptr<graph_detail::ExecutorState>& state, GraphTextureBinding binding);
    [[nodiscard]] static Result<void> Bind(const std::shared_ptr<graph_detail::ExecutorState>& state, GraphBufferBinding binding);
    [[nodiscard]] static Result<void> Bind(const std::shared_ptr<graph_detail::ExecutorState>& state, GraphTemporalTextureBinding binding);
    [[nodiscard]] static Result<rhi::SubmissionTicket> ExecuteFrame(const std::shared_ptr<graph_detail::ExecutorState>& state);
    static void RetireFrame(const std::shared_ptr<graph_detail::ExecutorState>& state, rhi::SubmissionTicket submission = {}) noexcept;
    std::shared_ptr<graph_detail::ExecutorState> state_;
};

} // namespace woki::gfx
