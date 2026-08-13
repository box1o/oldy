#include <cstring>
#include <atomic>
#include <mutex>
#include <thread>

#include <woki/rhi/command_encoder.hpp>
#include <woki/rhi/compute_pass_encoder.hpp>
#include <woki/rhi/device.hpp>
#include <woki/rhi/objects.hpp>
#include <woki/rhi/queue.hpp>
#include <woki/rhi/render_pass_encoder.hpp>
#include <woki/rhi/swapchain.hpp>
#include <woki/rhi/validation.hpp>

namespace woki::rhi::validation {
namespace {

struct State final {
    explicit State(ValidationRhiDescriptor value)
        : descriptor(std::move(value)) {}

    ValidationRhiDescriptor descriptor;
    std::thread::id owner{std::this_thread::get_id()};
    mutable std::mutex mutex;
    std::vector<std::string> breadcrumbs;

    void Breadcrumb(std::string value) {
        std::lock_guard lock(mutex);
        breadcrumbs.push_back(std::move(value));
        if (breadcrumbs.size() > 32)
            breadcrumbs.erase(breadcrumbs.begin());
    }

    void Report(RhiDiagnosticCode code, std::string message) const {
        if (!descriptor.diagnostic)
            return;
        std::lock_guard lock(mutex);
        descriptor.diagnostic({code, std::move(message), breadcrumbs});
    }

    [[nodiscard]] Result<void> Thread() const {
        if (descriptor.check_thread_ownership && owner != std::this_thread::get_id()) {
            Report(RhiDiagnosticCode::ThreadOwnership, "RHI-VAL-025: object used from a non-owning thread");
            return Err(ErrorCode::ValidationInvalidState, "RHI-VAL-025: object used from a non-owning thread");
        }
        return Ok();
    }
};

class ValidationBuffer final : public Buffer {
public:
    ValidationBuffer(scope<Buffer> inner, ref<State> state)
        : inner_(std::move(inner)),
          state_(std::move(state)) {}

    Result<scope<TexelBufferView>> CreateTexelView(const TexelBufferViewDesc& desc) const override {
        return inner_->CreateTexelView(desc);
    }

    void Destroy() override {
        destroyed_ = true;
        inner_->Destroy();
    }

    const void* GetConstMappedRange(size_t offset, size_t size) const override {
        if (!ValidRange(offset, size))
            return nullptr;
        return inner_->GetConstMappedRange(offset, size);
    }

    void* GetMappedRange(size_t offset, size_t size) override {
        if (!ValidRange(offset, size))
            return nullptr;
        return inner_->GetMappedRange(offset, size);
    }

    BufferMapState GetMapState() const override {
        return inner_->GetMapState();
    }

    u64 GetSize() const override {
        return inner_->GetSize();
    }

    BufferUsage GetUsage() const override {
        return inner_->GetUsage();
    }

    Future MapAsync(
        MapMode mode,
        size_t offset,
        size_t size,
        CallbackMode callback_mode,
        MapAsyncCallback callback
    ) const override {
        if (!ValidRange(offset, size)) {
            if (callback)
                callback(MapAsyncStatus::Error, "RHI-VAL-005: map range out of bounds");
            return {.completed = true, .success = false, .message = "RHI-VAL-005"};
        }
        return inner_->MapAsync(mode, offset, size, callback_mode, std::move(callback));
    }

    Result<void> ReadMappedRange(size_t offset, void* data, size_t size) const override {
        if (!ValidRange(offset, size))
            return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-005: mapped read out of bounds");
        return inner_->ReadMappedRange(offset, data, size);
    }

    void SetLabel(std::string_view label) override {
        inner_->SetLabel(label);
    }

    void Unmap() override {
        inner_->Unmap();
    }

    Result<void> WriteMappedRange(size_t offset, const void* data, size_t size) override {
        if (!ValidRange(offset, size))
            return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-005: mapped write out of bounds");
        return inner_->WriteMappedRange(offset, data, size);
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return inner_->GetNativeHandles();
    }

    Buffer& Inner() const {
        return *inner_;
    }

