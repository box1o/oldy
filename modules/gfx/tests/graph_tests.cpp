#include <random>
#include <catch2/catch_test_macros.hpp>

#include <woki/gfx/advanced.hpp>
#include <woki/rhi.hpp>

using namespace woki;

namespace {

class FakeCommandBackend final : public gfx::GraphCommandBackend {
public:
    Result<void> BeginFrame(u32, u32) override {
        events.emplace_back("frame");
        return Ok();
    }

    Result<void> BeginPass(std::string_view name, gfx::PassKind kind) override {
        events.push_back((kind == gfx::PassKind::Render ? "render:" : "compute:") + std::string(name));
        return Ok();
    }

    void EndPass() noexcept override {
        events.emplace_back("end");
    }

    Result<void> Copy(std::string_view name) override {
        events.emplace_back("copy:" + std::string(name));
        return Ok();
    }

    Result<void> Finish() override {
        events.emplace_back("finish");
        return Ok();
    }

    Result<rhi::SubmissionTicket> Submit() override {
        events.emplace_back("submit");
        if (!submit_succeeds)
            return Err(ErrorCode::InvalidState, "fake submit failure");
        const rhi::SubmissionTicket ticket(++submitted);
        if (complete_immediately)
            completed = rhi::SubmissionEpoch(ticket.Value());
        return Ok(ticket);
    }

    rhi::SubmissionEpoch CompletedSubmission() const noexcept override {
        return completed;
    }

    std::vector<std::string> events;
    bool submit_succeeds{true};
    bool complete_immediately{true};
    u64 submitted{};
    rhi::SubmissionEpoch completed;
};

class FakeTextureView final : public rhi::TextureView {
public:
    void SetLabel(std::string_view) override {}

    rhi::NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }
};

class FakeTexture final : public rhi::Texture {
public:
    explicit FakeTexture(gfx::GraphTextureDesc descriptor)
        : descriptor_(std::move(descriptor)) {}

    scope<rhi::TextureView> CreateErrorView(const rhi::TextureViewDesc&) const override {
        return std::make_unique<FakeTextureView>();
    }

    scope<rhi::TextureView> CreateView(const rhi::TextureViewDesc&) const override {
        return std::make_unique<FakeTextureView>();
    }

    void Destroy() override {}

    u32 GetDepthOrArrayLayers() const override {
        return descriptor_.depth_or_layers;
    }

    rhi::TextureDimension GetDimension() const override {
        return descriptor_.dimension;
    }

    rhi::TextureFormat GetFormat() const override {
        return descriptor_.format;
    }

    u32 GetHeight() const override {
        return descriptor_.extent.height;
    }

    u32 GetMipLevelCount() const override {
        return descriptor_.mip_levels;
    }

    u32 GetSampleCount() const override {
        return descriptor_.sample_count;
    }

    rhi::TextureViewDimension GetTextureBindingViewDimension() const override {
        return rhi::TextureViewDimension::e2D;
    }

    rhi::TextureUsage GetUsage() const override {
        return descriptor_.usage;
    }

    u32 GetWidth() const override {
        return descriptor_.extent.width;
    }

    void Pin(rhi::TextureUsage) override {}

    void SetLabel(std::string_view) override {}

    void SetOwnershipForMemoryDump(u64) override {}

    void Unpin() override {}

    rhi::NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }

private:
    gfx::GraphTextureDesc descriptor_;
};

class FakeBuffer final : public rhi::Buffer {
public:
    FakeBuffer(u64 size, rhi::BufferUsage usage)
        : size_(size),
          usage_(usage) {}

    Result<scope<rhi::TexelBufferView>> CreateTexelView(const rhi::TexelBufferViewDesc&) const override {
        return Err(ErrorCode::InvalidState, "unused");
    }

    void Destroy() override {}

    const void* GetConstMappedRange(size_t, size_t) const override {
        return nullptr;
    }

    void* GetMappedRange(size_t, size_t) override {
        return nullptr;
    }

    rhi::BufferMapState GetMapState() const override {
        return rhi::BufferMapState::Unmapped;
    }

    u64 GetSize() const override {
        return size_;
    }

    rhi::BufferUsage GetUsage() const override {
        return usage_;
    }

    rhi::Future MapAsync(rhi::MapMode, size_t, size_t, rhi::CallbackMode, rhi::MapAsyncCallback) const override {
        return {};
    }

    Result<void> ReadMappedRange(size_t, void*, size_t) const override {
        return Err(ErrorCode::InvalidState, "unused");
    }

    void SetLabel(std::string_view) override {}

    void Unmap() override {}

    Result<void> WriteMappedRange(size_t, const void*, size_t) override {
        return Err(ErrorCode::InvalidState, "unused");
    }

    rhi::NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }

private:
    u64 size_{};
    rhi::BufferUsage usage_{};
};

class FakeCommandBuffer final : public rhi::CommandBuffer {
public:
    void SetLabel(std::string_view) override {}

    rhi::NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }
};

class FakeQueue final : public rhi::Queue {
public:
    Result<void> CopyExternalTextureForBrowser(const rhi::ImageCopyExternalTexture&, const rhi::TexelCopyTextureInfo&, const rhi::Extent3D&, const rhi::CopyTextureForBrowserOptions&) const override {
        return Ok();
    }

    Result<void> CopyTextureForBrowser(const rhi::TexelCopyTextureInfo&, const rhi::TexelCopyTextureInfo&, const rhi::Extent3D&, const rhi::CopyTextureForBrowserOptions&) const override {
        return Ok();
    }

    rhi::Future OnSubmittedWorkDone(rhi::CallbackMode, rhi::QueueWorkDoneCallback) const override {
        return {};
    }

    void SetLabel(std::string_view) const override {}

    Result<rhi::SubmissionTicket> Submit(std::span<rhi::CommandBuffer* const>) const override {
        const rhi::SubmissionTicket ticket(++submitted_);
        completed_ = rhi::SubmissionEpoch(ticket.Value());
        return Ok(ticket);
    }

    rhi::SubmissionEpoch CompletedSubmission() const noexcept override {
        return completed_;
    }

    rhi::SubmissionTrackingStatus SubmissionTracking() const noexcept override {
        return rhi::SubmissionTrackingStatus::Healthy;
    }

    Result<void> WriteBuffer(const rhi::Buffer&, u64, const void*, u64) const override {
        return Ok();
    }

    Result<void> WriteTexture(const rhi::TexelCopyTextureInfo&, const void*, u64, const rhi::TexelCopyBufferLayout&, const rhi::Extent3D&) const override {
        return Ok();
    }

    rhi::NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }

