#include <algorithm>
#include <atomic>
#include <cstring>
#include <functional>
#include <mutex>
#include <unordered_map>

#include <woki/rhi/command_encoder.hpp>
#include <woki/rhi/compute_pass_encoder.hpp>
#include <woki/rhi/null.hpp>
#include <woki/rhi/render_pass_encoder.hpp>
#include <woki/rhi/swapchain.hpp>

#include "null_backend.hpp"

namespace woki::rhi::null {
namespace {

u32 BytesPerTexel(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8Unorm:
        case TextureFormat::R8Snorm:
        case TextureFormat::R8Uint:
        case TextureFormat::R8Sint:
            return 1;
        case TextureFormat::R16Float:
        case TextureFormat::R16Uint:
        case TextureFormat::R16Sint:
        case TextureFormat::RG8Unorm:
            return 2;
        case TextureFormat::RGBA16Float:
            return 8;
        case TextureFormat::RGBA32Float:
        case TextureFormat::RGBA32Uint:
        case TextureFormat::RGBA32Sint:
            return 16;
        default:
            return 4;
    }
}

Limits DefaultLimits() {
    Limits limits;
    limits.max_texture_dimension_1d = 8192;
    limits.max_texture_dimension_2d = 8192;
    limits.max_texture_dimension_3d = 2048;
    limits.max_texture_array_layers = 256;
    limits.max_bind_groups = 4;
    limits.max_bind_groups_plus_vertex_buffers = 12;
    limits.max_bindings_per_bind_group = 1000;
    limits.max_dynamic_uniform_buffers_per_pipeline_layout = 8;
    limits.max_dynamic_storage_buffers_per_pipeline_layout = 4;
    limits.max_sampled_textures_per_shader_stage = 16;
    limits.max_samplers_per_shader_stage = 16;
    limits.max_storage_buffers_per_shader_stage = 8;
    limits.max_storage_textures_per_shader_stage = 4;
    limits.max_uniform_buffers_per_shader_stage = 12;
    limits.max_uniform_buffer_binding_size = 65536;
    limits.max_storage_buffer_binding_size = 128ULL * 1024ULL * 1024ULL;
    limits.min_uniform_buffer_offset_alignment = 256;
    limits.min_storage_buffer_offset_alignment = 256;
    limits.max_vertex_buffers = 8;
    limits.max_buffer_size = 256ULL * 1024ULL * 1024ULL;
    limits.max_vertex_attributes = 16;
    limits.max_vertex_buffer_array_stride = 2048;
    limits.max_inter_stage_shader_variables = 16;
    limits.max_color_attachments = 8;
    limits.max_color_attachment_bytes_per_sample = 32;
    limits.max_compute_workgroup_storage_size = 16384;
    limits.max_compute_invocations_per_workgroup = 256;
    limits.max_compute_workgroup_size_x = 256;
    limits.max_compute_workgroup_size_y = 256;
    limits.max_compute_workgroup_size_z = 64;
    limits.max_compute_workgroups_per_dimension = 65535;
    limits.max_immediate_size = 256;
    return limits;
}

DeviceCapabilities DefaultCapabilities() {
    SupportedFeatures features{{FeatureName::TimestampQuery, FeatureName::IndirectFirstInstance, FeatureName::MultiDrawIndirect}};
    constexpr TextureUsage color = TextureUsage::CopySrc | TextureUsage::CopyDst | TextureUsage::TextureBinding | TextureUsage::StorageBinding | TextureUsage::RenderAttachment;
    constexpr TextureUsage depth = TextureUsage::TextureBinding | TextureUsage::RenderAttachment;
    const TextureFormatCapabilities formats[] = {
        {TextureFormat::R8Unorm, color, {1}, true, false},
        {TextureFormat::RG8Unorm, color, {1}, true, false},
        {TextureFormat::RGBA8Unorm, color, {1, 4}, true, true},
        {TextureFormat::RGBA8UnormSrgb, color, {1, 4}, true, true},
        {TextureFormat::BGRA8Unorm, color, {1, 4}, true, true},
        {TextureFormat::BGRA8UnormSrgb, color, {1, 4}, true, true},
        {TextureFormat::R32Uint, color, {1}, false, false},
        {TextureFormat::R32Float, color, {1}, true, false},
        {TextureFormat::RGBA16Float, color, {1, 4}, true, true},
        {TextureFormat::Depth24Plus, depth, {1, 4}, false, false},
        {TextureFormat::Depth24PlusStencil8, depth, {1, 4}, false, false},
        {TextureFormat::Depth32Float, depth, {1, 4}, false, false},
    };
    return NormalizeCapabilities(features, DefaultLimits(), formats, true, true);
}

SupportedFeatures LegacyFeatures(const DeviceCapabilities& capabilities) {
    SupportedFeatures result;
    if (capabilities.Has(CapabilityFeature::TimestampQueries))
        result.values.push_back(FeatureName::TimestampQuery);
    if (capabilities.Has(CapabilityFeature::IndirectDraw))
        result.values.push_back(FeatureName::IndirectFirstInstance);
    if (capabilities.Has(CapabilityFeature::IndirectCount))
        result.values.push_back(FeatureName::MultiDrawIndirect);
    for (const auto compression : capabilities.Compression()) {
        if (compression == TextureCompression::BC)
            result.values.push_back(FeatureName::TextureCompressionBC);
        if (compression == TextureCompression::ETC2)
            result.values.push_back(FeatureName::TextureCompressionETC2);
        if (compression == TextureCompression::ASTC)
            result.values.push_back(FeatureName::TextureCompressionASTC);
    }
    return result;
}

struct State final {
    mutable std::mutex mutex;
    std::vector<std::string> log;
    std::atomic<u64> submitted{};
    std::atomic<u64> completed{};
    std::atomic_bool lost{};
    std::atomic<DeviceLostReason> loss_reason{DeviceLostReason::Unknown};
    DeviceLostCallback lost_callback;
    bool immediate{true};

