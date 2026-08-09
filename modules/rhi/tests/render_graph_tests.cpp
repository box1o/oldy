#include <cmath>
#include <catch2/catch_test_macros.hpp>

#include <woki/rhi.hpp>

namespace {

using namespace woki;
using namespace woki::rhi;

class FakeTextureView final : public TextureView {
public:
    void SetLabel(std::string_view) override {}

    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override {
        return {.resource = const_cast<FakeTextureView*>(this)};
    }
};

class FakeTexture final : public Texture {
public:
    FakeTexture(u32 width, u32 height, u32 layers, TextureFormat format, TextureUsage usage, TextureDimension dimension = TextureDimension::e2D, u32 sample_count = 1, std::vector<TextureViewDesc>* view_descs = nullptr)
        : width_(width),
          height_(height),
          layers_(layers),
          format_(format),
          usage_(usage),
          dimension_(dimension),
          sample_count_(sample_count),
          view_descs_(view_descs) {}

    [[nodiscard]] scope<TextureView> CreateErrorView(const TextureViewDesc& = {}) const override {
        return createScope<FakeTextureView>();
    }

    [[nodiscard]] scope<TextureView> CreateView(const TextureViewDesc& desc = {}) const override {
        if (view_descs_ != nullptr) {
            view_descs_->push_back(desc);
        }
        return createScope<FakeTextureView>();
    }

    void Destroy() override {}

    [[nodiscard]] u32 GetDepthOrArrayLayers() const override {
        return layers_;
    }

    [[nodiscard]] TextureDimension GetDimension() const override {
        return dimension_;
    }

    [[nodiscard]] TextureFormat GetFormat() const override {
        return format_;
    }

    [[nodiscard]] u32 GetHeight() const override {
        return height_;
    }

    [[nodiscard]] u32 GetMipLevelCount() const override {
        return 1;
    }

    [[nodiscard]] u32 GetSampleCount() const override {
        return sample_count_;
    }

    [[nodiscard]] TextureViewDimension GetTextureBindingViewDimension() const override {
        return TextureViewDimension::e2D;
    }

    [[nodiscard]] TextureUsage GetUsage() const override {
        return usage_;
    }

    [[nodiscard]] u32 GetWidth() const override {
        return width_;
    }

    void Pin(TextureUsage) override {}

    void SetLabel(std::string_view) override {}

    void SetOwnershipForMemoryDump(u64) override {}

    void Unpin() override {}

    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override {
        return {.resource = const_cast<FakeTexture*>(this)};
    }

private:
    u32 width_;
    u32 height_;
    u32 layers_;
    TextureFormat format_;
    TextureUsage usage_;
    TextureDimension dimension_;
    u32 sample_count_;
    std::vector<TextureViewDesc>* view_descs_;
};

class FakeCommandBuffer final : public CommandBuffer {
public:
    void SetLabel(std::string_view) override {}

    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }
};

struct FakeState final {
    Extent3D copy_extent{};
    RenderPassDepthStencilAttachmentDesc depth_attachment{};
    bool captured_depth_attachment{false};
    u32 copy_count{0};
    u32 submit_count{0};
};

class FakeCommandEncoder final : public CommandEncoder {
public:
    explicit FakeCommandEncoder(FakeState& state)
        : state_(state) {}