private:
    mutable u64 submitted_{};
    mutable rhi::SubmissionEpoch completed_;
};

class FakeComputePassEncoder final : public rhi::ComputePassEncoder {
public:
    void DispatchWorkgroups(u32, u32, u32) override {}

    void DispatchWorkgroupsIndirect(const rhi::Buffer&, u64) override {}

    void End() override {}

    void InsertDebugMarker(std::string_view) override {}

    void PopDebugGroup() override {}

    void PushDebugGroup(std::string_view) override {}

    void SetBindGroup(u32, const rhi::BindGroup*, std::span<const u32>) override {}

    void SetImmediates(u32, const void*, size_t) override {}

    void SetLabel(std::string_view) override {}

    void SetPipeline(const rhi::ComputePipeline&) override {}

    void SetResourceTable(const rhi::ResourceTable*) override {}

    void WriteTimestamp(const rhi::QuerySet&, u32) override {}

    rhi::NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }
};

class FakeCommandEncoder final : public rhi::CommandEncoder {
public:
    Result<scope<rhi::ComputePassEncoder>> BeginComputePass(const rhi::ComputePassDesc&) override {
        return Ok(scope<rhi::ComputePassEncoder>(std::make_unique<FakeComputePassEncoder>()));
    }

    Result<scope<rhi::RenderPassEncoder>> BeginRenderPass(const rhi::RenderPassDesc&) override {
        return Err(ErrorCode::InvalidState, "unused");
    }

    Result<scope<rhi::RenderPassEncoder>> BeginRenderPass(const rhi::RenderPassDescTyped&) override {
        return Err(ErrorCode::InvalidState, "unused");
    }

    Result<void> ClearBuffer(const rhi::Buffer&, u64, u64) override {
        return Ok();
    }

    Result<void> CopyBufferToBuffer(const rhi::Buffer&, u64, const rhi::Buffer&, u64, u64) override {
        return Ok();
    }

    Result<void> CopyBufferToTexture(const rhi::TexelCopyBufferInfo&, const rhi::TexelCopyTextureInfo&, const rhi::Extent3D&) override {
        return Ok();
    }

    Result<void> CopyTextureToBuffer(const rhi::TexelCopyTextureInfo&, const rhi::TexelCopyBufferInfo&, const rhi::Extent3D&) override {
        return Ok();
    }

    Result<void> CopyTextureToTexture(const rhi::TexelCopyTextureInfo&, const rhi::TexelCopyTextureInfo&, const rhi::Extent3D&) override {
        return Ok();
    }

    Result<scope<rhi::CommandBuffer>> Finish(const rhi::CommandBufferDesc&) override {
        return Ok(scope<rhi::CommandBuffer>(std::make_unique<FakeCommandBuffer>()));
    }

    void InjectValidationError(std::string_view) override {}

    void InsertDebugMarker(std::string_view) override {}

    void PopDebugGroup() override {}

    void PushDebugGroup(std::string_view) override {}

    Result<void> ResolveQuerySet(const rhi::QuerySet&, u32, u32, const rhi::Buffer&, u64) override {
        return Ok();
    }

    void SetLabel(std::string_view) override {}

    Result<void> WriteBuffer(const rhi::Buffer&, u64, const u8*, u64) override {
        return Ok();
    }

    Result<void> WriteTimestamp(const rhi::QuerySet&, u32) override {
        return Ok();
    }

    rhi::NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }
};

class FakeDevice final : public rhi::Device {
public:
    Result<scope<rhi::BindGroup>> CreateBindGroup(const rhi::BindGroupDesc&) override {
        return Unsupported<rhi::BindGroup>();
    }

    Result<scope<rhi::BindGroupLayout>> CreateBindGroupLayout(const rhi::BindGroupLayoutDesc&) override {
        return Unsupported<rhi::BindGroupLayout>();
    }

    Result<scope<rhi::Buffer>> CreateBuffer(const rhi::BufferDesc& descriptor) override {
        ++buffer_creations;
        return Ok(scope<rhi::Buffer>(std::make_unique<FakeBuffer>(descriptor.size, descriptor.usage)));
    }

    Result<scope<rhi::CommandEncoder>> CreateCommandEncoder(const rhi::CommandEncoderDesc&) override {
        return Ok(scope<rhi::CommandEncoder>(std::make_unique<FakeCommandEncoder>()));
    }

    Result<scope<rhi::ComputePipeline>> CreateComputePipeline(const rhi::ComputePipelineDesc&) override {
        return Unsupported<rhi::ComputePipeline>();
    }

    rhi::Future CreateComputePipelineAsync(const rhi::ComputePipelineDesc&, rhi::CallbackMode, rhi::CreateComputePipelineCallback) override {
        return {};
    }

    Result<scope<rhi::Buffer>> CreateErrorBuffer(const rhi::BufferDesc&) override {
        return Unsupported<rhi::Buffer>();
    }

    Result<scope<rhi::ExternalTexture>> CreateErrorExternalTexture() override {
        return Unsupported<rhi::ExternalTexture>();
    }

    Result<scope<rhi::ShaderModule>> CreateErrorShaderModule(const rhi::ShaderModuleDesc&, std::string_view) override {
        return Unsupported<rhi::ShaderModule>();
    }

    Result<scope<rhi::Texture>> CreateErrorTexture(const rhi::TextureDesc&) override {
        return Unsupported<rhi::Texture>();
    }

    Result<scope<rhi::ExternalTexture>> CreateExternalTexture(const rhi::ExternalTextureDesc&) override {
        return Unsupported<rhi::ExternalTexture>();
    }

    Result<scope<rhi::PipelineLayout>> CreatePipelineLayout(const rhi::PipelineLayoutDesc&) override {
        return Unsupported<rhi::PipelineLayout>();
    }

    Result<scope<rhi::QuerySet>> CreateQuerySet(const rhi::QuerySetDesc&) override {
        return Unsupported<rhi::QuerySet>();
    }

    Result<scope<rhi::RenderBundleEncoder>> CreateRenderBundleEncoder(const rhi::RenderBundleEncoderDesc&) override {
        return Unsupported<rhi::RenderBundleEncoder>();
    }

    Result<scope<rhi::RenderPipeline>> CreateRenderPipeline(const rhi::RenderPipelineDesc&) override {
        return Unsupported<rhi::RenderPipeline>();
    }

    Result<scope<rhi::RenderPipeline>> CreateRenderPipeline(const rhi::RenderPipelineDescTyped&) override {
        return Unsupported<rhi::RenderPipeline>();
    }

    rhi::Future CreateRenderPipelineAsync(const rhi::RenderPipelineDesc&, rhi::CallbackMode, rhi::CreateRenderPipelineCallback) override {
        return {};
    }