    bool Destroyed() const {
        return destroyed_;
    }

private:
    bool ValidRange(u64 offset, u64 size) const {
        const bool valid = !destroyed_ && offset <= GetSize() && (size == kWholeSize || size <= GetSize() - offset);
        if (!valid)
            state_->Report(
                destroyed_ ? RhiDiagnosticCode::UseAfterRetire : RhiDiagnosticCode::BufferBounds,
                "RHI-VAL-005: buffer range out of bounds or destroyed"
            );
        return valid;
    }

    scope<Buffer> inner_;
    ref<State> state_;
    bool destroyed_{};
};

Buffer& Unwrap(const Buffer& value) {
    if (auto* wrapped = dynamic_cast<const ValidationBuffer*>(&value))
        return wrapped->Inner();
    return const_cast<Buffer&>(value);
}

class ValidationTextureView final : public TextureView {
public:
    explicit ValidationTextureView(scope<TextureView> inner)
        : inner_(std::move(inner)) {}

    void SetLabel(std::string_view label) override {
        inner_->SetLabel(label);
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return inner_->GetNativeHandles();
    }

    TextureView& Inner() const {
        return *inner_;
    }

private:
    scope<TextureView> inner_;
};

TextureView* Unwrap(TextureView* value) {
    if (auto* wrapped = dynamic_cast<ValidationTextureView*>(value))
        return &wrapped->Inner();
    return value;
}

class ValidationTexture final : public Texture {
public:
    explicit ValidationTexture(scope<Texture> inner)
        : inner_(std::move(inner)) {}

    scope<TextureView> CreateErrorView(const TextureViewDesc& desc) const override {
        return scope<TextureView>(new ValidationTextureView(inner_->CreateErrorView(desc)));
    }

    scope<TextureView> CreateView(const TextureViewDesc& desc) const override {
        return scope<TextureView>(new ValidationTextureView(inner_->CreateView(desc)));
    }

    void Destroy() override {
        destroyed_ = true;
        inner_->Destroy();
    }

    u32 GetDepthOrArrayLayers() const override {
        return inner_->GetDepthOrArrayLayers();
    }

    TextureDimension GetDimension() const override {
        return inner_->GetDimension();
    }

    TextureFormat GetFormat() const override {
        return inner_->GetFormat();
    }

    u32 GetHeight() const override {
        return inner_->GetHeight();
    }

    u32 GetMipLevelCount() const override {
        return inner_->GetMipLevelCount();
    }

    u32 GetSampleCount() const override {
        return inner_->GetSampleCount();
    }

    TextureViewDimension GetTextureBindingViewDimension() const override {
        return inner_->GetTextureBindingViewDimension();
    }

    TextureUsage GetUsage() const override {
        return inner_->GetUsage();
    }

    u32 GetWidth() const override {
        return inner_->GetWidth();
    }

    void Pin(TextureUsage usage) override {
        inner_->Pin(usage);
    }

    void SetLabel(std::string_view label) override {
        inner_->SetLabel(label);
    }

    void SetOwnershipForMemoryDump(u64 owner) override {
        inner_->SetOwnershipForMemoryDump(owner);
    }

    void Unpin() override {
        inner_->Unpin();
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return inner_->GetNativeHandles();
    }

    Texture& Inner() const {
        return *inner_;
    }

private:
    scope<Texture> inner_;
    bool destroyed_{};
};

Texture* Unwrap(Texture* value) {
    if (auto* wrapped = dynamic_cast<ValidationTexture*>(value))
        return &wrapped->Inner();
    return value;
}

class ValidationCommandBuffer final : public CommandBuffer {
public:
    ValidationCommandBuffer(scope<CommandBuffer> inner, ref<State> state)
        : inner_(std::move(inner)),
          state_(std::move(state)) {}

    void SetLabel(std::string_view label) override {
        inner_->SetLabel(label);
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return inner_->GetNativeHandles();
    }

    CommandBuffer& Inner() const {
        return *inner_;
    }

    bool MarkSubmitted() {
        return !submitted_.exchange(true);
    }

private:
    scope<CommandBuffer> inner_;
    ref<State> state_;
    std::atomic_bool submitted_{};
};

class ValidationComputePass final : public ComputePassEncoder {
public:
    ValidationComputePass(scope<ComputePassEncoder> inner, ref<State> state)
        : inner_(std::move(inner)),
          state_(std::move(state)) {}