    void Record(std::string value) {
        std::lock_guard lock(mutex);
        log.push_back(std::move(value));
    }
};

class NullBuffer final : public Buffer {
public:
    explicit NullBuffer(BufferDesc desc)
        : desc_(std::move(desc)),
          bytes_(static_cast<size_t>(desc_.size)),
          mapped_(desc_.mapped_at_creation ? BufferMapState::Mapped : BufferMapState::Unmapped) {}

    Result<scope<TexelBufferView>> CreateTexelView(const TexelBufferViewDesc&) const override {
        return Err(ErrorCode::GraphicsUnsupportedApi, "NullRHI texel views are not implemented");
    }

    void Destroy() override {
        destroyed_ = true;
        bytes_.clear();
    }

    const void* GetConstMappedRange(size_t offset, size_t size) const override {
        return Range(offset, size) ? bytes_.data() + offset : nullptr;
    }

    void* GetMappedRange(size_t offset, size_t size) override {
        return Range(offset, size) ? bytes_.data() + offset : nullptr;
    }

    BufferMapState GetMapState() const override {
        return mapped_;
    }

    u64 GetSize() const override {
        return desc_.size;
    }

    BufferUsage GetUsage() const override {
        return desc_.usage;
    }

    Future MapAsync(MapMode, size_t offset, size_t size, CallbackMode, MapAsyncCallback callback) const override {
        Future result{.id = 1, .completed = true, .success = Range(offset, size)};
        mapped_ = result.success ? BufferMapState::Mapped : BufferMapState::Unmapped;
        if (callback)
            callback(result.success ? MapAsyncStatus::Success : MapAsyncStatus::Error, result.success ? "" : "RHI-VAL-005: map range is out of bounds");
        return result;
    }

    Result<void> ReadMappedRange(size_t offset, void* data, size_t size) const override {
        if (mapped_ != BufferMapState::Mapped || data == nullptr || !Range(offset, size))
            return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-005: invalid mapped read");
        std::memcpy(data, bytes_.data() + offset, size);
        return Ok();
    }

    void SetLabel(std::string_view label) override {
        desc_.label = label;
    }

    void Unmap() override {
        mapped_ = BufferMapState::Unmapped;
    }

    Result<void> WriteMappedRange(size_t offset, const void* data, size_t size) override {
        if (mapped_ != BufferMapState::Mapped || data == nullptr || !Range(offset, size))
            return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-005: invalid mapped write");
        std::memcpy(bytes_.data() + offset, data, size);
        return Ok();
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return {.resource = const_cast<NullBuffer*>(this)};
    }

    bool Range(u64 offset, u64 size) const {
        return !destroyed_ && offset <= bytes_.size() && (size == kWholeSize || size <= bytes_.size() - offset);
    }

    std::vector<u8>& Bytes() {
        return bytes_;
    }

    const std::vector<u8>& Bytes() const {
        return bytes_;
    }

private:
    mutable BufferDesc desc_;
    std::vector<u8> bytes_;
    mutable BufferMapState mapped_;
    bool destroyed_{};
};

class NullTexture;

class NullTextureView final : public TextureView {
public:
    explicit NullTextureView(NullTexture* texture)
        : texture_(texture) {}

    void SetLabel(std::string_view label) override {
        label_ = label;
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return {.resource = const_cast<NullTextureView*>(this)};
    }

    NullTexture* TextureObject() const {
        return texture_;
    }

private:
    NullTexture* texture_{};
    std::string label_;
};

class NullTexture final : public Texture {
public:
    explicit NullTexture(TextureDesc desc)
        : desc_(std::move(desc)),
          bytes_(static_cast<size_t>(desc_.size.width) * desc_.size.height * desc_.size.depth_or_array_layers * BytesPerTexel(desc_.format)) {}

    scope<TextureView> CreateErrorView(const TextureViewDesc&) const override {
        return createScope<NullTextureView>(const_cast<NullTexture*>(this));
    }

    scope<TextureView> CreateView(const TextureViewDesc&) const override {
        return createScope<NullTextureView>(const_cast<NullTexture*>(this));
    }

    void Destroy() override {
        destroyed_ = true;
        bytes_.clear();
    }

    u32 GetDepthOrArrayLayers() const override {
        return desc_.size.depth_or_array_layers;
    }

    TextureDimension GetDimension() const override {
        return desc_.dimension;
    }

    TextureFormat GetFormat() const override {
        return desc_.format;
    }

    u32 GetHeight() const override {
        return desc_.size.height;
    }

    u32 GetMipLevelCount() const override {
        return desc_.mip_level_count;
    }

    u32 GetSampleCount() const override {
        return desc_.sample_count;
    }

    TextureViewDimension GetTextureBindingViewDimension() const override {
        return desc_.dimension == TextureDimension::e3D ? TextureViewDimension::e3D : TextureViewDimension::e2D;
    }

    TextureUsage GetUsage() const override {
        return desc_.usage;
    }

    u32 GetWidth() const override {
        return desc_.size.width;
    }

    void Pin(TextureUsage) override {}

    void SetLabel(std::string_view label) override {
        desc_.label = label;
    }

    void SetOwnershipForMemoryDump(u64) override {}

    void Unpin() override {}

    NativeHandles GetNativeHandles() const noexcept override {
        return {.resource = const_cast<NullTexture*>(this)};
    }

    bool Region(const Origin3D& origin, const Extent3D& extent) const {
        return !destroyed_ && origin.x + extent.width <= desc_.size.width && origin.y + extent.height <= desc_.size.height && origin.z + extent.depth_or_array_layers <= desc_.size.depth_or_array_layers;
    }

    u64 Offset(u32 x, u32 y, u32 z) const {
        return (static_cast<u64>(z) * desc_.size.height * desc_.size.width + static_cast<u64>(y) * desc_.size.width + x) * BytesPerTexel(desc_.format);
    }

    std::vector<u8>& Bytes() {
        return bytes_;
    }