    Result<scope<rhi::ResourceTable>> CreateResourceTable(const rhi::ResourceTableDesc&) override {
        return Unsupported<rhi::ResourceTable>();
    }

    Result<scope<rhi::Sampler>> CreateSampler(const rhi::SamplerDesc&) override {
        return Unsupported<rhi::Sampler>();
    }

    Result<scope<rhi::ShaderModule>> CreateShaderModule(const rhi::ShaderModuleDesc&) override {
        return Unsupported<rhi::ShaderModule>();
    }

    Result<scope<rhi::Texture>> CreateTexture(const rhi::TextureDesc&) override {
        return Unsupported<rhi::Texture>();
    }

    Result<scope<rhi::Swapchain>> CreateSwapchain(ref<rhi::Surface>, rhi::SwapchainDesc) override {
        return Unsupported<rhi::Swapchain>();
    }

    void Destroy() override {}

    void ForceLoss(rhi::DeviceLostReason, std::string_view) override {}

    Result<scope<rhi::Adapter>> GetAdapter() const override {
        return Unsupported<rhi::Adapter>();
    }

    Result<void> GetAdapterInfo(rhi::AdapterInfo&) const override {
        return Err(ErrorCode::InvalidState, "unused");
    }

    Result<void> GetAHardwareBufferProperties(void*, void*) const override {
        return Err(ErrorCode::InvalidState, "unused");
    }

    void GetFeatures(rhi::SupportedFeatures&) const override {}

    Result<void> GetLimits(rhi::Limits&) const override {
        return Err(ErrorCode::InvalidState, "unused");
    }

    rhi::Future GetLostFuture() const override {
        return {};
    }

    rhi::Queue& GetQueue() const noexcept override {
        return queue_;
    }

    bool HasFeature(rhi::FeatureName) const noexcept override {
        return false;
    }

    Result<scope<rhi::SharedBufferMemory>> ImportSharedBufferMemory(const rhi::SharedBufferMemoryDesc&) override {
        return Unsupported<rhi::SharedBufferMemory>();
    }

    Result<scope<rhi::SharedFence>> ImportSharedFence(const rhi::SharedFenceDesc&) override {
        return Unsupported<rhi::SharedFence>();
    }

    Result<scope<rhi::SharedTextureMemory>> ImportSharedTextureMemory(const rhi::SharedTextureMemoryDesc&) override {
        return Unsupported<rhi::SharedTextureMemory>();
    }

    void InjectError(rhi::ErrorType, std::string_view) override {}

    rhi::Future PopErrorScope(rhi::CallbackMode, rhi::PopErrorScopeCallback) const override {
        return {};
    }

    void PushErrorScope(rhi::ErrorFilter) override {}

    void SetLabel(std::string_view) override {}

    void SetLoggingCallback(rhi::LoggingCallback) override {}

    void Tick() const noexcept override {}

    void ValidateTextureDescriptor(const rhi::TextureDesc&) const override {}

    rhi::NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }

    u32 buffer_creations{};

private:
    template <typename T>
    static Result<scope<T>> Unsupported() {
        return Err(ErrorCode::InvalidState, "unused");
    }

    mutable FakeQueue queue_;
};

gfx::GraphTextureDesc Texture(std::string label = "texture") {
    gfx::GraphTextureDesc value;
    value.label = std::move(label);
    value.extent = gfx::GraphExtent::Fixed(64, 64);
    value.format = rhi::TextureFormat::RGBA8Unorm;
    return value;
}

gfx::GraphExecuteCallback NoOp() {
    return [](gfx::RenderGraphContext&) { return Ok(); };
}

} // namespace

TEST_CASE("Render graph builds deterministic RAW schedule and dumps") {
    auto build = [] {
        gfx::RenderGraphBuilder builder;
        const auto texture = builder.CreateTexture(Texture());
        auto writer = builder.AddPass("writer", gfx::PassKind::Compute);
        const auto version = writer.Write(texture);
        writer.Execute(NoOp());
        auto reader = builder.AddPass("reader", gfx::PassKind::Compute);
        reader.Read(version).SideEffect("observable").Execute(NoOp());
        return builder.Compile(64, 64);
    };
    auto first = build();
    auto second = build();
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(first->Passes().size() == 2);
    CHECK(first->Passes()[0].name == "writer");
    CHECK(first->Dependencies().size() == 1);
    CHECK(first->Dependencies()[0].reason.starts_with("RAW"));
    CHECK(first->DeterministicHash() == second->DeterministicHash());
    CHECK(first->DumpJson() == second->DumpJson());
    CHECK(first->DumpDot().find("writer") != std::string::npos);
    CHECK(first->DumpMermaid().find("flowchart") != std::string::npos);
}

TEST_CASE("Render graph rejects read before write and cross graph handles") {
    gfx::RenderGraphBuilder first;
    const auto texture = first.CreateTexture(Texture());
    const auto missing = first.Initial(texture);
    auto read = first.AddPass("read", gfx::PassKind::Compute);
    read.Read(missing).SideEffect("root").Execute(NoOp());
    auto invalid = first.Compile(64, 64);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().Message().find("GRF1003") != std::string_view::npos);

    gfx::RenderGraphBuilder left;
    gfx::RenderGraphBuilder right;
    const auto left_texture = left.CreateTexture(Texture("left"));
    auto left_pass = left.AddPass("left", gfx::PassKind::Compute);
    const auto left_version = left_pass.Write(left_texture);
    left_pass.SideEffect("left").Execute(NoOp());
    auto right_pass = right.AddPass("right", gfx::PassKind::Compute);
    right_pass.Read(left_version).SideEffect("right").Execute(NoOp());
    auto cross = right.Compile(64, 64);
    REQUIRE_FALSE(cross);
    CHECK(cross.error().Message().find("GRF1002") != std::string_view::npos);
}

TEST_CASE("Render graph culls dead passes and emits queue waves") {
    gfx::RenderGraphBuilder builder;
    const auto a = builder.CreateTexture(Texture("a"));
    const auto b = builder.CreateTexture(Texture("b"));
    auto dead = builder.AddPass("dead", gfx::PassKind::Compute);
    static_cast<void>(dead.Write(a));
    dead.Execute(NoOp());
    auto live = builder.AddPass("live", gfx::PassKind::Compute);
    static_cast<void>(live.Write(b));
    live.Queue(gfx::QueuePreference::Compute, true).SideEffect("root").Execute(NoOp());
    auto graph = builder.Compile(64, 64);
    REQUIRE(graph);
    REQUIRE(graph->Passes().size() == 1);
    CHECK(graph->Passes()[0].name == "live");
    CHECK(graph->Passes()[0].queue == gfx::QueueClass::Compute);
    REQUIRE(graph->Waves().size() == 1);
}