    ~ValidationComputePass() override {
        if (!ended_)
            state_->Report(RhiDiagnosticCode::EncoderScope, "RHI-VAL-020: compute pass destroyed before End");
    }

    void DispatchWorkgroups(u32 x, u32 y, u32 z) override {
        if (!pipeline_)
            state_->Report(RhiDiagnosticCode::PassState, "RHI-VAL-021: dispatch without pipeline");
        inner_->DispatchWorkgroups(x, y, z);
        state_->Breadcrumb("compute.dispatch");
    }

    void DispatchWorkgroupsIndirect(const Buffer& buffer, u64 offset) override {
        inner_->DispatchWorkgroupsIndirect(Unwrap(buffer), offset);
        state_->Breadcrumb("compute.dispatch_indirect");
    }

    void End() override {
        if (ended_) {
            state_->Report(RhiDiagnosticCode::EncoderScope, "RHI-VAL-020: compute pass ended twice");
            return;
        }
        ended_ = true;
        inner_->End();
    }

    void InsertDebugMarker(std::string_view value) override {
        inner_->InsertDebugMarker(value);
    }

    void PopDebugGroup() override {
        inner_->PopDebugGroup();
    }

    void PushDebugGroup(std::string_view value) override {
        inner_->PushDebugGroup(value);
    }

    void SetBindGroup(u32 i, const BindGroup* g, std::span<const u32> o) override {
        inner_->SetBindGroup(i, g, o);
    }

    void SetImmediates(u32 o, const void* d, size_t s) override {
        inner_->SetImmediates(o, d, s);
    }

    void SetLabel(std::string_view v) override {
        inner_->SetLabel(v);
    }

    void SetPipeline(const ComputePipeline& p) override {
        pipeline_ = true;
        inner_->SetPipeline(p);
    }

    void SetResourceTable(const ResourceTable* t) override {
        inner_->SetResourceTable(t);
    }

    void WriteTimestamp(const QuerySet& q, u32 i) override {
        inner_->WriteTimestamp(q, i);
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return inner_->GetNativeHandles();
    }

private:
    scope<ComputePassEncoder> inner_;
    ref<State> state_;
    bool ended_{};
    bool pipeline_{};
};

class ValidationRenderPass final : public RenderPassEncoder {
public:
    ValidationRenderPass(scope<RenderPassEncoder> inner, ref<State> state)
        : inner_(std::move(inner)),
          state_(std::move(state)) {}

    ~ValidationRenderPass() override {
        if (!ended_)
            state_->Report(RhiDiagnosticCode::EncoderScope, "RHI-VAL-020: render pass destroyed before End");
    }

    void BeginOcclusionQuery(u32 i) override {
        inner_->BeginOcclusionQuery(i);
    }

    void Draw(u32 a, u32 b, u32 c, u32 d) override {
        CheckDraw();
        inner_->Draw(a, b, c, d);
        state_->Breadcrumb("render.draw");
    }

    void DrawIndexed(u32 a, u32 b, u32 c, i32 d, u32 e) override {
        CheckDraw();
        inner_->DrawIndexed(a, b, c, d, e);
        state_->Breadcrumb("render.draw_indexed");
    }

    void DrawIndexedIndirect(const Buffer& b, u64 o) override {
        CheckDraw();
        inner_->DrawIndexedIndirect(Unwrap(b), o);
    }

    void DrawIndirect(const Buffer& b, u64 o) override {
        CheckDraw();
        inner_->DrawIndirect(Unwrap(b), o);
    }

    void End() override {
        if (ended_) {
            state_->Report(RhiDiagnosticCode::EncoderScope, "RHI-VAL-020: render pass ended twice");
            return;
        }
        ended_ = true;
        inner_->End();
    }

    void EndOcclusionQuery() override {
        inner_->EndOcclusionQuery();
    }

    void ExecuteBundles(std::span<RenderBundle* const> b) override {
        inner_->ExecuteBundles(b);
    }

    void InsertDebugMarker(std::string_view v) override {
        inner_->InsertDebugMarker(v);
    }