    [[nodiscard]] Result<scope<ComputePassEncoder>> BeginComputePass(const ComputePassDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<RenderPassEncoder>> BeginRenderPass(const RenderPassDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<RenderPassEncoder>> BeginRenderPass(const RenderPassDescTyped& desc) override {
        if (desc.depth_stencil_attachment != nullptr) {
            state_.depth_attachment = *desc.depth_stencil_attachment;
            state_.captured_depth_attachment = true;
        }
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<void> ClearBuffer(const Buffer&, u64, u64) override {
        return Ok();
    }

    [[nodiscard]] Result<void> CopyBufferToBuffer(const Buffer&, u64, const Buffer&, u64, u64) override {
        return Ok();
    }

    [[nodiscard]] Result<void> CopyBufferToTexture(const TexelCopyBufferInfo&, const TexelCopyTextureInfo&, const Extent3D&) override {
        return Ok();
    }

    [[nodiscard]] Result<void> CopyTextureToBuffer(const TexelCopyTextureInfo&, const TexelCopyBufferInfo&, const Extent3D&) override {
        return Ok();
    }

    [[nodiscard]] Result<void> CopyTextureToTexture(const TexelCopyTextureInfo&, const TexelCopyTextureInfo&, const Extent3D& extent) override {
        state_.copy_extent = extent;
        ++state_.copy_count;
        return Ok();
    }

    [[nodiscard]] Result<scope<CommandBuffer>> Finish(const CommandBufferDesc&) override {
        return Ok<scope<CommandBuffer>>(createScope<FakeCommandBuffer>());
    }

    void InjectValidationError(std::string_view) override {}

    void InsertDebugMarker(std::string_view) override {}

    void PopDebugGroup() override {}

    void PushDebugGroup(std::string_view) override {}

    [[nodiscard]] Result<void> ResolveQuerySet(const QuerySet&, u32, u32, const Buffer&, u64) override {
        return Ok();
    }

    void SetLabel(std::string_view) override {}

    [[nodiscard]] Result<void> WriteBuffer(const Buffer&, u64, const u8*, u64) override {
        return Ok();
    }

    [[nodiscard]] Result<void> WriteTimestamp(const QuerySet&, u32) override {
        return Ok();
    }

    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }

private:
    FakeState& state_;
};

class FakeQueue final : public Queue {
public:
    explicit FakeQueue(FakeState& state)
        : state_(state) {}

    [[nodiscard]] Result<void> CopyExternalTextureForBrowser(const ImageCopyExternalTexture&, const TexelCopyTextureInfo&, const Extent3D&, const CopyTextureForBrowserOptions&) const override {
        return Ok();
    }

    [[nodiscard]] Result<void> CopyTextureForBrowser(const TexelCopyTextureInfo&, const TexelCopyTextureInfo&, const Extent3D&, const CopyTextureForBrowserOptions&) const override {
        return Ok();
    }

    [[nodiscard]] Future OnSubmittedWorkDone(CallbackMode, QueueWorkDoneCallback) const override {
        return {};
    }

    void SetLabel(std::string_view) const override {}

    [[nodiscard]] Result<void> Submit(std::span<CommandBuffer* const>) const override {
        ++state_.submit_count;
        return Ok();
    }

    [[nodiscard]] Result<void> WriteBuffer(const Buffer&, u64, const void*, u64) const override {
        return Ok();
    }

    [[nodiscard]] Result<void> WriteTexture(const TexelCopyTextureInfo&, const void*, u64, const TexelCopyBufferLayout&, const Extent3D&) const override {
        return Ok();
    }

    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }

private:
    FakeState& state_;
};

class FakeDevice final : public Device {
public:
    FakeDevice()
        : queue_(state_) {}