    const std::vector<u8>& Bytes() const {
        return bytes_;
    }

private:
    mutable TextureDesc desc_;
    std::vector<u8> bytes_;
    bool destroyed_{};
};

template <typename Base>
class LabelObject : public Base {
public:
    void SetLabel(std::string_view label) override {
        label_ = label;
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return {.resource = const_cast<LabelObject*>(this)};
    }

private:
    std::string label_;
};

class NullBindGroup final : public LabelObject<BindGroup> {};

class NullBindGroupLayout final : public LabelObject<BindGroupLayout> {};

class NullPipelineLayout final : public LabelObject<PipelineLayout> {};

class NullSampler final : public LabelObject<Sampler> {};

class NullRenderBundle final : public LabelObject<RenderBundle> {};

class NullComputePipeline final : public LabelObject<ComputePipeline> {
public:
    scope<BindGroupLayout> GetBindGroupLayout(u32) const override {
        return createScope<NullBindGroupLayout>();
    }
};

class NullRenderPipeline final : public LabelObject<RenderPipeline> {
public:
    scope<BindGroupLayout> GetBindGroupLayout(u32) const override {
        return createScope<NullBindGroupLayout>();
    }
};

class NullShaderModule final : public LabelObject<ShaderModule> {
public:
    Future GetCompilationInfo(CallbackMode, ShaderModuleCompilationInfoCallback callback) const override {
        if (callback)
            callback(CompilationInfoRequestStatus::Success, nullptr, {});
        return {.id = 1, .completed = true, .success = true};
    }
};

class NullExternalTexture final : public LabelObject<ExternalTexture> {
public:
    void Destroy() override {}

    void Expire() override {}

    void Refresh() override {}
};

class NullQuerySet final : public LabelObject<QuerySet> {
public:
    NullQuerySet(QueryType type, u32 count)
        : type_(type),
          count_(count) {}

    void Destroy() override {
        destroyed_ = true;
    }

    u32 GetCount() const override {
        return count_;
    }

    QueryType GetType() const override {
        return type_;
    }

private:
    QueryType type_;
    u32 count_;
    bool destroyed_{};
};

class NullResourceTable final : public LabelObject<ResourceTable> {
public:
    explicit NullResourceTable(u32 size)
        : values_(size) {}

    void Destroy() override {
        values_.clear();
    }

    u32 GetSize() const override {
        return static_cast<u32>(values_.size());
    }

    u32 InsertBinding(const BindingResourceDesc& value) override {
        values_.push_back(value);
        return static_cast<u32>(values_.size() - 1);
    }

    Result<void> RemoveBinding(u32 slot) override {
        if (slot >= values_.size())
            return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-005: table slot out of range");
        values_[slot] = {};
        return Ok();
    }

    Result<void> Update(u32 slot, const BindingResourceDesc& value) override {
        if (slot >= values_.size())
            return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-005: table slot out of range");
        values_[slot] = value;
        return Ok();
    }

private:
    std::vector<BindingResourceDesc> values_;
};

struct EncoderState {
    ref<State> state;
    std::vector<std::function<Result<void>()>> commands;
    bool pass_active{};
    bool finished{};
};

class NullComputePass final : public ComputePassEncoder {
public:
    explicit NullComputePass(ref<EncoderState> state)
        : state_(std::move(state)) {}

    ~NullComputePass() override {
        if (!ended_)
            End();
    }

    void DispatchWorkgroups(u32 x, u32 y, u32 z) override {
        state_->state->Record("compute.dispatch " + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z));
    }

    void DispatchWorkgroupsIndirect(const Buffer&, u64) override {
        state_->state->Record("compute.dispatch_indirect");
    }

    void End() override {
        if (!ended_) {
            ended_ = true;
            state_->pass_active = false;
            state_->state->Record("compute.end");
        }
    }

    void InsertDebugMarker(std::string_view v) override {
        state_->state->Record("marker " + std::string(v));
    }

    void PopDebugGroup() override {}

    void PushDebugGroup(std::string_view) override {}

    void SetBindGroup(u32, const BindGroup*, std::span<const u32>) override {}

    void SetImmediates(u32, const void*, size_t) override {}

    void SetLabel(std::string_view) override {}

    void SetPipeline(const ComputePipeline&) override {
        pipeline_ = true;
    }

    void SetResourceTable(const ResourceTable*) override {}

    void WriteTimestamp(const QuerySet&, u32) override {}

    NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }

private:
    ref<EncoderState> state_;
    bool ended_{};
    bool pipeline_{};
};

class NullRenderPass final : public RenderPassEncoder {
public:
    explicit NullRenderPass(ref<EncoderState> state)
        : state_(std::move(state)) {}

    ~NullRenderPass() override {
        if (!ended_)
            End();
    }

    void BeginOcclusionQuery(u32) override {}

    void Draw(u32 v, u32 i, u32, u32) override {
        state_->state->Record("render.draw " + std::to_string(v) + " " + std::to_string(i));
    }

    void DrawIndexed(u32 v, u32 i, u32, i32, u32) override {
        state_->state->Record("render.draw_indexed " + std::to_string(v) + " " + std::to_string(i));
    }

    void DrawIndexedIndirect(const Buffer&, u64) override {
        state_->state->Record("render.draw_indexed_indirect");
    }

    void DrawIndirect(const Buffer&, u64) override {
        state_->state->Record("render.draw_indirect");
    }

    void End() override {
        if (!ended_) {
            ended_ = true;
            state_->pass_active = false;
            state_->state->Record("render.end");
        }
    }

    void EndOcclusionQuery() override {}

    void ExecuteBundles(std::span<RenderBundle* const>) override {}

    void InsertDebugMarker(std::string_view v) override {
        state_->state->Record("marker " + std::string(v));
    }

    void MultiDrawIndexedIndirect(const Buffer&, u64, const u32 count, const Buffer* count_buffer, u64) override {
        state_->state->Record(std::string("render.multi_draw_indexed_indirect ") + std::to_string(count) + (count_buffer == nullptr ? "" : " count"));
    }

    void MultiDrawIndirect(const Buffer&, u64, const u32 count, const Buffer* count_buffer, u64) override {
        state_->state->Record(std::string("render.multi_draw_indirect ") + std::to_string(count) + (count_buffer == nullptr ? "" : " count"));
    }

    void PixelLocalStorageBarrier() override {}

    void PopDebugGroup() override {}

    void PushDebugGroup(std::string_view) override {}

    void SetBindGroup(u32, const BindGroup*, std::span<const u32>) override {}

    void SetBlendConstant(const Color&) override {}

    void SetImmediates(u32, const void*, size_t) override {}