    void MultiDrawIndexedIndirect(const Buffer& b, u64 o, u32 c, const Buffer* d, u64 e) override {
        CheckDraw();
        inner_->MultiDrawIndexedIndirect(Unwrap(b), o, c, d ? &Unwrap(*d) : nullptr, e);
        state_->Breadcrumb("render.multi_draw_indexed_indirect");
    }

    void MultiDrawIndirect(const Buffer& b, u64 o, u32 c, const Buffer* d, u64 e) override {
        CheckDraw();
        inner_->MultiDrawIndirect(Unwrap(b), o, c, d ? &Unwrap(*d) : nullptr, e);
        state_->Breadcrumb("render.multi_draw_indirect");
    }

    void PixelLocalStorageBarrier() override {
        inner_->PixelLocalStorageBarrier();
    }

    void PopDebugGroup() override {
        inner_->PopDebugGroup();
    }

    void PushDebugGroup(std::string_view v) override {
        inner_->PushDebugGroup(v);
    }

    void SetBindGroup(u32 i, const BindGroup* g, std::span<const u32> o) override {
        inner_->SetBindGroup(i, g, o);
    }

    void SetBlendConstant(const Color& c) override {
        inner_->SetBlendConstant(c);
    }

    void SetImmediates(u32 o, const void* d, size_t s) override {
        inner_->SetImmediates(o, d, s);
    }

    void SetIndexBuffer(const Buffer& b, IndexFormat f, u64 o, u64 s) override {
        inner_->SetIndexBuffer(Unwrap(b), f, o, s);
    }

    void SetLabel(std::string_view v) override {
        inner_->SetLabel(v);
    }

    void SetPipeline(const RenderPipeline& p) override {
        pipeline_ = true;
        inner_->SetPipeline(p);
    }

    void SetResourceTable(const ResourceTable* t) override {
        inner_->SetResourceTable(t);
    }

    void SetScissorRect(u32 a, u32 b, u32 c, u32 d) override {
        inner_->SetScissorRect(a, b, c, d);
    }

    void SetStencilReference(u32 v) override {
        inner_->SetStencilReference(v);
    }

    void SetVertexBuffer(u32 i, const Buffer* b, u64 o, u64 s) override {
        inner_->SetVertexBuffer(i, b ? &Unwrap(*b) : nullptr, o, s);
    }

    void SetViewport(f32 a, f32 b, f32 c, f32 d, f32 e, f32 f) override {
        inner_->SetViewport(a, b, c, d, e, f);
    }

    void WriteTimestamp(const QuerySet& q, u32 i) override {
        inner_->WriteTimestamp(q, i);
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return inner_->GetNativeHandles();
    }

private:
    void CheckDraw() {
        if (!pipeline_)
            state_->Report(RhiDiagnosticCode::PassState, "RHI-VAL-021: draw without pipeline");
    }

    scope<RenderPassEncoder> inner_;
    ref<State> state_;
    bool ended_{};
    bool pipeline_{};
};

class ValidationCommandEncoder final : public CommandEncoder {
public:
    ValidationCommandEncoder(scope<CommandEncoder> inner, ref<State> state)
        : inner_(std::move(inner)),
          state_(std::move(state)) {}

    Result<scope<ComputePassEncoder>> BeginComputePass(const ComputePassDesc& d) override {
        TRY_VOID(CheckOpen());
        scope<ComputePassEncoder> p;
        TRY_ASSIGN(p, inner_->BeginComputePass(d));
        state_->Breadcrumb("compute.begin");
        return Ok(scope<ComputePassEncoder>(new ValidationComputePass(std::move(p), state_)));
    }

    Result<scope<RenderPassEncoder>> BeginRenderPass(const RenderPassDesc& d) override {
        TRY_VOID(CheckOpen());
        scope<RenderPassEncoder> p;
        TRY_ASSIGN(p, inner_->BeginRenderPass(d));
        state_->Breadcrumb("render.begin");
        return Ok(scope<RenderPassEncoder>(new ValidationRenderPass(std::move(p), state_)));
    }