TEST_CASE("Render graph detects explicit cycles and blackboard duplicates") {
    gfx::RenderGraphBuilder builder;
    auto a = builder.AddPass("a", gfx::PassKind::Compute);
    auto b = builder.AddPass("b", gfx::PassKind::Compute);
    a.DependsOn(b.Handle()).SideEffect("a").Execute(NoOp());
    b.DependsOn(a.Handle()).SideEffect("b").Execute(NoOp());
    auto graph = builder.Compile(64, 64);
    REQUIRE_FALSE(graph);
    CHECK(graph.error().Message().find("GRF1016") != std::string_view::npos);

    gfx::GraphBlackboard blackboard;
    REQUIRE(blackboard.Emplace<int>(7));
    CHECK(*blackboard.Get<int>() == 7);
    CHECK_FALSE(blackboard.Emplace<int>(9));
}

TEST_CASE("Render graph keeps ordered repeated MRT formats") {
    gfx::RenderGraphBuilder builder;
    const auto first = builder.CreateTexture(Texture("first"));
    const auto second = builder.CreateTexture(Texture("second"));
    auto pass = builder.AddPass("mrt", gfx::PassKind::Render);
    gfx::ColorAttachment slot0;
    gfx::ColorAttachment slot1;
    slot1.slot = 1;
    const auto a = pass.Color(first, slot0);
    const auto b = pass.Color(second, slot1);
    pass.Execute(NoOp());
    REQUIRE(builder.Export(a));
    REQUIRE(builder.Export(b));
    auto graph = builder.Compile(64, 64);
    REQUIRE(graph);
    CHECK(graph->Passes().size() == 1);
}

TEST_CASE("Render graph handles carry graph identity") {
    gfx::RenderGraphBuilder first;
    gfx::RenderGraphBuilder second;
    const auto a = first.CreateTexture(Texture());
    const auto b = second.CreateTexture(Texture());
    CHECK(a.Generation() != 0);
    CHECK_FALSE(a == b);
}

TEST_CASE("Compile consumes builders and freezes retained pass builders") {
    gfx::RenderGraphBuilder builder;
    const auto texture = builder.CreateTexture(Texture());
    auto pass = builder.AddPass("root", gfx::PassKind::Compute);
    const auto output = pass.Write(texture);
    pass.SideEffect("root").Execute(NoOp());
    auto graph = builder.Compile(64, 64);
    REQUIRE(graph);
    const auto hash = graph->DeterministicHash();

    pass.SideEffect("changed").Queue(gfx::QueuePreference::Copy).Execute([](gfx::RenderGraphContext&) { return Err(ErrorCode::InvalidState, "changed"); });
    CHECK_FALSE(pass.Write(texture));
    CHECK_FALSE(builder.CreateTexture(Texture("late")));
    CHECK_FALSE(builder.Export(output));
    CHECK_FALSE(builder.Compile(64, 64));
    CHECK(graph->DeterministicHash() == hash);
    REQUIRE(graph->Passes().size() == 1);
    CHECK(graph->Passes()[0].queue == gfx::QueueClass::Compute);
}

TEST_CASE("Liveness ignores dead readers and does not retain later overwrites of exports") {
    gfx::RenderGraphBuilder builder;
    const auto texture = builder.CreateTexture(Texture());
    auto first = builder.AddPass("exported-writer", gfx::PassKind::Compute);
    const auto exported = first.Write(texture);
    first.Execute(NoOp());
    REQUIRE(builder.Export(exported));
    auto dead_reader = builder.AddPass("dead-reader", gfx::PassKind::Compute);
    dead_reader.Read(exported).Execute(NoOp());
    auto overwrite = builder.AddPass("dead-overwrite", gfx::PassKind::Compute);
    static_cast<void>(overwrite.Write(texture));
    overwrite.Execute(NoOp());

    auto graph = builder.Compile(64, 64);
    REQUIRE(graph);
    REQUIRE(graph->Passes().size() == 1);
    CHECK(graph->Passes()[0].name == "exported-writer");
}

TEST_CASE("Render graph rejects access direction and impossible queue preferences") {
    gfx::RenderGraphBuilder wrong_direction;
    const auto texture = wrong_direction.CreateTexture(Texture());
    auto pass = wrong_direction.AddPass("wrong", gfx::PassKind::Compute);
    static_cast<void>(pass.Write(texture, gfx::GraphAccess::Sampled));
    pass.SideEffect("root").Execute(NoOp());
    auto invalid = wrong_direction.Compile(64, 64);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().Message().find("GRF1021") != std::string_view::npos);

    gfx::RenderGraphBuilder wrong_queue;
    auto render = wrong_queue.AddPass("render", gfx::PassKind::Render);
    const auto color = wrong_queue.CreateTexture(Texture());
    const auto output = render.Color(color);
    render.Queue(gfx::QueuePreference::Compute, true).Execute(NoOp());
    REQUIRE(wrong_queue.Export(output));
    auto queue_invalid = wrong_queue.Compile(64, 64);
    REQUIRE_FALSE(queue_invalid);
    CHECK(queue_invalid.error().Message().find("GRF1019") != std::string_view::npos);
}

TEST_CASE("Render graph validates MRT gaps and resolve contracts") {
    gfx::RenderGraphBuilder sparse;
    const auto color = sparse.CreateTexture(Texture());
    auto render = sparse.AddPass("sparse", gfx::PassKind::Render);
    gfx::ColorAttachment attachment;
    attachment.slot = 1;
    const auto output = render.Color(color, attachment);
    render.Execute(NoOp());
    REQUIRE(sparse.Export(output));
    auto sparse_result = sparse.Compile(64, 64);
    REQUIRE_FALSE(sparse_result);
    CHECK(sparse_result.error().Message().find("GRF1020") != std::string_view::npos);

    gfx::RenderGraphBuilder resolve;
    auto source_desc = Texture("source");
    source_desc.sample_count = 1;
    const auto source = resolve.CreateTexture(source_desc);
    const auto target = resolve.CreateTexture(Texture("target"));
    auto pass = resolve.AddPass("resolve", gfx::PassKind::Render);
    const auto resolved = pass.ResolveColor(source, target);
    pass.Execute(NoOp());
    REQUIRE(resolve.Export(resolved));
    auto resolve_result = resolve.Compile(64, 64);
    REQUIRE_FALSE(resolve_result);
    CHECK(resolve_result.error().Message().find("GRF1024") != std::string_view::npos);
}