    void SetIndexBuffer(const Buffer&, IndexFormat, u64, u64) override {}

    void SetLabel(std::string_view) override {}

    void SetPipeline(const RenderPipeline&) override {
        pipeline_ = true;
    }

    void SetResourceTable(const ResourceTable*) override {}

    void SetScissorRect(u32, u32, u32, u32) override {}

    void SetStencilReference(u32) override {}

    void SetVertexBuffer(u32, const Buffer*, u64, u64) override {}

    void SetViewport(f32, f32, f32, f32, f32, f32) override {}

    void WriteTimestamp(const QuerySet&, u32) override {}

    NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }

private:
    ref<EncoderState> state_;
    bool ended_{};
    bool pipeline_{};
};

class NullCommandBuffer final : public CommandBuffer {
public:
    explicit NullCommandBuffer(std::vector<std::function<Result<void>()>> commands)
        : commands_(std::move(commands)) {}

    void SetLabel(std::string_view label) override {
        label_ = label;
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return {.resource = const_cast<NullCommandBuffer*>(this)};
    }

    Result<void> Execute() {
        if (submitted_)
            return Err(ErrorCode::ValidationInvalidState, "RHI-VAL-031: command buffer was already submitted");
        submitted_ = true;
        for (auto& command : commands_)
            TRY_VOID(command());
        return Ok();
    }

private:
    std::vector<std::function<Result<void>()>> commands_;
    std::string label_;
    bool submitted_{};
};

NullBuffer* AsBuffer(const Buffer& value) {
    return dynamic_cast<NullBuffer*>(const_cast<Buffer*>(&value));
}

NullTexture* AsTexture(Texture* value) {
    return dynamic_cast<NullTexture*>(value);
}

Result<void> CopyTextureRows(NullTexture& texture, const Origin3D& origin, const Extent3D& size, u8* buffer, u64 buffer_size, const TexelCopyBufferLayout& layout, bool to_texture) {
    if (!texture.Region(origin, size))
        return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-012: texture copy region is out of bounds");
    const u64 row_bytes = static_cast<u64>(size.width) * BytesPerTexel(texture.GetFormat());
    const u64 stride = layout.bytes_per_row == kCopyStrideUndefined ? row_bytes : layout.bytes_per_row;
    const u64 rows = static_cast<u64>(size.height) * size.depth_or_array_layers;
    if (layout.offset > buffer_size || (rows != 0 && (rows - 1) * stride + row_bytes > buffer_size - layout.offset))
        return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-012: texture copy buffer is out of bounds");
    for (u32 z = 0; z < size.depth_or_array_layers; ++z)
        for (u32 y = 0; y < size.height; ++y) {
            auto* tex = texture.Bytes().data() + texture.Offset(origin.x, origin.y + y, origin.z + z);
            auto* row = buffer + layout.offset + (static_cast<u64>(z) * size.height + y) * stride;
            if (to_texture)
                std::memcpy(tex, row, static_cast<size_t>(row_bytes));
            else
                std::memcpy(row, tex, static_cast<size_t>(row_bytes));
        }
    return Ok();
}

class NullCommandEncoder final : public CommandEncoder {
public:
    explicit NullCommandEncoder(ref<State> state)
        : state_(createRef<EncoderState>()) {
        state_->state = std::move(state);
    }

    Result<scope<ComputePassEncoder>> BeginComputePass(const ComputePassDesc&) override {
        if (!Ready())
            return Err(ErrorCode::ValidationInvalidState, "RHI-VAL-020: encoder scope is invalid");
        state_->pass_active = true;
        state_->state->Record("compute.begin");
        return Ok(scope<ComputePassEncoder>(new NullComputePass(state_)));
    }

    Result<scope<RenderPassEncoder>> BeginRenderPass(const RenderPassDesc&) override {
        return BeginRender();
    }

    Result<scope<RenderPassEncoder>> BeginRenderPass(const RenderPassDescTyped&) override {
        return BeginRender();
    }

    Result<void> ClearBuffer(const Buffer& buffer, u64 offset, u64 size) override {
        auto* target = AsBuffer(buffer);
        if (!Ready() || target == nullptr || !target->Range(offset, size))
            return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-005: clear range is invalid");
        const u64 count = size == kWholeSize ? target->GetSize() - offset : size;
        state_->commands.push_back([target, offset, count] {
            std::fill_n(target->Bytes().begin() + static_cast<std::ptrdiff_t>(offset), static_cast<std::size_t>(count), 0);
            return Ok();
        });
        return Ok();
    }

    Result<void> CopyBufferToBuffer(const Buffer& source, u64 so, const Buffer& destination, u64 offset, u64 size) override {
        auto* src = AsBuffer(source);
        auto* dst = AsBuffer(destination);
        if (!Ready() || !src || !dst || !src->Range(so, size) || !dst->Range(offset, size))
            return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-012: buffer copy is out of bounds");
        state_->commands.push_back([src, dst, so, offset, size] {
            std::memmove(dst->Bytes().data() + offset, src->Bytes().data() + so, static_cast<size_t>(size));
            return Ok();
        });
        return Ok();
    }

    Result<void> CopyBufferToTexture(const TexelCopyBufferInfo& source, const TexelCopyTextureInfo& destination, const Extent3D& size) override {
        auto* src = source.buffer ? AsBuffer(*source.buffer) : nullptr;
        auto* dst = AsTexture(destination.texture);
        if (!Ready() || !src || !dst)
            return Err(ErrorCode::ValidationNullValue, "RHI-VAL-001: invalid texture copy object");
        state_->commands.push_back([src, dst, origin = destination.origin, size, layout = source.layout] { return CopyTextureRows(*dst, origin, size, src->Bytes().data(), src->Bytes().size(), layout, true); });
        return Ok();
    }

    Result<void> CopyTextureToBuffer(const TexelCopyTextureInfo& source, const TexelCopyBufferInfo& destination, const Extent3D& size) override {
        auto* src = AsTexture(source.texture);
        auto* dst = destination.buffer ? AsBuffer(*destination.buffer) : nullptr;
        if (!Ready() || !src || !dst)
            return Err(ErrorCode::ValidationNullValue, "RHI-VAL-001: invalid texture copy object");
        state_->commands.push_back([src, dst, origin = source.origin, size, layout = destination.layout] { return CopyTextureRows(*src, origin, size, dst->Bytes().data(), dst->Bytes().size(), layout, false); });
        return Ok();
    }