    Result<scope<RenderPassEncoder>> BeginRenderPass(const RenderPassDescTyped& d) override {
        TRY_VOID(CheckOpen());
        std::vector<RenderPassColorAttachmentDesc> colors(d.color_attachments.begin(), d.color_attachments.end());
        for (auto& c : colors) {
            c.view = Unwrap(c.view);
            c.resolve_target = Unwrap(c.resolve_target);
        }
        auto copy = d;
        copy.color_attachments = colors;
        scope<RenderPassEncoder> p;
        TRY_ASSIGN(p, inner_->BeginRenderPass(copy));
        state_->Breadcrumb("render.begin");
        return Ok(scope<RenderPassEncoder>(new ValidationRenderPass(std::move(p), state_)));
    }

    Result<void> ClearBuffer(const Buffer& b, u64 o, u64 s) override {
        TRY_VOID(CheckOpen());
        return inner_->ClearBuffer(Unwrap(b), o, s);
    }

    Result<void> CopyBufferToBuffer(const Buffer& a, u64 b, const Buffer& c, u64 d, u64 e) override {
        TRY_VOID(CheckOpen());
        return inner_->CopyBufferToBuffer(Unwrap(a), b, Unwrap(c), d, e);
    }

    Result<void> CopyBufferToTexture(
        const TexelCopyBufferInfo& s,
        const TexelCopyTextureInfo& d,
        const Extent3D& e
    ) override {
        auto source = s;
        auto destination = d;
        source.buffer = source.buffer ? &Unwrap(*source.buffer) : nullptr;
        destination.texture = Unwrap(destination.texture);
        return inner_->CopyBufferToTexture(source, destination, e);
    }

    Result<void> CopyTextureToBuffer(
        const TexelCopyTextureInfo& s,
        const TexelCopyBufferInfo& d,
        const Extent3D& e
    ) override {
        auto source = s;
        auto destination = d;
        source.texture = Unwrap(source.texture);
        destination.buffer = destination.buffer ? &Unwrap(*destination.buffer) : nullptr;
        return inner_->CopyTextureToBuffer(source, destination, e);
    }

    Result<void> CopyTextureToTexture(
        const TexelCopyTextureInfo& s,
        const TexelCopyTextureInfo& d,
        const Extent3D& e
    ) override {
        auto source = s;
        auto destination = d;
        source.texture = Unwrap(source.texture);
        destination.texture = Unwrap(destination.texture);
        return inner_->CopyTextureToTexture(source, destination, e);
    }

    Result<scope<CommandBuffer>> Finish(const CommandBufferDesc& d) override {
        TRY_VOID(CheckOpen());
        finished_ = true;
        scope<CommandBuffer> b;
        TRY_ASSIGN(b, inner_->Finish(d));
        state_->Breadcrumb("encoder.finish");
        return Ok(scope<CommandBuffer>(new ValidationCommandBuffer(std::move(b), state_)));
    }

    void InjectValidationError(std::string_view v) override {
        inner_->InjectValidationError(v);
    }

    void InsertDebugMarker(std::string_view v) override {
        inner_->InsertDebugMarker(v);
    }

    void PopDebugGroup() override {
        inner_->PopDebugGroup();
    }

    void PushDebugGroup(std::string_view v) override {
        inner_->PushDebugGroup(v);
    }

    Result<void> ResolveQuerySet(const QuerySet& q, u32 a, u32 b, const Buffer& d, u64 o) override {
        return inner_->ResolveQuerySet(q, a, b, Unwrap(d), o);
    }

    void SetLabel(std::string_view v) override {
        inner_->SetLabel(v);
    }

    Result<void> WriteBuffer(const Buffer& b, u64 o, const u8* d, u64 s) override {
        return inner_->WriteBuffer(Unwrap(b), o, d, s);
    }

    Result<void> WriteTimestamp(const QuerySet& q, u32 i) override {
        return inner_->WriteTimestamp(q, i);
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return inner_->GetNativeHandles();
    }

private:
    Result<void> CheckOpen() {
        TRY_VOID(state_->Thread());
        if (finished_) {
            state_->Report(RhiDiagnosticCode::EncoderScope, "RHI-VAL-020: encoder already finished");
            return Err(ErrorCode::ValidationInvalidState, "RHI-VAL-020: encoder already finished");
        }
        return Ok();
    }