TEST_CASE("Render graph validates depth attachments and copy bounds") {
    gfx::RenderGraphBuilder depth_builder;
    const auto color_depth = depth_builder.CreateTexture(Texture("not-depth"));
    auto depth_pass = depth_builder.AddPass("bad-depth", gfx::PassKind::Render);
    const auto depth_output = depth_pass.Depth(color_depth);
    depth_pass.Execute(NoOp());
    REQUIRE(depth_builder.Export(depth_output));
    auto depth = depth_builder.Compile(64, 64);
    REQUIRE_FALSE(depth);
    CHECK(depth.error().Message().find("GRF1022") != std::string_view::npos);

    gfx::RenderGraphBuilder extent_builder;
    const auto color = extent_builder.CreateTexture(Texture("color"));
    auto depth_descriptor = Texture("depth");
    depth_descriptor.format = rhi::TextureFormat::Depth32Float;
    depth_descriptor.extent = gfx::GraphExtent::Fixed(32, 64);
    const auto depth_texture = extent_builder.CreateTexture(depth_descriptor);
    auto mismatched = extent_builder.AddPass("mismatched", gfx::PassKind::Render);
    const auto color_output = mismatched.Color(color);
    static_cast<void>(mismatched.Depth(depth_texture));
    mismatched.Execute(NoOp());
    REQUIRE(extent_builder.Export(color_output));
    auto extent = extent_builder.Compile(64, 64);
    REQUIRE_FALSE(extent);
    CHECK(extent.error().Message().find("GRF1023") != std::string_view::npos);

    gfx::RenderGraphBuilder copy_builder;
    const auto source = copy_builder.CreateBuffer({.label = "source", .size = 16, .alignment = 4});
    const auto destination = copy_builder.CreateBuffer({.label = "destination", .size = 8, .alignment = 4});
    auto produce = copy_builder.AddPass("produce", gfx::PassKind::Compute);
    const auto produced = produce.Write(source);
    produce.Execute(NoOp());
    auto copy = copy_builder.AddPass("copy", gfx::PassKind::Copy);
    copy.Copy(produced, destination, 12).SideEffect("root");
    auto copy_result = copy_builder.Compile(64, 64);
    REQUIRE_FALSE(copy_result);
    CHECK(copy_result.error().Message().find("GRF1026") != std::string_view::npos);
}

TEST_CASE("Render graph emits external boundaries and same-state write barriers") {
    gfx::RenderGraphBuilder external;
    auto descriptor = Texture("present");
    descriptor.usage = rhi::TextureUsage::RenderAttachment;
    const auto present = external.ImportTexture({.descriptor = descriptor, .initial_state = gfx::ExternalState::Present, .final_state = gfx::ExternalState::Present, .frame_bound = true});
    auto render = external.AddPass("present", gfx::PassKind::Render);
    const auto output = render.Color(present);
    render.Execute(NoOp());
    REQUIRE(external.Export(output, gfx::ExternalState::Present));
    auto external_graph = external.Compile(64, 64);
    REQUIRE(external_graph);
    CHECK(std::ranges::any_of(external_graph->Transitions(),
        [](const auto& transition) { return transition.external_acquire && transition.before_pass == gfx::kInvalidGraphIndex && transition.external_state == gfx::ExternalState::Present; }));
    CHECK(std::ranges::any_of(external_graph->Transitions(),
        [](const auto& transition) { return transition.external_release && transition.pass == gfx::kInvalidGraphIndex && transition.external_state == gfx::ExternalState::Present; }));

    gfx::RenderGraphBuilder writes;
    const auto texture = writes.CreateTexture(Texture());
    auto first = writes.AddPass("first", gfx::PassKind::Compute);
    static_cast<void>(first.Write(texture, gfx::GraphAccess::StorageWrite));
    first.SideEffect("first").Execute(NoOp());
    auto second = writes.AddPass("second", gfx::PassKind::Compute);
    const auto latest = second.Write(texture, gfx::GraphAccess::StorageWrite);
    second.Execute(NoOp());
    REQUIRE(writes.Export(latest));
    auto write_graph = writes.Compile(64, 64);
    REQUIRE(write_graph);
    CHECK(std::ranges::any_of(write_graph->Transitions(),
        [](const auto& transition) { return transition.before == gfx::GraphAccess::StorageWrite && transition.after == gfx::GraphAccess::StorageWrite && transition.write_barrier; }));
}

TEST_CASE("Render graph aliases only non-overlapping compatible textures") {
    gfx::RenderGraphBuilder builder;
    const auto first = builder.CreateTexture(Texture("first"));
    const auto second = builder.CreateTexture(Texture("second"));
    auto write_first = builder.AddPass("write-first", gfx::PassKind::Compute);
    const auto first_version = write_first.Write(first);
    write_first.Execute(NoOp());
    auto read_first = builder.AddPass("read-first", gfx::PassKind::Compute);
    read_first.Read(first_version).SideEffect("first").Execute(NoOp());
    auto write_second = builder.AddPass("write-second", gfx::PassKind::Compute);
    write_second.DependsOn(read_first.Handle());
    const auto second_version = write_second.Write(second);
    write_second.Execute(NoOp());
    auto read_second = builder.AddPass("read-second", gfx::PassKind::Compute);
    read_second.Read(second_version).SideEffect("second").Execute(NoOp());
    auto graph = builder.Compile(64, 64);
    REQUIRE(graph);
    const auto first_life = std::ranges::find(graph->Lifetimes(), first_version.Index(), &gfx::GraphLifetimeInterval::version);
    const auto second_life = std::ranges::find(graph->Lifetimes(), second_version.Index(), &gfx::GraphLifetimeInterval::version);
    REQUIRE(first_life != graph->Lifetimes().end());
    REQUIRE(second_life != graph->Lifetimes().end());
    CHECK(first_life->last_use < second_life->first_use);
    CHECK(first_life->physical_slot == second_life->physical_slot);
    CHECK(graph->EstimatedTransientPeakBytes() == 64u * 64u * 4u);
}