    Result<void> CopyTextureToTexture(const TexelCopyTextureInfo& source, const TexelCopyTextureInfo& destination, const Extent3D& size) override {
        auto* src = AsTexture(source.texture);
        auto* dst = AsTexture(destination.texture);
        if (!Ready() || !src || !dst || src->GetFormat() != dst->GetFormat())
            return Err(ErrorCode::GraphicsInvalidFormat, "RHI-VAL-013: texture copy formats differ");
        const u64 row = static_cast<u64>(size.width) * BytesPerTexel(src->GetFormat());
        state_->commands.push_back([src, dst, source, destination, size, row]() -> Result<void> {
            if (!src->Region(source.origin, size) || !dst->Region(destination.origin, size))
                return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-012: texture copy is out of bounds");
            for (u32 z = 0; z < size.depth_or_array_layers; ++z)
                for (u32 y = 0; y < size.height; ++y)
                    std::memmove(dst->Bytes().data() + dst->Offset(destination.origin.x, destination.origin.y + y, destination.origin.z + z),
                        src->Bytes().data() + src->Offset(source.origin.x, source.origin.y + y, source.origin.z + z), static_cast<size_t>(row));
            return Ok();
        });
        return Ok();
    }

    Result<scope<CommandBuffer>> Finish(const CommandBufferDesc&) override {
        if (!Ready())
            return Err(ErrorCode::ValidationInvalidState, "RHI-VAL-020: encoder cannot finish in current scope");
        state_->finished = true;
        return Ok(scope<CommandBuffer>(new NullCommandBuffer(std::move(state_->commands))));
    }

    void InjectValidationError(std::string_view value) override {
        state_->state->Record("validation " + std::string(value));
    }

    void InsertDebugMarker(std::string_view value) override {
        state_->state->Record("marker " + std::string(value));
    }

    void PopDebugGroup() override {}

    void PushDebugGroup(std::string_view) override {}

    Result<void> ResolveQuerySet(const QuerySet& set, u32 first, u32 count, const Buffer& destination, u64 offset) override {
        auto* dst = AsBuffer(destination);
        if (set.GetType() == QueryType::Timestamp && !dst)
            return Err(ErrorCode::ValidationNullValue, "RHI-VAL-001: invalid query destination");
        if (first + count > set.GetCount() || !dst || !dst->Range(offset, static_cast<u64>(count) * 8))
            return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-017: query resolve is out of bounds");
        state_->commands.push_back([dst, offset, count] {
            std::fill_n(dst->Bytes().begin() + static_cast<std::ptrdiff_t>(offset), static_cast<std::size_t>(count) * 8, 0);
            return Ok();
        });
        return Ok();
    }

    void SetLabel(std::string_view) override {}

    Result<void> WriteBuffer(const Buffer& buffer, u64 offset, const u8* data, u64 size) override {
        auto* dst = AsBuffer(buffer);
        if (!dst || !dst->Range(offset, size) || data == nullptr)
            return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-005: write is out of bounds");
        std::vector<u8> copy(data, data + size);
        state_->commands.push_back([dst, offset, copy = std::move(copy)] {
            std::memcpy(dst->Bytes().data() + offset, copy.data(), copy.size());
            return Ok();
        });
        return Ok();
    }

    Result<void> WriteTimestamp(const QuerySet& set, u32 index) override {
        if (set.GetType() != QueryType::Timestamp || index >= set.GetCount())
            return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-017: illegal timestamp query");
        return Ok();
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }

private:
    bool Ready() const {
        return !state_->finished && !state_->pass_active;
    }

    Result<scope<RenderPassEncoder>> BeginRender() {
        if (!Ready())
            return Err(ErrorCode::ValidationInvalidState, "RHI-VAL-020: encoder scope is invalid");
        state_->pass_active = true;
        state_->state->Record("render.begin");
        return Ok(scope<RenderPassEncoder>(new NullRenderPass(state_)));
    }

    ref<EncoderState> state_;
};

class NullQueue final : public Queue {
public:
    explicit NullQueue(ref<State> state)
        : state_(std::move(state)) {}

    Result<void> CopyExternalTextureForBrowser(const ImageCopyExternalTexture&, const TexelCopyTextureInfo&, const Extent3D&, const CopyTextureForBrowserOptions&) const override {
        return Err(ErrorCode::GraphicsUnsupportedApi, "NullRHI has no external textures");
    }

    Result<void> CopyTextureForBrowser(const TexelCopyTextureInfo&, const TexelCopyTextureInfo&, const Extent3D&, const CopyTextureForBrowserOptions&) const override {
        return Err(ErrorCode::GraphicsUnsupportedApi, "NullRHI browser copy is unavailable");
    }

    Future OnSubmittedWorkDone(CallbackMode, QueueWorkDoneCallback callback) const override {
        if (callback)
            callback(QueueWorkDoneStatus::Success, {});
        return {.id = state_->completed.load(), .completed = true, .success = true};
    }

    void SetLabel(std::string_view) const override {}

    Result<SubmissionTicket> Submit(std::span<CommandBuffer* const> commands) const override {
        if (state_->lost.load())
            return Err(ErrorCode::GraphicsDeviceLost, "RHI-DEV-001: NullRHI device is lost");
        for (auto* command : commands) {
            auto* native = dynamic_cast<NullCommandBuffer*>(command);
            if (!native)
                return Err(ErrorCode::ValidationInvalidState, "RHI-VAL-030: command buffer belongs to another device");
            TRY_VOID(native->Execute());
        }
        const auto value = state_->submitted.fetch_add(1) + 1;
        if (state_->immediate)
            state_->completed.store(value);
        state_->Record("queue.submit " + std::to_string(value));
        return Ok(SubmissionTicket(value));
    }

    SubmissionEpoch CompletedSubmission() const noexcept override {
        return SubmissionEpoch(state_->completed.load());
    }