    scope<CommandEncoder> inner_;
    ref<State> state_;
    bool finished_{};
};

class ValidationQueue final : public Queue {
public:
    ValidationQueue(Queue& inner, ref<State> state)
        : inner_(inner),
          state_(std::move(state)) {}

    Result<void> CopyExternalTextureForBrowser(
        const ImageCopyExternalTexture& a,
        const TexelCopyTextureInfo& b,
        const Extent3D& c,
        const CopyTextureForBrowserOptions& d
    ) const override {
        return inner_.CopyExternalTextureForBrowser(a, b, c, d);
    }

    Result<void> CopyTextureForBrowser(
        const TexelCopyTextureInfo& a,
        const TexelCopyTextureInfo& b,
        const Extent3D& c,
        const CopyTextureForBrowserOptions& d
    ) const override {
        return inner_.CopyTextureForBrowser(a, b, c, d);
    }

    Future OnSubmittedWorkDone(CallbackMode m, QueueWorkDoneCallback c) const override {
        return inner_.OnSubmittedWorkDone(m, std::move(c));
    }

    void SetLabel(std::string_view l) const override {
        inner_.SetLabel(l);
    }

    Result<SubmissionTicket> Submit(std::span<CommandBuffer* const> commands) const override {
        TRY_VOID(state_->Thread());
        std::vector<CommandBuffer*> unwrapped;
        unwrapped.reserve(commands.size());
        for (auto* command : commands) {
            auto* wrapped = dynamic_cast<ValidationCommandBuffer*>(command);
            if (!wrapped) {
                state_->Report(
                    RhiDiagnosticCode::DeviceOwnership,
                    "RHI-VAL-030: command buffer did not originate from this validation device"
                );
                return Err(ErrorCode::ValidationInvalidState, "RHI-VAL-030: command buffer ownership mismatch");
            }
            if (!wrapped->MarkSubmitted())
                return Err(ErrorCode::ValidationInvalidState, "RHI-VAL-031: command buffer already submitted");
            unwrapped.push_back(&wrapped->Inner());
        }
        state_->Breadcrumb("queue.submit");
        return inner_.Submit(unwrapped);
    }

    SubmissionEpoch CompletedSubmission() const noexcept override {
        return inner_.CompletedSubmission();
    }

    SubmissionTrackingStatus SubmissionTracking() const noexcept override {
        return inner_.SubmissionTracking();
    }

    Result<void> WriteBuffer(const Buffer& b, u64 o, const void* d, u64 s) const override {
        return inner_.WriteBuffer(Unwrap(b), o, d, s);
    }

    Result<void> WriteTexture(
        const TexelCopyTextureInfo& d,
        const void* p,
        u64 s,
        const TexelCopyBufferLayout& l,
        const Extent3D& e
    ) const override {
        auto copy = d;
        copy.texture = Unwrap(copy.texture);
        return inner_.WriteTexture(copy, p, s, l, e);
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return inner_.GetNativeHandles();
    }

private:
    Queue& inner_;
    ref<State> state_;
};

class ValidationDevice final : public Device {
public:
    ValidationDevice(ref<Device> inner, ValidationRhiDescriptor descriptor)
        : inner_(std::move(inner)),
          state_(createRef<State>(std::move(descriptor))),
          queue_(inner_->GetQueue(), state_) {}

    Result<scope<BindGroup>> CreateBindGroup(const BindGroupDesc& d) override {
        return inner_->CreateBindGroup(d);
    }

    Result<scope<BindGroupLayout>> CreateBindGroupLayout(const BindGroupLayoutDesc& d) override {
        return inner_->CreateBindGroupLayout(d);
    }

    Result<scope<Buffer>> CreateBuffer(const BufferDesc& d) override {
        if (d.size == 0 || d.size > Capabilities().GetLimits().max_buffer_size || d.usage == BufferUsage::None)
            return Fail<Buffer>(RhiDiagnosticCode::DescriptorLimit, "RHI-VAL-002: invalid buffer descriptor");
        scope<Buffer> b;
        TRY_ASSIGN(b, inner_->CreateBuffer(d));
        return Ok(scope<Buffer>(new ValidationBuffer(std::move(b), state_)));
    }