TEST_CASE("Render graph hazards respect texture mip subresources") {
    auto compile = [](const u32 second_mip) {
        gfx::RenderGraphBuilder builder;
        auto descriptor = Texture("mipped");
        descriptor.mip_levels = 3;
        const auto texture = builder.CreateTexture(descriptor);
        auto first = builder.AddPass("mip-zero", gfx::PassKind::Compute);
        static_cast<void>(first.Write(texture, gfx::GraphAccess::StorageWrite, {.base_mip_level = 0, .mip_level_count = 1}));
        first.SideEffect("first").Execute(NoOp());
        auto second = builder.AddPass("mip-second", gfx::PassKind::Compute);
        static_cast<void>(second.Write(texture, gfx::GraphAccess::StorageWrite, {.base_mip_level = second_mip, .mip_level_count = 1}));
        second.SideEffect("second").Execute(NoOp());
        return builder.Compile(64, 64);
    };

    auto disjoint = compile(1);
    REQUIRE(disjoint);
    CHECK(std::ranges::none_of(disjoint->Dependencies(), [](const auto& dependency) { return dependency.reason.starts_with("WAW"); }));
    auto overlapping = compile(0);
    REQUIRE(overlapping);
    CHECK(std::ranges::any_of(overlapping->Dependencies(), [](const auto& dependency) { return dependency.reason.starts_with("WAW"); }));
}

TEST_CASE("Render graph schedule is stable for seeded random DAGs") {
    auto build = [] {
        gfx::RenderGraphBuilder builder;
        std::minstd_rand random(0x03u);
        std::vector<gfx::GraphPass> passes;
        for (u32 index = 0; index < 32; ++index) {
            auto pass = builder.AddPass("pass-" + std::to_string(index), gfx::PassKind::Compute);
            if (index != 0 && (random() & 1u) != 0)
                pass.DependsOn(passes[random() % index]);
            pass.SideEffect("property-root").Execute(NoOp());
            passes.push_back(pass.Handle());
        }
        return builder.Compile(32, 32);
    };
    auto first = build();
    auto second = build();
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(first->Passes().size() == 32);
    CHECK(first->DeterministicHash() == second->DeterministicHash());
    CHECK(first->DumpJson() == second->DumpJson());
    for (const auto& edge : first->Dependencies()) {
        const auto before = std::ranges::find(first->Passes(), edge.before, &gfx::CompiledPassInfo::declaration_index);
        const auto after = std::ranges::find(first->Passes(), edge.after, &gfx::CompiledPassInfo::declaration_index);
        REQUIRE(before != first->Passes().end());
        REQUIRE(after != first->Passes().end());
        CHECK(before->schedule_index < after->schedule_index);
    }
}

TEST_CASE("Compile reports accumulate independent named diagnostics") {
    gfx::RenderGraphBuilder builder;
    auto bad_texture = Texture("bad-texture");
    bad_texture.format = rhi::TextureFormat::Undefined;
    static_cast<void>(builder.CreateTexture(bad_texture));
    static_cast<void>(builder.CreateBuffer({.label = "bad-buffer", .size = 0, .alignment = 3}));
    static_cast<void>(builder.AddPass("", gfx::PassKind::Render));
    static_cast<void>(builder.AddPass("compute", gfx::PassKind::Compute));
    auto report = builder.CompileWithReport(64, 64);
    REQUIRE_FALSE(report);
    CHECK(report.diagnostics.size() >= 5);
    CHECK(std::ranges::any_of(report.diagnostics, [](const auto& value) { return value.code == "GRF1004" && value.resource == "bad-texture"; }));
    CHECK(std::ranges::any_of(report.diagnostics, [](const auto& value) { return value.code == "GRF1005" && value.resource == "bad-buffer"; }));
    CHECK(std::ranges::all_of(report.diagnostics, [](const auto& value) { return !value.stage.empty(); }));
}

TEST_CASE("Graph dumps escape hostile names and canonical hashes cover semantic input") {
    auto build = [](std::string name, u32 width) {
        gfx::RenderGraphBuilder builder;
        auto descriptor = Texture("resource\"\\\n");
        descriptor.extent = gfx::GraphExtent::Fixed(width, 64);
        const auto texture = builder.CreateTexture(descriptor);
        auto pass = builder.AddPass(std::move(name), gfx::PassKind::Compute);
        const auto output = pass.Write(texture);
        pass.SideEffect("root").Execute(NoOp());
        REQUIRE(builder.Export(output));
        return builder.Compile(width, 64);
    };
    auto first = build("p\"\\\n&", 64);
    auto same = build("p\"\\\n&", 64);
    auto renamed = build("other", 64);
    auto resized = build("p\"\\\n&", 32);
    REQUIRE(first);
    REQUIRE(same);
    REQUIRE(renamed);
    REQUIRE(resized);
    CHECK(first->DumpJson().find("p\\\"\\\\\\n&") != std::string::npos);
    CHECK(first->DumpDot().find("p\\\"\\\\\\n&") != std::string::npos);
    CHECK(first->DumpMermaid().find("p&quot;\\<br/>&amp;") != std::string::npos);
    CHECK(first->DeterministicHash() == same->DeterministicHash());
    CHECK(first->DeterministicHash() != renamed->DeterministicHash());
    CHECK(first->DeterministicHash() != resized->DeterministicHash());
}

TEST_CASE("Executor backend orders callbacks and submits exactly once") {
    gfx::RenderGraphBuilder builder;
    const auto color = builder.CreateTexture(Texture("color"));
    const auto copy_target = builder.CreateTexture(Texture("copy-target"));
    std::vector<std::string> callbacks;
    auto render = builder.AddPass("render", gfx::PassKind::Render);
    const auto rendered = render.Color(color);
    render.SideEffect("render").Execute([&](gfx::RenderGraphContext&) { callbacks.emplace_back("render"); });
    auto compute = builder.AddPass("compute", gfx::PassKind::Compute);
    compute.Read(rendered).SideEffect("compute").Execute([&](gfx::RenderGraphContext&) { callbacks.emplace_back("compute"); });
    auto copy = builder.AddPass("copy", gfx::PassKind::Copy);
    copy.Copy(rendered, copy_target).SideEffect("copy").Execute([&](gfx::RenderGraphContext&) { callbacks.emplace_back("copy"); });
    auto graph = builder.Compile(64, 64);
    REQUIRE(graph);
    auto backend = std::make_shared<FakeCommandBackend>();
    gfx::GraphExecutor executor(backend, std::move(*graph));
    auto frame = executor.Begin(64, 64);
    REQUIRE(frame);
    const auto submitted = frame->Execute();
    REQUIRE(submitted);
    CHECK(submitted->Value() == 1);
    CHECK(callbacks == std::vector<std::string>{"render", "compute", "copy"});
    CHECK(backend->events == std::vector<std::string>{"frame", "render:render", "end", "compute:compute", "end", "copy:copy", "finish", "submit"});
}