    SubmissionTrackingStatus SubmissionTracking() const noexcept override {
        return SubmissionTrackingStatus::Healthy;
    }

    Result<void> WriteBuffer(const Buffer& buffer, u64 offset, const void* data, u64 size) const override {
        auto* dst = AsBuffer(buffer);
        if (!dst || !dst->Range(offset, size) || data == nullptr)
            return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-005: queue write is out of bounds");
        std::memcpy(dst->Bytes().data() + offset, data, static_cast<size_t>(size));
        state_->Record("queue.write_buffer");
        return Ok();
    }

    Result<void> WriteTexture(const TexelCopyTextureInfo& destination, const void* data, u64 data_size, const TexelCopyBufferLayout& layout, const Extent3D& size) const override {
        auto* dst = AsTexture(destination.texture);
        if (!dst || data == nullptr)
            return Err(ErrorCode::ValidationNullValue, "RHI-VAL-001: invalid texture write");
        TRY_VOID(CopyTextureRows(*dst, destination.origin, size, const_cast<u8*>(static_cast<const u8*>(data)), data_size, layout, true));
        state_->Record("queue.write_texture");
        return Ok();
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }

private:
    ref<State> state_;
};

class NullRenderBundleEncoder final : public RenderBundleEncoder {
public:
    void Draw(u32, u32, u32, u32) override {}

    void DrawIndexed(u32, u32, u32, i32, u32) override {}

    void DrawIndexedIndirect(const Buffer&, u64) override {}

    void DrawIndirect(const Buffer&, u64) override {}

    Result<scope<RenderBundle>> Finish(const RenderBundleDesc&) override {
        return Ok(scope<RenderBundle>(new NullRenderBundle));
    }

    void InsertDebugMarker(std::string_view) override {}

    void PopDebugGroup() override {}

    void PushDebugGroup(std::string_view) override {}

    void SetBindGroup(u32, const BindGroup*, std::span<const u32>) override {}

    void SetImmediates(u32, const void*, size_t) override {}

    void SetIndexBuffer(const Buffer&, IndexFormat, u64, u64) override {}

    void SetLabel(std::string_view) override {}

    void SetPipeline(const RenderPipeline&) override {}

    void SetResourceTable(const ResourceTable*) override {}

    void SetVertexBuffer(u32, const Buffer*, u64, u64) override {}

    NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }
};

class NullAdapter;

class NullDevice final : public Device, public ref_from_this<NullDevice> {
public:
    NullDevice(NullRhiDescriptor desc, DeviceDesc device)
        : desc_(std::move(desc)),
          state_(createRef<State>()),
          queue_(state_) {
        state_->immediate = desc_.complete_submissions_immediately;
        state_->lost_callback = std::move(device.device_lost_callback);
        if (desc_.capabilities.Formats().empty())
            desc_.capabilities = DefaultCapabilities();
    }

    Result<scope<BindGroup>> CreateBindGroup(const BindGroupDesc& desc) override {
        if (!desc.layout)
            return Err(ErrorCode::ValidationNullValue, "RHI-VAL-001: bind group layout is null");
        return Ok(scope<BindGroup>(new NullBindGroup));
    }

    Result<scope<BindGroupLayout>> CreateBindGroupLayout(const BindGroupLayoutDesc&) override {
        return Ok(scope<BindGroupLayout>(new NullBindGroupLayout));
    }

    Result<scope<Buffer>> CreateBuffer(const BufferDesc& desc) override {
        if (desc.size > Capabilities().GetLimits().max_buffer_size || desc.usage == BufferUsage::None)
            return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-002: invalid buffer descriptor");
        return Ok(scope<Buffer>(new NullBuffer(desc)));
    }

    Result<scope<CommandEncoder>> CreateCommandEncoder(const CommandEncoderDesc&) override {
        return Ok(scope<CommandEncoder>(new NullCommandEncoder(state_)));
    }

    Result<scope<ComputePipeline>> CreateComputePipeline(const ComputePipelineDesc& desc) override {
        if (!desc.compute.module)
            return Err(ErrorCode::ValidationNullValue, "RHI-VAL-001: compute module is null");
        return Ok(scope<ComputePipeline>(new NullComputePipeline));
    }

    Future CreateComputePipelineAsync(const ComputePipelineDesc& desc, CallbackMode, CreateComputePipelineCallback callback) override {
        auto result = CreateComputePipeline(desc);
        if (callback)
            callback(result ? CreatePipelineAsyncStatus::Success : CreatePipelineAsyncStatus::ValidationError, result ? std::move(*result) : scope<ComputePipeline>{},
                result ? std::string_view{} : result.error().Message());
        return {.id = 1, .completed = true, .success = bool(result)};
    }

    Result<scope<Buffer>> CreateErrorBuffer(const BufferDesc& desc) override {
        return CreateBuffer(desc);
    }

    Result<scope<ExternalTexture>> CreateErrorExternalTexture() override {
        return Ok(scope<ExternalTexture>(new NullExternalTexture));
    }

    Result<scope<ShaderModule>> CreateErrorShaderModule(const ShaderModuleDesc&, std::string_view) override {
        return Ok(scope<ShaderModule>(new NullShaderModule));
    }

    Result<scope<Texture>> CreateErrorTexture(const TextureDesc& desc) override {
        return CreateTexture(desc);
    }

    Result<scope<ExternalTexture>> CreateExternalTexture(const ExternalTextureDesc&) override {
        return Ok(scope<ExternalTexture>(new NullExternalTexture));
    }

    Result<scope<PipelineLayout>> CreatePipelineLayout(const PipelineLayoutDesc& desc) override {
        if (desc.bind_group_layouts.size() > Capabilities().GetLimits().max_bind_groups)
            return Err(ErrorCode::ValidationOutOfRange, "RHI-VAL-002: too many bind groups");
        return Ok(scope<PipelineLayout>(new NullPipelineLayout));
    }

    Result<scope<QuerySet>> CreateQuerySet(const QuerySetDesc& desc) override {
        if (desc.count == 0 || (desc.type == QueryType::Timestamp && !Capabilities().Has(CapabilityFeature::TimestampQueries)))
            return Err(ErrorCode::ValidationInvalidState, "RHI-VAL-017: unsupported query set");
        return Ok(scope<QuerySet>(new NullQuerySet(desc.type, desc.count)));
    }