    Result<scope<CommandEncoder>> CreateCommandEncoder(const CommandEncoderDesc& d) override {
        scope<CommandEncoder> e;
        TRY_ASSIGN(e, inner_->CreateCommandEncoder(d));
        return Ok(scope<CommandEncoder>(new ValidationCommandEncoder(std::move(e), state_)));
    }

    Result<scope<ComputePipeline>> CreateComputePipeline(const ComputePipelineDesc& d) override {
        return inner_->CreateComputePipeline(d);
    }

    Future CreateComputePipelineAsync(
        const ComputePipelineDesc& d,
        CallbackMode m,
        CreateComputePipelineCallback c
    ) override {
        return inner_->CreateComputePipelineAsync(d, m, std::move(c));
    }

    Result<scope<Buffer>> CreateErrorBuffer(const BufferDesc& d) override {
        return inner_->CreateErrorBuffer(d);
    }

    Result<scope<ExternalTexture>> CreateErrorExternalTexture() override {
        return inner_->CreateErrorExternalTexture();
    }

    Result<scope<ShaderModule>> CreateErrorShaderModule(const ShaderModuleDesc& d, std::string_view m) override {
        return inner_->CreateErrorShaderModule(d, m);
    }

    Result<scope<Texture>> CreateErrorTexture(const TextureDesc& d) override {
        return inner_->CreateErrorTexture(d);
    }

    Result<scope<ExternalTexture>> CreateExternalTexture(const ExternalTextureDesc& d) override {
        return inner_->CreateExternalTexture(d);
    }

    Result<scope<PipelineLayout>> CreatePipelineLayout(const PipelineLayoutDesc& d) override {
        return inner_->CreatePipelineLayout(d);
    }

    Result<scope<QuerySet>> CreateQuerySet(const QuerySetDesc& d) override {
        if (d.type == QueryType::Timestamp && !Capabilities().Has(CapabilityFeature::TimestampQueries))
            return Fail<QuerySet>(RhiDiagnosticCode::QueryIllegal, "RHI-VAL-017: timestamp queries unsupported");
        return inner_->CreateQuerySet(d);
    }

    Result<scope<RenderBundleEncoder>> CreateRenderBundleEncoder(const RenderBundleEncoderDesc& d) override {
        return inner_->CreateRenderBundleEncoder(d);
    }

    Result<scope<RenderPipeline>> CreateRenderPipeline(const RenderPipelineDesc& d) override {
        return inner_->CreateRenderPipeline(d);
    }

    Result<scope<RenderPipeline>> CreateRenderPipeline(const RenderPipelineDescTyped& d) override {
        for (const auto& t : d.fragment ? d.fragment->targets : std::span<const ColorTargetStateDesc>{})
            if (!Capabilities().Supports(t.format, TextureUsage::RenderAttachment, d.multisample.count))
                return Fail<RenderPipeline>(
                    RhiDiagnosticCode::PipelineTarget,
                    "RHI-VAL-015: unsupported pipeline target/sample count"
                );
        return inner_->CreateRenderPipeline(d);
    }

    Future CreateRenderPipelineAsync(
        const RenderPipelineDesc& d,
        CallbackMode m,
        CreateRenderPipelineCallback c
    ) override {
        return inner_->CreateRenderPipelineAsync(d, m, std::move(c));
    }

    Result<scope<ResourceTable>> CreateResourceTable(const ResourceTableDesc& d) override {
        return inner_->CreateResourceTable(d);
    }

    Result<scope<Sampler>> CreateSampler(const SamplerDesc& d) override {
        return inner_->CreateSampler(d);
    }

    Result<scope<ShaderModule>> CreateShaderModule(const ShaderModuleDesc& d) override {
        return inner_->CreateShaderModule(d);
    }