TEST_CASE("Executor bounds transient sets until submission completion") {
    gfx::RenderGraphBuilder builder;
    auto pass = builder.AddPass("work", gfx::PassKind::Compute);
    pass.SideEffect("work").Execute(NoOp());
    auto graph = builder.Compile(64, 64);
    REQUIRE(graph);
    auto backend = std::make_shared<FakeCommandBackend>();
    backend->complete_immediately = false;
    gfx::GraphExecutor executor(backend, std::move(*graph));

    for (u64 expected = 1; expected <= 3; ++expected) {
        auto frame = executor.Begin(64, 64);
        REQUIRE(frame);
        auto submitted = frame->Execute();
        REQUIRE(submitted);
        CHECK(submitted->Value() == expected);
    }
    CHECK_FALSE(executor.Begin(64, 64));

    backend->completed = rhi::SubmissionEpoch(2);
    auto resumed = executor.Begin(64, 64);
    REQUIRE(resumed);
    auto submitted = resumed->Execute();
    REQUIRE(submitted);
    CHECK(submitted->Value() == 4);
}

TEST_CASE("Native executor allocates and resolves retained transient buffers") {
    gfx::RenderGraphBuilder builder;
    const gfx::GraphBufferDesc descriptor{.label = "transient", .size = 64, .alignment = 4};
    const auto destination = builder.CreateBuffer(descriptor);
    bool resolved{};
    auto compute = builder.AddPass("retain-buffer", gfx::PassKind::Compute);
    const auto output = compute.Write(destination);
    compute.SideEffect("retain-buffer").Execute([&](gfx::RenderGraphContext& context) -> Result<void> {
        auto buffer = context.Buffer(output);
        if (!buffer)
            return Err(std::move(buffer.error()));
        resolved = true;
        return Ok();
    });
    auto graph = builder.Compile(64, 64);
    REQUIRE(graph);
    auto device = std::make_shared<FakeDevice>();
    gfx::GraphExecutor executor(device, std::move(*graph));
    auto frame = executor.Begin(64, 64);
    REQUIRE(frame);
    auto executed = frame->Execute();
    const std::string diagnostic = executed ? "" : std::string(executed.error().Message());
    INFO(diagnostic);
    REQUIRE(executed);
    CHECK(device->buffer_creations == 1);
    CHECK(resolved);
}

TEST_CASE("Executor ends failed passes and never finishes or submits failures") {
    for (const auto kind : {gfx::PassKind::Render, gfx::PassKind::Compute}) {
        gfx::RenderGraphBuilder builder;
        auto pass = builder.AddPass("failure", kind);
        if (kind == gfx::PassKind::Render) {
            const auto texture = builder.CreateTexture(Texture());
            static_cast<void>(pass.Color(texture));
        }
        pass.SideEffect("root").Execute([](gfx::RenderGraphContext&) { return Err(ErrorCode::InvalidState, "callback failure"); });
        auto graph = builder.Compile(64, 64);
        REQUIRE(graph);
        auto backend = std::make_shared<FakeCommandBackend>();
        gfx::GraphExecutor executor(backend, std::move(*graph));
        auto frame = executor.Begin(64, 64);
        REQUIRE(frame);
        CHECK_FALSE(frame->Execute());
        CHECK(std::ranges::count(backend->events, "end") == 1);
        CHECK(std::ranges::find(backend->events, "finish") == backend->events.end());
        CHECK(std::ranges::find(backend->events, "submit") == backend->events.end());
        CHECK(executor.Begin(64, 64).has_value());
    }
}

TEST_CASE("Executor enforces frame ownership, dimensions, culling and safe invalidation") {
    int calls{};
    gfx::RenderGraphBuilder builder;
    auto dead = builder.AddPass("dead", gfx::PassKind::Compute);
    dead.Execute([&](gfx::RenderGraphContext&) { ++calls; });
    auto live = builder.AddPass("live", gfx::PassKind::Compute);
    live.SideEffect("root").Execute([&](gfx::RenderGraphContext&) { ++calls; });
    auto graph = builder.Compile(64, 64);
    REQUIRE(graph);
    auto backend = std::make_shared<FakeCommandBackend>();
    auto executor = std::make_unique<gfx::GraphExecutor>(backend, std::move(*graph));
    CHECK_FALSE(executor->Begin(32, 64));
    auto frame = executor->Begin(64, 64);
    REQUIRE(frame);
    CHECK_FALSE(executor->Begin(64, 64));
    executor.reset();
    CHECK_FALSE(frame->Execute());
    CHECK(calls == 0);
}

TEST_CASE("Executor reports missing and wrong frame bindings") {
    gfx::RenderGraphBuilder builder;
    auto descriptor = Texture("frame-input");
    descriptor.usage = rhi::TextureUsage::TextureBinding;
    const auto input = builder.ImportTexture({.descriptor = descriptor, .initial_state = gfx::ExternalState::ShaderRead, .frame_bound = true});
    auto pass = builder.AddPass("sample", gfx::PassKind::Compute);
    pass.Read(builder.Initial(input)).SideEffect("root").Execute(NoOp());
    auto graph = builder.Compile(64, 64);
    REQUIRE(graph);
    auto backend = std::make_shared<FakeCommandBackend>();
    gfx::GraphExecutor executor(backend, std::move(*graph));
    auto frame = executor.Begin(64, 64);
    REQUIRE(frame);
    auto wrong = descriptor;
    wrong.format = rhi::TextureFormat::RGBA16Float;
    auto bound = frame->Bind({.resource = input, .texture = nullptr, .view = nullptr, .signature = wrong});
    REQUIRE_FALSE(bound);
    CHECK(bound.error().Message().find("GRF2012") != std::string_view::npos);
    auto executed = frame->Execute();
    REQUIRE_FALSE(executed);
    CHECK(executed.error().Message().find("GRF2021") != std::string_view::npos);
    CHECK(std::ranges::find(backend->events, "finish") == backend->events.end());
}

TEST_CASE("Executor accepts static imports and compiler emits their boundaries") {
    gfx::RenderGraphBuilder builder;
    auto descriptor = Texture("static-input");
    descriptor.usage = rhi::TextureUsage::TextureBinding;
    auto physical = std::make_shared<FakeTexture>(descriptor);
    const auto input = builder.ImportTexture({.descriptor = descriptor, .initial_state = gfx::ExternalState::ShaderRead, .final_state = gfx::ExternalState::ShaderRead, .frame_bound = false}, physical);
    auto pass = builder.AddPass("sample-static", gfx::PassKind::Compute);
    pass.Read(builder.Initial(input)).SideEffect("root").Execute(NoOp());
    auto graph = builder.Compile(64, 64);
    REQUIRE(graph);
    CHECK(std::ranges::count_if(graph->Transitions(), [](const auto& value) { return value.external_acquire; }) == 1);
    CHECK(std::ranges::count_if(graph->Transitions(), [](const auto& value) { return value.external_release; }) == 1);
    auto backend = std::make_shared<FakeCommandBackend>();
    gfx::GraphExecutor executor(backend, std::move(*graph));
    auto frame = executor.Begin(64, 64);
    REQUIRE(frame);
    CHECK(frame->Execute());
}