    Result<scope<RenderBundleEncoder>> CreateRenderBundleEncoder(const RenderBundleEncoderDesc&) override {
        return Ok(scope<RenderBundleEncoder>(new NullRenderBundleEncoder));
    }

    Result<scope<RenderPipeline>> CreateRenderPipeline(const RenderPipelineDesc&) override {
        return Ok(scope<RenderPipeline>(new NullRenderPipeline));
    }

    Result<scope<RenderPipeline>> CreateRenderPipeline(const RenderPipelineDescTyped& desc) override {
        if (!desc.vertex || !desc.vertex->module)
            return Err(ErrorCode::ValidationNullValue, "RHI-VAL-001: vertex module is null");
        for (const auto& target : desc.fragment ? desc.fragment->targets : std::span<const ColorTargetStateDesc>{})
            if (!Capabilities().Supports(target.format, TextureUsage::RenderAttachment, desc.multisample.count))
                return Err(ErrorCode::GraphicsInvalidFormat, "RHI-VAL-015: pipeline target is unsupported");
        return Ok(scope<RenderPipeline>(new NullRenderPipeline));
    }

    Future CreateRenderPipelineAsync(const RenderPipelineDesc& desc, CallbackMode, CreateRenderPipelineCallback callback) override {
        auto result = CreateRenderPipeline(desc);
        if (callback)
            callback(result ? CreatePipelineAsyncStatus::Success : CreatePipelineAsyncStatus::ValidationError, result ? std::move(*result) : scope<RenderPipeline>{},
                result ? std::string_view{} : result.error().Message());
        return {.id = 1, .completed = true, .success = bool(result)};
    }

    Result<scope<ResourceTable>> CreateResourceTable(const ResourceTableDesc& desc) override {
        return Ok(scope<ResourceTable>(new NullResourceTable(desc.size)));
    }

    Result<scope<Sampler>> CreateSampler(const SamplerDesc&) override {
        return Ok(scope<Sampler>(new NullSampler));
    }

    Result<scope<ShaderModule>> CreateShaderModule(const ShaderModuleDesc& desc) override {
        if (desc.code.empty())
            return Err(ErrorCode::GraphicsShaderCompilationFailed, "RHI-VAL-014: shader source is empty");
        return Ok(scope<ShaderModule>(new NullShaderModule));
    }

    Result<scope<Texture>> CreateTexture(const TextureDesc& desc) override {
        if (desc.size.width == 0 || desc.size.height == 0 || desc.size.depth_or_array_layers == 0 || desc.format == TextureFormat::Undefined || !Capabilities().Supports(desc.format, desc.usage, desc.sample_count))
            return Err(ErrorCode::GraphicsTextureCreationFailed, "RHI-VAL-003: invalid or unsupported texture descriptor");
        return Ok(scope<Texture>(new NullTexture(desc)));
    }

    Result<scope<Swapchain>> CreateSwapchain(ref<Surface>, SwapchainDesc) override {
        return Err(ErrorCode::GraphicsUnsupportedApi, "NullRHI swapchain requires no presentation; use an offscreen target");
    }

    void Destroy() override {
        Lose(DeviceLostReason::Destroyed, "NullRHI device destroyed");
    }

    void ForceLoss(DeviceLostReason reason, std::string_view message) override {
        Lose(reason, message);
    }

    Result<scope<Adapter>> GetAdapter() const override;

    Result<void> GetAdapterInfo(AdapterInfo& info) const override {
        info = {.device = desc_.adapter_name, .description = desc_.adapter_name, .backend_type = BackendType::Null, .adapter_type = AdapterType::CPU, .feature_level = FeatureLevel::Core};
        return Ok();
    }

    Result<void> GetAHardwareBufferProperties(void*, void*) const override {
        return Err(ErrorCode::GraphicsUnsupportedApi, "NullRHI has no native hardware buffers");
    }

    void GetFeatures(SupportedFeatures& values) const override {
        values = LegacyFeatures(Capabilities());
    }

    Result<void> GetLimits(Limits& limits) const override {
        limits = Capabilities().GetLimits();
        return Ok();
    }

    Future GetLostFuture() const override {
        return {.id = 1, .completed = state_->lost.load(), .success = state_->lost.load()};
    }

    Queue& GetQueue() const noexcept override {
        return const_cast<NullQueue&>(queue_);
    }

    bool HasFeature(FeatureName feature) const noexcept override {
        SupportedFeatures f;
        GetFeatures(f);
        return f.Has(feature);
    }

    const DeviceCapabilities& Capabilities() const noexcept override {
        return desc_.capabilities;
    }

    bool IsLost() const noexcept override {
        return state_->lost.load();
    }

    DeviceLostReason LossReason() const noexcept override {
        return state_->loss_reason.load();
    }

    Result<scope<SharedBufferMemory>> ImportSharedBufferMemory(const SharedBufferMemoryDesc&) override {
        return Err(ErrorCode::GraphicsUnsupportedApi, "NullRHI shared memory is unavailable");
    }

    Result<scope<SharedFence>> ImportSharedFence(const SharedFenceDesc&) override {
        return Err(ErrorCode::GraphicsUnsupportedApi, "NullRHI shared fences are unavailable");
    }

    Result<scope<SharedTextureMemory>> ImportSharedTextureMemory(const SharedTextureMemoryDesc&) override {
        return Err(ErrorCode::GraphicsUnsupportedApi, "NullRHI shared memory is unavailable");
    }

    void InjectError(ErrorType, std::string_view message) override {
        state_->Record("error " + std::string(message));
    }

    Future PopErrorScope(CallbackMode, PopErrorScopeCallback callback) const override {
        if (callback)
            callback(PopErrorScopeStatus::Success, ErrorType::NoError, {});
        return {.id = 1, .completed = true, .success = true};
    }

    void PushErrorScope(ErrorFilter) override {}

    void SetLabel(std::string_view) override {}

    void SetLoggingCallback(LoggingCallback callback) override {
        logging_ = std::move(callback);
    }