    Result<scope<Texture>> CreateTexture(const TextureDesc& d) override {
        if (d.size.width == 0 || d.size.height == 0 || d.size.depth_or_array_layers == 0
            || !Capabilities().Supports(d.format, d.usage, d.sample_count))
            return Fail<Texture>(
                RhiDiagnosticCode::TextureDescriptor,
                "RHI-VAL-003: unsupported texture descriptor '" + d.label
                    + "' (format=" + std::to_string(static_cast<u32>(d.format))
                    + ", usage=" + std::to_string(static_cast<u64>(d.usage))
                    + ", samples=" + std::to_string(d.sample_count) + ", size=" + std::to_string(d.size.width) + "x"
                    + std::to_string(d.size.height) + "x" + std::to_string(d.size.depth_or_array_layers) + ")"
            );
        scope<Texture> t;
        TRY_ASSIGN(t, inner_->CreateTexture(d));
        return Ok(scope<Texture>(new ValidationTexture(std::move(t))));
    }

    Result<scope<Swapchain>> CreateSwapchain(ref<Surface> s, SwapchainDesc d) override {
        return inner_->CreateSwapchain(std::move(s), std::move(d));
    }

    void Destroy() override {
        inner_->Destroy();
    }

    void ForceLoss(DeviceLostReason r, std::string_view m) override {
        inner_->ForceLoss(r, m);
    }

    Result<scope<Adapter>> GetAdapter() const override {
        return inner_->GetAdapter();
    }

    Result<void> GetAdapterInfo(AdapterInfo& i) const override {
        return inner_->GetAdapterInfo(i);
    }

    Result<void> GetAHardwareBufferProperties(void* h, void* p) const override {
        return inner_->GetAHardwareBufferProperties(h, p);
    }

    void GetFeatures(SupportedFeatures& f) const override {
        inner_->GetFeatures(f);
    }

    Result<void> GetLimits(Limits& l) const override {
        return inner_->GetLimits(l);
    }

    Future GetLostFuture() const override {
        return inner_->GetLostFuture();
    }

    Queue& GetQueue() const noexcept override {
        return const_cast<ValidationQueue&>(queue_);
    }

    bool HasFeature(FeatureName f) const noexcept override {
        return inner_->HasFeature(f);
    }

    const DeviceCapabilities& Capabilities() const noexcept override {
        return inner_->Capabilities();
    }

    bool IsLost() const noexcept override {
        return inner_->IsLost();
    }

    DeviceLostReason LossReason() const noexcept override {
        return inner_->LossReason();
    }

    Result<scope<SharedBufferMemory>> ImportSharedBufferMemory(const SharedBufferMemoryDesc& d) override {
        return inner_->ImportSharedBufferMemory(d);
    }

    Result<scope<SharedFence>> ImportSharedFence(const SharedFenceDesc& d) override {
        return inner_->ImportSharedFence(d);
    }

    Result<scope<SharedTextureMemory>> ImportSharedTextureMemory(const SharedTextureMemoryDesc& d) override {
        return inner_->ImportSharedTextureMemory(d);
    }

    void InjectError(ErrorType t, std::string_view m) override {
        inner_->InjectError(t, m);
    }

    Future PopErrorScope(CallbackMode m, PopErrorScopeCallback c) const override {
        return inner_->PopErrorScope(m, std::move(c));
    }

    void PushErrorScope(ErrorFilter f) override {
        inner_->PushErrorScope(f);
    }

    void SetLabel(std::string_view l) override {
        inner_->SetLabel(l);
    }

    void SetLoggingCallback(LoggingCallback c) override {
        inner_->SetLoggingCallback(std::move(c));
    }

    void Tick() const noexcept override {
        inner_->Tick();
    }

    void ValidateTextureDescriptor(const TextureDesc& d) const override {
        inner_->ValidateTextureDescriptor(d);
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return inner_->GetNativeHandles();
    }

private:
    template <typename T>
    Result<scope<T>> Fail(RhiDiagnosticCode code, std::string message) {
        state_->Report(code, message);
        return Err(ErrorCode::ValidationInvalidState, std::move(message));
    }

    ref<Device> inner_;
    ref<State> state_;
    mutable ValidationQueue queue_;
};

} // namespace
} // namespace woki::rhi::validation

namespace woki::rhi {
ref<Device> CreateValidationDevice(ref<Device> device, ValidationRhiDescriptor descriptor) {
    if (!device)
        return nullptr;
    return createRef<validation::ValidationDevice>(std::move(device), std::move(descriptor));
}
} // namespace woki::rhi