    [[nodiscard]] Result<scope<BindGroup>> CreateBindGroup(const BindGroupDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<BindGroupLayout>> CreateBindGroupLayout(const BindGroupLayoutDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<Buffer>> CreateBuffer(const BufferDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<CommandEncoder>> CreateCommandEncoder(const CommandEncoderDesc&) override {
        return Ok<scope<CommandEncoder>>(createScope<FakeCommandEncoder>(state_));
    }

    [[nodiscard]] Result<scope<ComputePipeline>> CreateComputePipeline(const ComputePipelineDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Future CreateComputePipelineAsync(const ComputePipelineDesc&, CallbackMode, CreateComputePipelineCallback) override {
        return {};
    }

    [[nodiscard]] Result<scope<Buffer>> CreateErrorBuffer(const BufferDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<ExternalTexture>> CreateErrorExternalTexture() override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<ShaderModule>> CreateErrorShaderModule(const ShaderModuleDesc&, std::string_view) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<Texture>> CreateErrorTexture(const TextureDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<ExternalTexture>> CreateExternalTexture(const ExternalTextureDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<PipelineLayout>> CreatePipelineLayout(const PipelineLayoutDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<QuerySet>> CreateQuerySet(const QuerySetDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<RenderBundleEncoder>> CreateRenderBundleEncoder(const RenderBundleEncoderDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<RenderPipeline>> CreateRenderPipeline(const RenderPipelineDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<RenderPipeline>> CreateRenderPipeline(const RenderPipelineDescTyped&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Future CreateRenderPipelineAsync(const RenderPipelineDesc&, CallbackMode, CreateRenderPipelineCallback) override {
        return {};
    }

    [[nodiscard]] Result<scope<ResourceTable>> CreateResourceTable(const ResourceTableDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<Sampler>> CreateSampler(const SamplerDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<ShaderModule>> CreateShaderModule(const ShaderModuleDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<Texture>> CreateTexture(const TextureDesc& desc) override {
        if (!allocate_textures_) {
            return Err(ErrorCode::GraphicsTextureCreationFailed, "fake allocation failure");
        }
        return Ok<scope<Texture>>(createScope<FakeTexture>(desc.size.width, desc.size.height, desc.size.depth_or_array_layers, desc.format, desc.usage, desc.dimension, desc.sample_count, &created_view_descs_));
    }

    [[nodiscard]] Result<scope<Swapchain>> CreateSwapchain(ref<Surface>, SwapchainDesc) override {
        return Err(ErrorCode::InvalidState);
    }

    void Destroy() override {}

    void ForceLoss(DeviceLostReason, std::string_view) override {}

    [[nodiscard]] Result<scope<Adapter>> GetAdapter() const override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<void> GetAdapterInfo(AdapterInfo&) const override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<void> GetAHardwareBufferProperties(void*, void*) const override {
        return Err(ErrorCode::InvalidState);
    }

    void GetFeatures(SupportedFeatures&) const override {}

    [[nodiscard]] Result<void> GetLimits(Limits&) const override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Future GetLostFuture() const override {
        return {};
    }

    [[nodiscard]] Queue& GetQueue() const noexcept override {
        return const_cast<FakeQueue&>(queue_);
    }

    [[nodiscard]] bool HasFeature(FeatureName) const noexcept override {
        return false;
    }

    [[nodiscard]] Result<scope<SharedBufferMemory>> ImportSharedBufferMemory(const SharedBufferMemoryDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<SharedFence>> ImportSharedFence(const SharedFenceDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    [[nodiscard]] Result<scope<SharedTextureMemory>> ImportSharedTextureMemory(const SharedTextureMemoryDesc&) override {
        return Err(ErrorCode::InvalidState);
    }

    void InjectError(woki::rhi::ErrorType, std::string_view) override {}

    [[nodiscard]] Future PopErrorScope(CallbackMode, PopErrorScopeCallback) const override {
        return {};
    }

    void PushErrorScope(ErrorFilter) override {}

    void SetLabel(std::string_view) override {}

    void SetLoggingCallback(LoggingCallback) override {}

    void Tick() const noexcept override {}

    void ValidateTextureDescriptor(const TextureDesc&) const override {}

    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }

    FakeState state_{};
    bool allocate_textures_{false};
    std::vector<TextureViewDesc> created_view_descs_{};

private:
    mutable FakeQueue queue_;
};

} // namespace

TEST_CASE("RenderGraph validates transient extents before allocation") {
    auto device = createRef<FakeDevice>();
    RenderGraphBuilder builder(device);
    (void)builder.Transient({
        .format = TextureFormat::RGBA8Unorm,
        .usage = TextureUsage::RenderAttachment,
        .extent = ExtentMode::Relative(0.f, 1.f),
    });

    auto graph = builder.Compile(64, 64);
    REQUIRE_FALSE(graph);
    CHECK(graph.error().Code() == ErrorCode::ValidationOutOfRange);
}

TEST_CASE("RenderGraph propagates transient allocation failure") {
    auto device = createRef<FakeDevice>();
    RenderGraphBuilder builder(device);
    const Resource target = builder.Transient({
        .format = TextureFormat::RGBA8Unorm,
        .usage = TextureUsage::RenderAttachment,
        .extent = ExtentMode::Fixed(16, 16),
    });
    builder.AddPass("pass").Color(0, target).Execute([](RenderPassContext&) {});

    auto graph = builder.Compile(64, 64);
    REQUIRE_FALSE(graph);
    CHECK(graph.error().Code() == ErrorCode::GraphicsTextureCreationFailed);
}

TEST_CASE("RenderGraph copies a complete compatible texture and executes once") {
    auto device = createRef<FakeDevice>();
    auto source = createRef<FakeTexture>(32, 24, 4, TextureFormat::RGBA8Unorm, TextureUsage::CopySrc);
    auto destination = createRef<FakeTexture>(32, 24, 4, TextureFormat::RGBA8Unorm, TextureUsage::CopyDst);

    RenderGraphBuilder builder(device);
    const Resource src = builder.Use(source);
    const Resource dst = builder.Use(destination);
    builder.AddPass("copy").Copy(src, dst).Execute([](CopyPassContext& context) { return context.CopyAll(); });

    auto graph = builder.Compile(128, 128);
    REQUIRE(graph);
    auto frame = (*graph)->BeginFrame(128, 128);
    REQUIRE(frame);
    REQUIRE(frame->Execute());

    CHECK(device->state_.copy_count == 1);
    CHECK(device->state_.copy_extent.width == 32);
    CHECK(device->state_.copy_extent.height == 24);
    CHECK(device->state_.copy_extent.depth_or_array_layers == 4);
    CHECK(device->state_.submit_count == 1);

    auto second = frame->Execute();
    REQUIRE_FALSE(second);
    CHECK(second.error().Code() == ErrorCode::InvalidState);
    CHECK(device->state_.copy_count == 1);
    CHECK(device->state_.submit_count == 1);
}

TEST_CASE("RenderGraph rejects partial or incompatible whole-texture copies") {
    auto device = createRef<FakeDevice>();

    auto compile_copy = [&](ref<Texture> source, ref<Texture> destination) {
        RenderGraphBuilder builder(device);
        const Resource src = builder.Use(std::move(source));
        const Resource dst = builder.Use(std::move(destination));
        builder.AddPass("copy").Copy(src, dst).Execute([](CopyPassContext& context) { return context.CopyAll(); });
        return builder.Compile(64, 64);
    };

    SECTION("extent") {
        auto graph = compile_copy(createRef<FakeTexture>(32, 24, 1, TextureFormat::RGBA8Unorm, TextureUsage::CopySrc), createRef<FakeTexture>(32, 20, 1, TextureFormat::RGBA8Unorm, TextureUsage::CopyDst));
        REQUIRE_FALSE(graph);
        CHECK(graph.error().Code() == ErrorCode::ValidationInvalidState);
    }
    SECTION("dimension") {
        auto graph = compile_copy(createRef<FakeTexture>(32, 1, 1, TextureFormat::RGBA8Unorm, TextureUsage::CopySrc, TextureDimension::e1D),
            createRef<FakeTexture>(32, 1, 1, TextureFormat::RGBA8Unorm, TextureUsage::CopyDst, TextureDimension::e2D));
        REQUIRE_FALSE(graph);
        CHECK(graph.error().Code() == ErrorCode::ValidationInvalidState);
    }
    SECTION("sample count") {
        auto graph = compile_copy(createRef<FakeTexture>(32, 24, 1, TextureFormat::RGBA8Unorm, TextureUsage::CopySrc, TextureDimension::e2D, 4),
            createRef<FakeTexture>(32, 24, 1, TextureFormat::RGBA8Unorm, TextureUsage::CopyDst, TextureDimension::e2D, 4));
        REQUIRE_FALSE(graph);
        CHECK(graph.error().Code() == ErrorCode::ValidationInvalidState);
    }
    SECTION("aspects") {
        auto graph = compile_copy(createRef<FakeTexture>(32, 24, 1, TextureFormat::Depth32Float, TextureUsage::CopySrc), createRef<FakeTexture>(32, 24, 1, TextureFormat::RGBA8Unorm, TextureUsage::CopyDst));
        REQUIRE_FALSE(graph);
        CHECK(graph.error().Code() == ErrorCode::GraphicsInvalidFormat);
    }
}

TEST_CASE("RenderGraph does not resize resources while a frame is outstanding") {
    auto device = createRef<FakeDevice>();
    auto source = createRef<FakeTexture>(8, 8, 1, TextureFormat::RGBA8Unorm, TextureUsage::CopySrc);
    auto destination = createRef<FakeTexture>(8, 8, 1, TextureFormat::RGBA8Unorm, TextureUsage::CopyDst);
    RenderGraphBuilder builder(device);
    const Resource src = builder.Use(source);
    const Resource dst = builder.Use(destination);
    builder.AddPass("copy").Copy(src, dst).Execute([](CopyPassContext& context) { return context.CopyAll(); });
    auto graph = builder.Compile(64, 64);
    REQUIRE(graph);

    {
        auto frame = (*graph)->BeginFrame(64, 64);
        REQUIRE(frame);
        auto resized = (*graph)->BeginFrame(128, 128);
        REQUIRE_FALSE(resized);
        CHECK(resized.error().Code() == ErrorCode::InvalidState);
    }
    REQUIRE((*graph)->BeginFrame(128, 128));
}

TEST_CASE("RenderGraph only creates sampled depth views for texture-binding usage") {
    auto device = createRef<FakeDevice>();
    device->allocate_textures_ = true;
    RenderGraphBuilder builder(device);
    const Resource depth = builder.Transient({
        .format = TextureFormat::Depth32Float,
        .usage = TextureUsage::RenderAttachment,
        .extent = ExtentMode::Fixed(16, 16),
    });
    builder.AddPass("depth").Depth(depth).Execute([](RenderPassContext&) {});
    REQUIRE(builder.Compile(16, 16));
    REQUIRE(device->created_view_descs_.size() == 1);
    CHECK(device->created_view_descs_[0].aspect == TextureAspect::Undefined);
}

TEST_CASE("RenderGraph sampled depth view is depth-only and binding-only") {
    auto device = createRef<FakeDevice>();
    device->allocate_textures_ = true;
    RenderGraphBuilder builder(device);
    const Resource depth = builder.Transient({
        .format = TextureFormat::Depth32Float,
        .usage = TextureUsage::RenderAttachment | TextureUsage::TextureBinding,
        .extent = ExtentMode::Fixed(16, 16),
    });
    builder.AddPass("depth").Depth(depth).Execute([](RenderPassContext&) {});
    REQUIRE(builder.Compile(16, 16));
    REQUIRE(device->created_view_descs_.size() == 2);
    CHECK(device->created_view_descs_[1].aspect == TextureAspect::DepthOnly);
    CHECK(device->created_view_descs_[1].usage == TextureUsage::TextureBinding);
}

TEST_CASE("RenderGraph emits a complete read-only depth attachment descriptor") {
    auto device = createRef<FakeDevice>();
    auto depth = createRef<FakeTexture>(16, 16, 1, TextureFormat::Depth32Float, TextureUsage::RenderAttachment);
    RenderGraphBuilder builder(device);
    const Resource target = builder.Use(depth);
    builder.AddPass("depth").Depth(target, {.write = false}).Execute([](RenderPassContext&) {});
    auto graph = builder.Compile(16, 16);
    REQUIRE(graph);
    auto frame = (*graph)->BeginFrame(16, 16);
    REQUIRE(frame);
    REQUIRE_FALSE(frame->Execute());
    REQUIRE(device->state_.captured_depth_attachment);
    CHECK(device->state_.depth_attachment.depth_read_only);
    CHECK(device->state_.depth_attachment.depth_load_op == LoadOp::Undefined);
    CHECK(device->state_.depth_attachment.depth_store_op == StoreOp::Undefined);
    CHECK(std::isnan(device->state_.depth_attachment.depth_clear_value));
    CHECK(device->state_.depth_attachment.stencil_read_only);
    CHECK(device->state_.depth_attachment.stencil_load_op == LoadOp::Undefined);
    CHECK(device->state_.depth_attachment.stencil_store_op == StoreOp::Undefined);
}

TEST_CASE("Depth attachment defaults leave stencil read-only and unused") {
    const RenderPassDepthStencilAttachmentDesc desc{};
    CHECK(desc.stencil_load_op == LoadOp::Undefined);
    CHECK(desc.stencil_store_op == StoreOp::Undefined);
    CHECK(desc.stencil_read_only);
}