    void Tick() const noexcept override {
        state_->completed.store(state_->submitted.load());
    }

    void ValidateTextureDescriptor(const TextureDesc&) const override {}

    NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }

    const ref<State>& SharedState() const {
        return state_;
    }

    void Lose(DeviceLostReason reason, std::string_view message) {
        if (state_->lost.exchange(true))
            return;
        state_->loss_reason.store(reason);
        state_->Record("device.lost " + std::string(message));
        if (state_->lost_callback)
            state_->lost_callback(reason, message);
    }

private:
    NullRhiDescriptor desc_;
    ref<State> state_;
    mutable NullQueue queue_;
    LoggingCallback logging_;
};

class NullAdapter final : public Adapter {
public:
    explicit NullAdapter(NullRhiDescriptor desc)
        : desc_(std::move(desc)) {
        if (desc_.capabilities.Formats().empty())
            desc_.capabilities = DefaultCapabilities();
    }

    Result<ref<Device>> CreateDevice(const DeviceDesc& desc) override {
        return Ok(ref<Device>(createRef<NullDevice>(desc_, desc)));
    }

    Result<ref<Device>> RequestDevice(const DeviceDesc& desc) override {
        return CreateDevice(desc);
    }

    Future RequestDevice(const DeviceDesc& desc, CallbackMode, RequestDeviceCallback callback) override {
        auto result = CreateDevice(desc);
        if (callback)
            callback(result ? RequestDeviceStatus::Success : RequestDeviceStatus::Error, result ? *result : nullptr, result ? std::string_view{} : result.error().Message());
        return {.id = 1, .completed = true, .success = bool(result)};
    }

    AdapterInfo GetInfo() const override {
        AdapterInfo info;
        static_cast<void>(GetInfo(info));
        return info;
    }

    Result<void> GetInfo(AdapterInfo& info) const override {
        info = {.device = desc_.adapter_name, .description = desc_.adapter_name, .backend_type = BackendType::Null, .adapter_type = AdapterType::CPU, .feature_level = FeatureLevel::Core};
        return Ok();
    }

    void GetFeatures(SupportedFeatures& f) const override {
        f = LegacyFeatures(desc_.capabilities);
    }

    SupportedFeatures GetFeatures() const override {
        SupportedFeatures f;
        GetFeatures(f);
        return f;
    }

    Result<void> GetFormatCapabilities(TextureFormat format, DawnFormatCapabilities&) const override {
        return desc_.capabilities.Format(format) ? Ok() : Err(ErrorCode::GraphicsInvalidFormat, "format unsupported");
    }

    Limits GetLimits() const override {
        return desc_.capabilities.GetLimits();
    }

    Result<void> GetLimits(Limits& limits) const override {
        limits = GetLimits();
        return Ok();
    }

    bool HasFeature(FeatureName feature) const noexcept override {
        return GetFeatures().Has(feature);
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }

private:
    NullRhiDescriptor desc_;
};

class NullInstance final : public Instance {
public:
    explicit NullInstance(NullRhiDescriptor descriptor)
        : descriptor_(std::move(descriptor)) {}

    bool IsValid() const noexcept override {
        return true;
    }

    const InstanceDesc& GetDesc() const noexcept override {
        return desc_;
    }

    Result<ref<Surface>> CreateSurface(const SurfaceDescriptor&) override {
        return Err(ErrorCode::GraphicsUnsupportedApi, "NullRHI uses offscreen presentation");
    }

    Result<ref<Surface>> CreateSurface(Window&, SurfaceDesc) override {
        return Err(ErrorCode::GraphicsUnsupportedApi, "NullRHI uses offscreen presentation");
    }

    void GetWGSLLanguageFeatures(SupportedWGSLLanguageFeatures& features) const override {
        features.features = {};
    }

    bool HasWGSLLanguageFeature(WGSLLanguageFeatureName) const noexcept override {
        return false;
    }

    void ProcessEvents() const noexcept override {}

    WaitStatus WaitAny(Future& future, u64) override {
        future.completed = true;
        return WaitStatus::Success;
    }

    WaitStatus WaitAny(FutureWaitInfo& info, u64 timeout) override {
        auto status = WaitAny(info.future, timeout);
        info.completed = true;
        return status;
    }

    WaitStatus WaitAny(std::span<FutureWaitInfo> infos, u64 timeout) override {
        for (auto& info : infos)
            WaitAny(info, timeout);
        return WaitStatus::Success;
    }

    Result<scope<Adapter>> RequestAdapter(RequestAdapterDesc) override {
        return CreateAdapter(descriptor_);
    }

    Future RequestAdapter(RequestAdapterDesc, CallbackMode, RequestAdapterCallback callback) override {
        auto adapter = CreateAdapter(descriptor_);
        if (callback)
            callback(RequestAdapterStatus::Success, std::move(*adapter), {});
        return {.id = 1, .completed = true, .success = true};
    }

    NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }

private:
    NullRhiDescriptor descriptor_;
    InstanceDesc desc_{.label = "NullRHI instance"};
};

Result<scope<Adapter>> NullDevice::GetAdapter() const {
    return CreateAdapter(desc_);
}

} // namespace

Result<scope<Adapter>> CreateAdapter(NullRhiDescriptor descriptor) {
    return Ok(scope<Adapter>(new NullAdapter(std::move(descriptor))));
}

} // namespace woki::rhi::null

namespace woki::rhi {

Result<scope<Instance>> CreateNullInstance(NullRhiDescriptor descriptor) {
    return Ok(scope<Instance>(new null::NullInstance(std::move(descriptor))));
}

std::span<const std::string> NullCommandLog(const Device& device) noexcept {
    const auto* null_device = dynamic_cast<const null::NullDevice*>(&device);
    if (!null_device)
        return {};
    const auto& state = null_device->SharedState();
    return state->log;
}

Result<void> LoseNullDevice(Device& device, DeviceLostReason reason, std::string_view message) {
    auto* null_device = dynamic_cast<null::NullDevice*>(&device);
    if (!null_device)
        return Err(ErrorCode::InvalidArgument, "device is not a NullRHI device");
    null_device->Lose(reason, message);
    return Ok();
}

} // namespace woki::rhi