TEST_CASE("Static imports reject missing and mismatched physical resources") {
    gfx::RenderGraphBuilder missing_builder;
    auto descriptor = Texture("missing-static");
    descriptor.usage = rhi::TextureUsage::TextureBinding;
    static_cast<void>(missing_builder.ImportTexture({.descriptor = descriptor, .initial_state = gfx::ExternalState::ShaderRead, .frame_bound = false}));
    auto missing_report = missing_builder.CompileWithReport(64, 64);
    REQUIRE_FALSE(missing_report);
    CHECK(std::ranges::any_of(missing_report.diagnostics, [](const auto& value) { return value.code == "GRF1014" && value.resource == "missing-static"; }));

    gfx::RenderGraphBuilder mismatch_builder;
    auto physical_descriptor = descriptor;
    physical_descriptor.format = rhi::TextureFormat::RGBA16Float;
    const auto input = mismatch_builder.ImportTexture({.descriptor = descriptor, .initial_state = gfx::ExternalState::ShaderRead, .frame_bound = false}, std::make_shared<FakeTexture>(physical_descriptor));
    auto pass = mismatch_builder.AddPass("sample", gfx::PassKind::Compute);
    pass.Read(mismatch_builder.Initial(input)).SideEffect("root").Execute(NoOp());
    auto graph = mismatch_builder.Compile(64, 64);
    REQUIRE(graph);
    auto backend = std::make_shared<FakeCommandBackend>();
    gfx::GraphExecutor executor(backend, std::move(*graph));
    auto frame = executor.Begin(64, 64);
    REQUIRE_FALSE(frame);
    CHECK(frame.error().Message().find("GRF2012") != std::string_view::npos);
}

TEST_CASE("Temporal history rotates only after successful submission") {
    gfx::RenderGraphBuilder builder;
    auto descriptor = Texture("history");
    descriptor.usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::StorageBinding;
    const auto temporal = builder.ImportTemporal({.previous = nullptr, .current = nullptr, .descriptor = descriptor});
    auto pass = builder.AddPass("history", gfx::PassKind::Compute);
    pass.Read(temporal.previous_version);
    const auto output = pass.Write(temporal.current, gfx::GraphAccess::StorageWrite);
    pass.Execute(NoOp());
    REQUIRE(builder.Export(output));
    auto graph = builder.Compile(64, 64);
    REQUIRE(graph);
    auto backend = std::make_shared<FakeCommandBackend>();
    backend->submit_succeeds = false;
    gfx::GraphExecutor executor(backend, std::move(*graph));
    auto view_a = std::make_shared<FakeTextureView>();
    auto view_b = std::make_shared<FakeTextureView>();
    auto texture_a = std::make_shared<FakeTexture>(descriptor);
    auto texture_b = std::make_shared<FakeTexture>(descriptor);
    auto first = executor.Begin(64, 64);
    REQUIRE(first);
    auto wrong_signature = descriptor;
    wrong_signature.format = rhi::TextureFormat::RGBA16Float;
    CHECK_FALSE(first->Bind({.resource = temporal, .previous_texture = texture_a, .previous_view = view_a, .current_texture = texture_b, .current_view = view_b, .signature = wrong_signature}));
    REQUIRE(first->Bind({.resource = temporal, .previous_texture = texture_a, .previous_view = view_a, .current_texture = texture_b, .current_view = view_b, .signature = descriptor}));
    CHECK_FALSE(first->Execute());

    backend->submit_succeeds = true;
    auto after_failure = executor.Begin(64, 64);
    REQUIRE(after_failure);
    CHECK_FALSE(after_failure->Bind({.resource = temporal, .previous_texture = nullptr, .previous_view = nullptr, .current_texture = texture_b, .current_view = view_b, .signature = descriptor}));
    REQUIRE(after_failure->Bind({.resource = temporal, .previous_texture = texture_a, .previous_view = view_a, .current_texture = texture_b, .current_view = view_b, .signature = descriptor}));
    REQUIRE(after_failure->Execute());

    auto after_success = executor.Begin(64, 64);
    REQUIRE(after_success);
    CHECK(after_success->Bind({.resource = temporal, .previous_texture = nullptr, .previous_view = nullptr, .current_texture = texture_a, .current_view = view_a, .signature = descriptor}));
}

TEST_CASE("Fixed-seed DAG properties preserve legal schedules and alias intervals") {
    for (const u32 seed : {3u, 17u, 101u, 911u}) {
        auto build = [seed] {
            std::minstd_rand random(seed);
            gfx::RenderGraphBuilder builder;
            std::vector<gfx::GraphPass> passes;
            for (u32 index = 0; index < 24; ++index) {
                const auto texture = builder.CreateTexture(Texture("r" + std::to_string(index)));
                auto writer = builder.AddPass("w" + std::to_string(index), gfx::PassKind::Compute);
                if (!passes.empty())
                    writer.DependsOn(passes[random() % passes.size()]);
                const auto version = writer.Write(texture);
                writer.Execute(NoOp());
                auto reader = builder.AddPass("r" + std::to_string(index), gfx::PassKind::Compute);
                reader.Read(version).SideEffect("root").Execute(NoOp());
                passes.push_back(reader.Handle());
            }
            return builder.Compile(64, 64);
        };
        auto first = build();
        auto second = build();
        REQUIRE(first);
        REQUIRE(second);
        CHECK(first->DeterministicHash() == second->DeterministicHash());
        for (const auto& edge : first->Dependencies()) {
            const auto before = std::ranges::find(first->Passes(), edge.before, &gfx::CompiledPassInfo::declaration_index);
            const auto after = std::ranges::find(first->Passes(), edge.after, &gfx::CompiledPassInfo::declaration_index);
            REQUIRE(before != first->Passes().end());
            REQUIRE(after != first->Passes().end());
            CHECK(before->schedule_index < after->schedule_index);
        }
        for (const auto& life : first->Lifetimes())
            CHECK(life.first_use <= life.last_use);
        for (size_t a = 0; a < first->Lifetimes().size(); ++a)
            for (size_t b = a + 1; b < first->Lifetimes().size(); ++b)
                if (first->Lifetimes()[a].physical_slot == first->Lifetimes()[b].physical_slot)
                    CHECK((first->Lifetimes()[a].last_use < first->Lifetimes()[b].first_use || first->Lifetimes()[b].last_use < first->Lifetimes()[a].first_use));
    }
}
