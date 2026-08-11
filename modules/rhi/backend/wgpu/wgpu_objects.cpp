#include <utility>

#include "wgpu_enums.hpp"
#include "wgpu_objects.hpp"
#include "detail/handle.hpp"
#include "detail/string.hpp"
#include "detail/native_helpers.hpp"
#include "detail/compilation_info.hpp"
#include "detail/resource_descriptor.hpp"

namespace woki::rhi::wgpu {
namespace {

using convert::FromWgpu;
using convert::FromWgpuBufferUsage;
using convert::ToWgpu;
using convert::ToWgpuMapMode;

struct MapAsyncCallbackState {
    MapAsyncCallback callback;
};

void BufferMapAsyncThunk(const WGPUMapAsyncStatus status, const WGPUStringView message, void* userdata1, void*) {
    auto state = scope<MapAsyncCallbackState>(static_cast<MapAsyncCallbackState*>(userdata1));
    if (state == nullptr || !state->callback) {
        return;
    }

    state->callback(FromWgpu(status), detail::StringFromView(message));
}

struct CompilationInfoCallbackState {
    ShaderModuleCompilationInfoCallback callback;
};

void ShaderModuleCompilationInfoThunk(const WGPUCompilationInfoRequestStatus status, const WGPUCompilationInfo* compilation_info, void* userdata1, void*) {
    auto state = scope<CompilationInfoCallbackState>(static_cast<CompilationInfoCallbackState*>(userdata1));
    if (state == nullptr || !state->callback) {
        return;
    }

    if (compilation_info != nullptr) {
        const CompilationInfo info = detail::FromWgpuCompilationInfo(*compilation_info);
        state->callback(FromWgpu(status), &info, {});
        return;
    }

    state->callback(FromWgpu(status), nullptr, {});
}

[[nodiscard]] Result<void> FromWgpuStatus(const WGPUStatus status, std::string_view message) {
    if (status != WGPUStatus_Success) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, std::string(message));
    }
    return Ok();
}

[[nodiscard]] WGPUTexture NativeTexture(const Texture& texture) noexcept {
    return static_cast<WGPUTexture>(texture.GetNativeHandles().resource);
}

[[nodiscard]] bool IsMappedRangeValid(const WGPUBuffer buffer, const size_t offset, const size_t size, const bool allow_whole_size) noexcept {
    if (buffer == nullptr) {
        return false;
    }

    const u64 buffer_size = wgpuBufferGetSize(buffer);
    const u64 range_offset = static_cast<u64>(offset);
    if (range_offset > buffer_size) {
        return false;
    }
    if (allow_whole_size && size == kWholeMapSize) {
        return true;
    }
    return static_cast<u64>(size) <= buffer_size - range_offset;
}

} // namespace

class WgpuBindGroupImpl final : public BindGroup {
public:
    explicit WgpuBindGroupImpl(WGPUBindGroup handle) noexcept
        : handle_(handle) {}

    void SetLabel(std::string_view label) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::BindGroupHandle handle_;
};

class WgpuBindGroupLayoutImpl final : public BindGroupLayout {
public:
    explicit WgpuBindGroupLayoutImpl(WGPUBindGroupLayout handle) noexcept
        : handle_(handle) {}

    void SetLabel(std::string_view label) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::BindGroupLayoutHandle handle_;
};

class WgpuPipelineLayoutImpl final : public PipelineLayout {
public:
    explicit WgpuPipelineLayoutImpl(WGPUPipelineLayout handle) noexcept
        : handle_(handle) {}

    void SetLabel(std::string_view label) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::PipelineLayoutHandle handle_;
};

class WgpuSamplerImpl final : public Sampler {
public:
    explicit WgpuSamplerImpl(WGPUSampler handle) noexcept
        : handle_(handle) {}

    void SetLabel(std::string_view label) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::SamplerHandle handle_;
};

class WgpuRenderBundleImpl final : public RenderBundle {
public:
    explicit WgpuRenderBundleImpl(WGPURenderBundle handle) noexcept
        : handle_(handle) {}

    void SetLabel(std::string_view label) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::RenderBundleHandle handle_;
};

class WgpuTexelBufferViewImpl final : public TexelBufferView {
public:
    explicit WgpuTexelBufferViewImpl(WGPUTexelBufferView handle) noexcept
        : handle_(handle) {}

    void SetLabel(std::string_view label) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::TexelBufferViewHandle handle_;
};

class WgpuTextureViewImpl final : public TextureView {
public:
    explicit WgpuTextureViewImpl(WGPUTextureView handle) noexcept
        : handle_(handle) {}

    void SetLabel(std::string_view label) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::TextureViewHandle handle_;
};

class WgpuComputePipelineImpl final : public ComputePipeline {
public:
    explicit WgpuComputePipelineImpl(WGPUComputePipeline handle) noexcept
        : handle_(handle) {}

    [[nodiscard]] scope<BindGroupLayout> GetBindGroupLayout(u32 group_index) const override;
    void SetLabel(std::string_view label) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::ComputePipelineHandle handle_;
};

class WgpuRenderPipelineImpl final : public RenderPipeline {
public:
    explicit WgpuRenderPipelineImpl(WGPURenderPipeline handle) noexcept
        : handle_(handle) {}

    [[nodiscard]] scope<BindGroupLayout> GetBindGroupLayout(u32 group_index) const override;
    void SetLabel(std::string_view label) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::RenderPipelineHandle handle_;
};

class WgpuQuerySetImpl final : public QuerySet {
public:
    explicit WgpuQuerySetImpl(WGPUQuerySet handle) noexcept
        : handle_(handle) {}

    void Destroy() override;
    [[nodiscard]] u32 GetCount() const override;
    [[nodiscard]] QueryType GetType() const override;
    void SetLabel(std::string_view label) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::QuerySetHandle handle_;
};

class WgpuExternalTextureImpl final : public ExternalTexture {
public:
    explicit WgpuExternalTextureImpl(WGPUExternalTexture handle) noexcept
        : handle_(handle) {}

    void Destroy() override;
    void Expire() override;
    void Refresh() override;
    void SetLabel(std::string_view label) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::ExternalTextureHandle handle_;
};

class WgpuBufferImpl final : public Buffer {
public:
    explicit WgpuBufferImpl(WGPUBuffer handle) noexcept
        : handle_(handle) {}

    [[nodiscard]] Result<scope<TexelBufferView>> CreateTexelView(const TexelBufferViewDesc& desc) const override;
    void Destroy() override;
    [[nodiscard]] const void* GetConstMappedRange(size_t offset, size_t size) const override;
    [[nodiscard]] void* GetMappedRange(size_t offset, size_t size) override;
    [[nodiscard]] BufferMapState GetMapState() const override;
    [[nodiscard]] u64 GetSize() const override;
    [[nodiscard]] BufferUsage GetUsage() const override;
    [[nodiscard]] Future MapAsync(MapMode mode, size_t offset, size_t size, CallbackMode callback_mode, MapAsyncCallback callback) const override;
    [[nodiscard]] Result<void> ReadMappedRange(size_t offset, void* data, size_t size) const override;
    void SetLabel(std::string_view label) override;
    void Unmap() override;
    [[nodiscard]] Result<void> WriteMappedRange(size_t offset, const void* data, size_t size) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::BufferHandle handle_;
};

class WgpuTextureImpl final : public Texture {
public:
    explicit WgpuTextureImpl(WGPUTexture handle) noexcept
        : handle_(handle) {}

    [[nodiscard]] scope<TextureView> CreateErrorView(const TextureViewDesc& desc) const override;
    [[nodiscard]] scope<TextureView> CreateView(const TextureViewDesc& desc) const override;
    void Destroy() override;
    [[nodiscard]] u32 GetDepthOrArrayLayers() const override;
    [[nodiscard]] TextureDimension GetDimension() const override;
    [[nodiscard]] TextureFormat GetFormat() const override;
    [[nodiscard]] u32 GetHeight() const override;
    [[nodiscard]] u32 GetMipLevelCount() const override;
    [[nodiscard]] u32 GetSampleCount() const override;
    [[nodiscard]] TextureViewDimension GetTextureBindingViewDimension() const override;
    [[nodiscard]] TextureUsage GetUsage() const override;
    [[nodiscard]] u32 GetWidth() const override;
    void Pin(TextureUsage usage) override;
    void SetLabel(std::string_view label) override;
    void SetOwnershipForMemoryDump(u64 owner_guid) override;
    void Unpin() override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::TextureHandle handle_;
};

class WgpuRenderBundleEncoderImpl final : public RenderBundleEncoder {
public:
    explicit WgpuRenderBundleEncoderImpl(WGPURenderBundleEncoder handle) noexcept
        : encoder_(handle) {}

    void Draw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance) override;
    void DrawIndexed(u32 index_count, u32 instance_count, u32 first_index, i32 base_vertex, u32 first_instance) override;
    void DrawIndexedIndirect(const Buffer& indirect_buffer, u64 indirect_offset) override;
    void DrawIndirect(const Buffer& indirect_buffer, u64 indirect_offset) override;
    [[nodiscard]] Result<scope<RenderBundle>> Finish(const RenderBundleDesc& desc) override;
    void InsertDebugMarker(std::string_view marker_label) override;
    void PopDebugGroup() override;
    void PushDebugGroup(std::string_view group_label) override;
    void SetBindGroup(u32 group_index, const BindGroup* group, std::span<const u32> dynamic_offsets) override;
    void SetImmediates(u32 offset, const void* data, size_t size) override;
    void SetIndexBuffer(const Buffer& buffer, IndexFormat format, u64 offset, u64 size) override;
    void SetLabel(std::string_view label) override;
    void SetPipeline(const RenderPipeline& pipeline) override;
    void SetResourceTable(const ResourceTable* table) override;
    void SetVertexBuffer(u32 slot, const Buffer* buffer, u64 offset, u64 size) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::RenderBundleEncoderHandle encoder_;
};

class WgpuResourceTableImpl final : public ResourceTable {
public:
    explicit WgpuResourceTableImpl(WGPUResourceTable handle) noexcept
        : handle_(handle) {}

    void Destroy() override;
    [[nodiscard]] u32 GetSize() const override;
    [[nodiscard]] u32 InsertBinding(const BindingResourceDesc& resource) override;
    [[nodiscard]] Result<void> RemoveBinding(u32 slot) override;
    void SetLabel(std::string_view label) override;
    [[nodiscard]] Result<void> Update(u32 slot, const BindingResourceDesc& resource) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::ResourceTableHandle handle_;
};

class WgpuShaderModuleImpl final : public ShaderModule {
public:
    explicit WgpuShaderModuleImpl(WGPUShaderModule handle) noexcept
        : handle_(handle) {}

    [[nodiscard]] Future GetCompilationInfo(CallbackMode callback_mode, ShaderModuleCompilationInfoCallback callback) const override;
    void SetLabel(std::string_view label) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::ShaderModuleHandle handle_;
};

class WgpuSharedBufferMemoryImpl final : public SharedBufferMemory {
public:
    explicit WgpuSharedBufferMemoryImpl(WGPUSharedBufferMemory handle) noexcept
        : handle_(handle) {}

    [[nodiscard]] Result<void> BeginAccess(const Buffer& buffer, const SharedBufferMemoryBeginAccessDesc& desc) const override;
    [[nodiscard]] Result<scope<Buffer>> CreateBuffer(const BufferDesc& desc) const override;
    [[nodiscard]] Result<void> EndAccess(const Buffer& buffer, SharedBufferMemoryEndAccessState& state) const override;
    [[nodiscard]] Result<void> GetProperties(SharedBufferMemoryProperties& properties) const override;
    [[nodiscard]] bool IsDeviceLost() const override;
    void SetLabel(std::string_view label) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::SharedBufferMemoryHandle handle_;
};

class WgpuSharedFenceImpl final : public SharedFence {
public:
    explicit WgpuSharedFenceImpl(WGPUSharedFence handle) noexcept
        : handle_(handle) {}

    void ExportInfo(SharedFenceExportInfo& info) const override;
    void SetLabel(std::string_view label) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::SharedFenceHandle handle_;
};

class WgpuSharedTextureMemoryImpl final : public SharedTextureMemory {
public:
    explicit WgpuSharedTextureMemoryImpl(WGPUSharedTextureMemory handle) noexcept
        : handle_(handle) {}

    [[nodiscard]] Result<void> BeginAccess(const Texture& texture, const SharedTextureMemoryBeginAccessDesc& desc) const override;
    [[nodiscard]] Result<scope<Texture>> CreateTexture(const TextureDesc& desc) const override;
    [[nodiscard]] Result<void> EndAccess(const Texture& texture, SharedTextureMemoryEndAccessState& state) const override;
    [[nodiscard]] Result<void> GetProperties(SharedTextureMemoryProperties& properties) const override;
    [[nodiscard]] bool IsDeviceLost() const override;
    void SetLabel(std::string_view label) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::SharedTextureMemoryHandle handle_;
};

[[nodiscard]] scope<RenderBundle> CreateRenderBundleObject(WGPURenderBundle handle);
[[nodiscard]] scope<TexelBufferView> CreateTexelBufferViewObject(WGPUTexelBufferView handle);

void WgpuBindGroupImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuBindGroupSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuBindGroupImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

void WgpuBindGroupLayoutImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuBindGroupLayoutSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuBindGroupLayoutImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

void WgpuPipelineLayoutImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuPipelineLayoutSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuPipelineLayoutImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

void WgpuSamplerImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuSamplerSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuSamplerImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

void WgpuRenderBundleImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuRenderBundleSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuRenderBundleImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

void WgpuTexelBufferViewImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuTexelBufferViewSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuTexelBufferViewImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

void WgpuTextureViewImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuTextureViewSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuTextureViewImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

scope<BindGroupLayout> WgpuComputePipelineImpl::GetBindGroupLayout(const u32 group_index) const {
    if (!handle_) {
        return nullptr;
    }

    return CreateBindGroupLayoutObject(wgpuComputePipelineGetBindGroupLayout(handle_.get(), group_index));
}

void WgpuComputePipelineImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuComputePipelineSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuComputePipelineImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

scope<BindGroupLayout> WgpuRenderPipelineImpl::GetBindGroupLayout(const u32 group_index) const {
    if (!handle_) {
        return nullptr;
    }

    return CreateBindGroupLayoutObject(wgpuRenderPipelineGetBindGroupLayout(handle_.get(), group_index));
}

void WgpuRenderPipelineImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuRenderPipelineSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuRenderPipelineImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

void WgpuQuerySetImpl::Destroy() {
    if (handle_) {
        wgpuQuerySetDestroy(handle_.get());
    }
}

u32 WgpuQuerySetImpl::GetCount() const {
    if (!handle_) {
        return 0;
    }

    return wgpuQuerySetGetCount(handle_.get());
}

QueryType WgpuQuerySetImpl::GetType() const {
    if (!handle_) {
        return QueryType::Occlusion;
    }

    return FromWgpu(wgpuQuerySetGetType(handle_.get()));
}

void WgpuQuerySetImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuQuerySetSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuQuerySetImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

void WgpuExternalTextureImpl::Destroy() {
    if (handle_) {
        wgpuExternalTextureDestroy(handle_.get());
    }
}

void WgpuExternalTextureImpl::Expire() {
    if (handle_) {
        wgpuExternalTextureExpire(handle_.get());
    }
}

void WgpuExternalTextureImpl::Refresh() {
    if (handle_) {
        wgpuExternalTextureRefresh(handle_.get());
    }
}

void WgpuExternalTextureImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuExternalTextureSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuExternalTextureImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

Result<scope<TexelBufferView>> WgpuBufferImpl::CreateTexelView(const TexelBufferViewDesc& desc) const {
    if (!handle_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Buffer is invalid");
    }

    const detail::TexelBufferViewDescriptorStorage storage(desc);
    const WGPUTexelBufferView view = wgpuBufferCreateTexelView(handle_.get(), &storage.native);
    if (view == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to create texel buffer view");
    }
    return Ok(CreateTexelBufferViewObject(view));
}

void WgpuBufferImpl::Destroy() {
    if (handle_) {
        wgpuBufferDestroy(handle_.get());
    }
}

const void* WgpuBufferImpl::GetConstMappedRange(const size_t offset, const size_t size) const {
    if (!IsMappedRangeValid(handle_.get(), offset, size, true)) {
        return nullptr;
    }

    return wgpuBufferGetConstMappedRange(handle_.get(), offset, size);
}

void* WgpuBufferImpl::GetMappedRange(const size_t offset, const size_t size) {
    if (!IsMappedRangeValid(handle_.get(), offset, size, true)) {
        return nullptr;
    }

    return wgpuBufferGetMappedRange(handle_.get(), offset, size);
}

BufferMapState WgpuBufferImpl::GetMapState() const {
    if (!handle_) {
        return BufferMapState::Unmapped;
    }

    return FromWgpu(wgpuBufferGetMapState(handle_.get()));
}

u64 WgpuBufferImpl::GetSize() const {
    if (!handle_) {
        return 0;
    }

    return wgpuBufferGetSize(handle_.get());
}

BufferUsage WgpuBufferImpl::GetUsage() const {
    if (!handle_) {
        return BufferUsage::None;
    }

    return FromWgpuBufferUsage(wgpuBufferGetUsage(handle_.get()));
}

Future WgpuBufferImpl::MapAsync(const MapMode mode, const size_t offset, const size_t size, const CallbackMode callback_mode, MapAsyncCallback callback) const {
    Future future{};
    if (!handle_) {
        future.message = "Buffer is invalid";
        return future;
    }

    if (!callback) {
        future.message = "MapAsync requires a callback";
        return future;
    }

    if (!IsMappedRangeValid(handle_.get(), offset, size, true)) {
        future.message = "Buffer map range is out of bounds";
        return future;
    }

    auto callback_state = createScope<MapAsyncCallbackState>(MapAsyncCallbackState{.callback = std::move(callback)});

    WGPUBufferMapCallbackInfo callback_info = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    callback_info.mode = ToWgpu(callback_mode);
    callback_info.callback = BufferMapAsyncThunk;
    callback_info.userdata1 = callback_state.get();

    auto* transferred_state = callback_state.release();
    const WGPUFuture native_future = wgpuBufferMapAsync(handle_.get(), ToWgpuMapMode(mode), offset, size, callback_info);
    future.id = native_future.id;
    if (future.id == 0) {
        callback_state.reset(transferred_state);
        future.message = "Buffer map request failed to start";
    }
    return future;
}

Result<void> WgpuBufferImpl::ReadMappedRange(const size_t offset, void* data, const size_t size) const {
    if (!handle_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Buffer is invalid");
    }

    if (data == nullptr && size != 0) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Mapped range destination is null");
    }
    if (!IsMappedRangeValid(handle_.get(), offset, size, false)) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Mapped range is out of bounds");
    }

    return FromWgpuStatus(wgpuBufferReadMappedRange(handle_.get(), offset, data, size), "Failed to read mapped buffer range");
}

void WgpuBufferImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuBufferSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

void WgpuBufferImpl::Unmap() {
    if (handle_) {
        wgpuBufferUnmap(handle_.get());
    }
}

Result<void> WgpuBufferImpl::WriteMappedRange(const size_t offset, const void* data, const size_t size) {
    if (!handle_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Buffer is invalid");
    }

    if (data == nullptr && size != 0) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Mapped range source is null");
    }
    if (!IsMappedRangeValid(handle_.get(), offset, size, false)) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Mapped range is out of bounds");
    }

    return FromWgpuStatus(wgpuBufferWriteMappedRange(handle_.get(), offset, data, size), "Failed to write mapped buffer range");
}

NativeHandles WgpuBufferImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

scope<TextureView> WgpuTextureImpl::CreateErrorView(const TextureViewDesc& desc) const {
    if (!handle_) {
        return nullptr;
    }

    const detail::TextureViewDescriptorStorage storage(desc);
    return CreateTextureViewObject(wgpuTextureCreateErrorView(handle_.get(), &storage.native));
}

scope<TextureView> WgpuTextureImpl::CreateView(const TextureViewDesc& desc) const {
    if (!handle_) {
        return nullptr;
    }

    const detail::TextureViewDescriptorStorage storage(desc);
    return CreateTextureViewObject(wgpuTextureCreateView(handle_.get(), &storage.native));
}

void WgpuTextureImpl::Destroy() {
    if (handle_) {
        wgpuTextureDestroy(handle_.get());
    }
}

u32 WgpuTextureImpl::GetDepthOrArrayLayers() const {
    if (!handle_) {
        return 0;
    }

    return wgpuTextureGetDepthOrArrayLayers(handle_.get());
}

TextureDimension WgpuTextureImpl::GetDimension() const {
    if (!handle_) {
        return TextureDimension::Undefined;
    }

    return FromWgpu(wgpuTextureGetDimension(handle_.get()));
}

TextureFormat WgpuTextureImpl::GetFormat() const {
    if (!handle_) {
        return TextureFormat::Undefined;
    }

    return FromWgpu(wgpuTextureGetFormat(handle_.get()));
}

u32 WgpuTextureImpl::GetHeight() const {
    if (!handle_) {
        return 0;
    }

    return wgpuTextureGetHeight(handle_.get());
}

u32 WgpuTextureImpl::GetMipLevelCount() const {
    if (!handle_) {
        return 0;
    }

    return wgpuTextureGetMipLevelCount(handle_.get());
}

u32 WgpuTextureImpl::GetSampleCount() const {
    if (!handle_) {
        return 0;
    }

    return wgpuTextureGetSampleCount(handle_.get());
}

TextureViewDimension WgpuTextureImpl::GetTextureBindingViewDimension() const {
    if (!handle_) {
        return TextureViewDimension::Undefined;
    }

    return FromWgpu(wgpuTextureGetTextureBindingViewDimension(handle_.get()));
}

TextureUsage WgpuTextureImpl::GetUsage() const {
    if (!handle_) {
        return TextureUsage::None;
    }

    return detail::TextureUsageFromWgpu(wgpuTextureGetUsage(handle_.get()));
}

u32 WgpuTextureImpl::GetWidth() const {
    if (!handle_) {
        return 0;
    }

    return wgpuTextureGetWidth(handle_.get());
}

void WgpuTextureImpl::Pin(const TextureUsage usage) {
    // Texture residency hints are unavailable in the supported Dawn SDK.
    (void)usage;
}

void WgpuTextureImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuTextureSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

void WgpuTextureImpl::SetOwnershipForMemoryDump(const u64 owner_guid) {
    if (handle_) {
        wgpuTextureSetOwnershipForMemoryDump(handle_.get(), owner_guid);
    }
}

void WgpuTextureImpl::Unpin() {}

NativeHandles WgpuTextureImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

void WgpuRenderBundleEncoderImpl::Draw(const u32 vertex_count, const u32 instance_count, const u32 first_vertex, const u32 first_instance) {
    if (encoder_) {
        wgpuRenderBundleEncoderDraw(encoder_.get(), vertex_count, instance_count, first_vertex, first_instance);
    }
}

void WgpuRenderBundleEncoderImpl::DrawIndexed(const u32 index_count, const u32 instance_count, const u32 first_index, const i32 base_vertex, const u32 first_instance) {
    if (encoder_) {
        wgpuRenderBundleEncoderDrawIndexed(encoder_.get(), index_count, instance_count, first_index, base_vertex, first_instance);
    }
}

void WgpuRenderBundleEncoderImpl::DrawIndexedIndirect(const Buffer& indirect_buffer, const u64 indirect_offset) {
    if (encoder_) {
        wgpuRenderBundleEncoderDrawIndexedIndirect(encoder_.get(), detail::NativeBuffer(indirect_buffer), indirect_offset);
    }
}

void WgpuRenderBundleEncoderImpl::DrawIndirect(const Buffer& indirect_buffer, const u64 indirect_offset) {
    if (encoder_) {
        wgpuRenderBundleEncoderDrawIndirect(encoder_.get(), detail::NativeBuffer(indirect_buffer), indirect_offset);
    }
}

Result<scope<RenderBundle>> WgpuRenderBundleEncoderImpl::Finish(const RenderBundleDesc& desc) {
    if (!encoder_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Render bundle encoder is invalid");
    }

    WGPURenderBundleDescriptor native = WGPU_RENDER_BUNDLE_DESCRIPTOR_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    native.label = detail::ToStringView(desc.label);

    const WGPURenderBundle bundle = wgpuRenderBundleEncoderFinish(encoder_.get(), &native);
    if (bundle == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to finish render bundle");
    }

    return Ok(CreateRenderBundleObject(bundle));
}

void WgpuRenderBundleEncoderImpl::InsertDebugMarker(const std::string_view marker_label) {
    if (encoder_) {
        wgpuRenderBundleEncoderInsertDebugMarker(encoder_.get(), detail::ToStringView(marker_label));
    }
}

void WgpuRenderBundleEncoderImpl::PopDebugGroup() {
    if (encoder_) {
        wgpuRenderBundleEncoderPopDebugGroup(encoder_.get());
    }
}

void WgpuRenderBundleEncoderImpl::PushDebugGroup(const std::string_view group_label) {
    if (encoder_) {
        wgpuRenderBundleEncoderPushDebugGroup(encoder_.get(), detail::ToStringView(group_label));
    }
}

void WgpuRenderBundleEncoderImpl::SetBindGroup(const u32 group_index, const BindGroup* group, const std::span<const u32> dynamic_offsets) {
    if (!encoder_) {
        return;
    }

    wgpuRenderBundleEncoderSetBindGroup(encoder_.get(), group_index, detail::NativeBindGroup(group), dynamic_offsets.size(), dynamic_offsets.empty() ? nullptr : dynamic_offsets.data());
}

void WgpuRenderBundleEncoderImpl::SetImmediates(const u32 offset, const void* data, const size_t size) {
    if (encoder_) {
        wgpuRenderBundleEncoderSetImmediates(encoder_.get(), offset, data, size);
    }
}

void WgpuRenderBundleEncoderImpl::SetIndexBuffer(const Buffer& buffer, const IndexFormat format, const u64 offset, const u64 size) {
    if (encoder_) {
        wgpuRenderBundleEncoderSetIndexBuffer(encoder_.get(), detail::NativeBuffer(buffer), ToWgpu(format), offset, size);
    }
}

void WgpuRenderBundleEncoderImpl::SetLabel(const std::string_view label) {
    if (encoder_) {
        wgpuRenderBundleEncoderSetLabel(encoder_.get(), detail::ToStringView(label));
    }
}

void WgpuRenderBundleEncoderImpl::SetPipeline(const RenderPipeline& pipeline) {
    if (encoder_) {
        wgpuRenderBundleEncoderSetPipeline(encoder_.get(), detail::NativeRenderPipeline(pipeline));
    }
}

void WgpuRenderBundleEncoderImpl::SetResourceTable(const ResourceTable* table) {
#if defined(WOKI_WGPU_HAS_RENDER_BUNDLE_RESOURCE_TABLE)
    if (encoder_) {
        wgpuRenderBundleEncoderSetResourceTable(encoder_.get(), detail::NativeResourceTable(table));
    }
#else
    (void)table;
#endif
}

void WgpuRenderBundleEncoderImpl::SetVertexBuffer(const u32 slot, const Buffer* buffer, const u64 offset, const u64 size) {
    if (encoder_) {
        wgpuRenderBundleEncoderSetVertexBuffer(encoder_.get(), slot, buffer == nullptr ? nullptr : detail::NativeBuffer(*buffer), offset, size);
    }
}

NativeHandles WgpuRenderBundleEncoderImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = encoder_.get();
    return handles;
}

void WgpuResourceTableImpl::Destroy() {
    if (handle_) {
        wgpuResourceTableDestroy(handle_.get());
    }
}

u32 WgpuResourceTableImpl::GetSize() const {
    if (!handle_) {
        return 0;
    }

    return wgpuResourceTableGetSize(handle_.get());
}

u32 WgpuResourceTableImpl::InsertBinding(const BindingResourceDesc& resource) {
    if (!handle_) {
        return 0;
    }

    const WGPUBindingResource native_resource = detail::ToWgpu(resource);
#if defined(WOKI_WGPU_HAS_RESOURCE_TABLE_BINDING_NAMES)
    return wgpuResourceTableInsertBinding(handle_.get(), &native_resource);
#elif defined(WOKI_WGPU_HAS_RESOURCE_TABLE_LEGACY_BINDING_NAMES)
    return wgpuResourceTableInsert(handle_.get(), &native_resource);
#else
    (void)native_resource;
    return 0;
#endif
}

Result<void> WgpuResourceTableImpl::RemoveBinding(const u32 slot) {
    if (!handle_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Resource table is invalid");
    }

#if defined(WOKI_WGPU_HAS_RESOURCE_TABLE_BINDING_NAMES)
    return FromWgpuStatus(wgpuResourceTableRemoveBinding(handle_.get(), slot), "Failed to remove resource table binding");
#elif defined(WOKI_WGPU_HAS_RESOURCE_TABLE_LEGACY_BINDING_NAMES)
    return FromWgpuStatus(wgpuResourceTableRemove(handle_.get(), slot), "Failed to remove resource table binding");
#else
    (void)slot;
    return Err(ErrorCode::GraphicsResourceCreationFailed, "Resource table binding mutation is unsupported by this WebGPU implementation");
#endif
}

void WgpuResourceTableImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuResourceTableSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

Result<void> WgpuResourceTableImpl::Update(const u32 slot, const BindingResourceDesc& resource) {
    if (!handle_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Resource table is invalid");
    }

    const WGPUBindingResource native_resource = detail::ToWgpu(resource);
    return FromWgpuStatus(wgpuResourceTableUpdate(handle_.get(), slot, &native_resource), "Failed to update resource table binding");
}

NativeHandles WgpuResourceTableImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

Future WgpuShaderModuleImpl::GetCompilationInfo(const CallbackMode callback_mode, ShaderModuleCompilationInfoCallback callback) const {
    Future future{};
    if (!handle_) {
        future.message = "Shader module is invalid";
        return future;
    }

    if (!callback) {
        future.message = "GetCompilationInfo requires a callback";
        return future;
    }

    auto callback_state = createScope<CompilationInfoCallbackState>(CompilationInfoCallbackState{.callback = std::move(callback)});

    WGPUCompilationInfoCallbackInfo callback_info = WGPU_COMPILATION_INFO_CALLBACK_INFO_INIT;
    callback_info.mode = ToWgpu(callback_mode);
    callback_info.callback = ShaderModuleCompilationInfoThunk;
    callback_info.userdata1 = callback_state.get();

    auto* transferred_state = callback_state.release();
    const WGPUFuture native_future = wgpuShaderModuleGetCompilationInfo(handle_.get(), callback_info);
    future.id = native_future.id;
    if (future.id == 0) {
        callback_state.reset(transferred_state);
        future.message = "Compilation info request failed to start";
    }
    return future;
}

void WgpuShaderModuleImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuShaderModuleSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuShaderModuleImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

Result<void> WgpuSharedBufferMemoryImpl::BeginAccess(const Buffer& buffer, const SharedBufferMemoryBeginAccessDesc& desc) const {
    if (!handle_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Shared buffer memory is invalid");
    }

    WGPUSharedBufferMemoryBeginAccessDescriptor native = WGPU_SHARED_BUFFER_MEMORY_BEGIN_ACCESS_DESCRIPTOR_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);

    return FromWgpuStatus(wgpuSharedBufferMemoryBeginAccess(handle_.get(), detail::NativeBuffer(buffer), &native), "Failed to begin shared buffer memory access");
}

Result<scope<Buffer>> WgpuSharedBufferMemoryImpl::CreateBuffer(const BufferDesc& desc) const {
    if (!handle_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Shared buffer memory is invalid");
    }

    const detail::BufferDescriptorStorage storage(desc);

    const WGPUBuffer buffer = wgpuSharedBufferMemoryCreateBuffer(handle_.get(), &storage.native);
    if (buffer == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to create shared buffer");
    }

    return Ok(CreateBufferObject(buffer));
}

Result<void> WgpuSharedBufferMemoryImpl::EndAccess(const Buffer& buffer, SharedBufferMemoryEndAccessState& state) const {
    if (!handle_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Shared buffer memory is invalid");
    }

    WGPUSharedBufferMemoryEndAccessState native = WGPU_SHARED_BUFFER_MEMORY_END_ACCESS_STATE_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(state.next_in_chain);

    const WGPUStatus status = wgpuSharedBufferMemoryEndAccess(handle_.get(), detail::NativeBuffer(buffer), &native);
    state.next_in_chain = native.nextInChain;

    return FromWgpuStatus(status, "Failed to end shared buffer memory access");
}

Result<void> WgpuSharedBufferMemoryImpl::GetProperties(SharedBufferMemoryProperties& properties) const {
    if (!handle_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Shared buffer memory is invalid");
    }

    WGPUSharedBufferMemoryProperties native = WGPU_SHARED_BUFFER_MEMORY_PROPERTIES_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(properties.next_in_chain);

    const WGPUStatus status = wgpuSharedBufferMemoryGetProperties(handle_.get(), &native);
    properties.next_in_chain = native.nextInChain;

    return FromWgpuStatus(status, "Failed to get shared buffer memory properties");
}

bool WgpuSharedBufferMemoryImpl::IsDeviceLost() const {
    if (!handle_) {
        return false;
    }

    return wgpuSharedBufferMemoryIsDeviceLost(handle_.get()) != 0;
}

void WgpuSharedBufferMemoryImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuSharedBufferMemorySetLabel(handle_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuSharedBufferMemoryImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

void WgpuSharedFenceImpl::ExportInfo(SharedFenceExportInfo& info) const {
    if (!handle_) {
        return;
    }

    WGPUSharedFenceExportInfo native = WGPU_SHARED_FENCE_EXPORT_INFO_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(info.next_in_chain);
    wgpuSharedFenceExportInfo(handle_.get(), &native);
    info.next_in_chain = native.nextInChain;
}

void WgpuSharedFenceImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuSharedFenceSetLabel(handle_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuSharedFenceImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

Result<void> WgpuSharedTextureMemoryImpl::BeginAccess(const Texture& texture, const SharedTextureMemoryBeginAccessDesc& desc) const {
    if (!handle_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Shared texture memory is invalid");
    }

    WGPUSharedTextureMemoryBeginAccessDescriptor native = WGPU_SHARED_TEXTURE_MEMORY_BEGIN_ACCESS_DESCRIPTOR_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);

    return FromWgpuStatus(wgpuSharedTextureMemoryBeginAccess(handle_.get(), NativeTexture(texture), &native), "Failed to begin shared texture memory access");
}

Result<scope<Texture>> WgpuSharedTextureMemoryImpl::CreateTexture(const TextureDesc& desc) const {
    if (!handle_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Shared texture memory is invalid");
    }

    const detail::TextureDescriptorStorage storage(desc);

    const WGPUTexture texture = wgpuSharedTextureMemoryCreateTexture(handle_.get(), &storage.native);
    if (texture == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to create shared texture");
    }

    return Ok(CreateTextureObject(texture));
}

Result<void> WgpuSharedTextureMemoryImpl::EndAccess(const Texture& texture, SharedTextureMemoryEndAccessState& state) const {
    if (!handle_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Shared texture memory is invalid");
    }

    WGPUSharedTextureMemoryEndAccessState native = WGPU_SHARED_TEXTURE_MEMORY_END_ACCESS_STATE_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(state.next_in_chain);

    const WGPUStatus status = wgpuSharedTextureMemoryEndAccess(handle_.get(), NativeTexture(texture), &native);
    state.next_in_chain = native.nextInChain;

    return FromWgpuStatus(status, "Failed to end shared texture memory access");
}

Result<void> WgpuSharedTextureMemoryImpl::GetProperties(SharedTextureMemoryProperties& properties) const {
    if (!handle_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Shared texture memory is invalid");
    }

    WGPUSharedTextureMemoryProperties native = WGPU_SHARED_TEXTURE_MEMORY_PROPERTIES_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(properties.next_in_chain);

    const WGPUStatus status = wgpuSharedTextureMemoryGetProperties(handle_.get(), &native);
    properties.next_in_chain = native.nextInChain;

    return FromWgpuStatus(status, "Failed to get shared texture memory properties");
}

bool WgpuSharedTextureMemoryImpl::IsDeviceLost() const {
    if (!handle_) {
        return false;
    }

    return wgpuSharedTextureMemoryIsDeviceLost(handle_.get()) != 0;
}

void WgpuSharedTextureMemoryImpl::SetLabel(const std::string_view label) {
    if (handle_) {
        wgpuSharedTextureMemorySetLabel(handle_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuSharedTextureMemoryImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = handle_.get();
    return handles;
}

[[nodiscard]] scope<RenderBundle> CreateRenderBundleObject(const WGPURenderBundle handle) {
    return createScope<WgpuRenderBundleImpl>(handle);
}

[[nodiscard]] scope<TexelBufferView> CreateTexelBufferViewObject(const WGPUTexelBufferView handle) {
    return createScope<WgpuTexelBufferViewImpl>(handle);
}

[[nodiscard]] scope<TextureView> CreateTextureViewObject(const WGPUTextureView handle) {
    return createScope<WgpuTextureViewImpl>(handle);
}

scope<BindGroup> CreateBindGroupObject(const WGPUBindGroup handle) {
    return createScope<WgpuBindGroupImpl>(handle);
}

scope<BindGroupLayout> CreateBindGroupLayoutObject(const WGPUBindGroupLayout handle) {
    return createScope<WgpuBindGroupLayoutImpl>(handle);
}

scope<Buffer> CreateBufferObject(const WGPUBuffer handle) {
    return createScope<WgpuBufferImpl>(handle);
}

scope<ComputePipeline> CreateComputePipelineObject(const WGPUComputePipeline handle) {
    return createScope<WgpuComputePipelineImpl>(handle);
}

scope<ExternalTexture> CreateExternalTextureObject(const WGPUExternalTexture handle) {
    return createScope<WgpuExternalTextureImpl>(handle);
}

scope<PipelineLayout> CreatePipelineLayoutObject(const WGPUPipelineLayout handle) {
    return createScope<WgpuPipelineLayoutImpl>(handle);
}

scope<QuerySet> CreateQuerySetObject(const WGPUQuerySet handle) {
    return createScope<WgpuQuerySetImpl>(handle);
}

scope<RenderBundleEncoder> CreateRenderBundleEncoderObject(const WGPURenderBundleEncoder handle) {
    return createScope<WgpuRenderBundleEncoderImpl>(handle);
}

scope<RenderPipeline> CreateRenderPipelineObject(const WGPURenderPipeline handle) {
    return createScope<WgpuRenderPipelineImpl>(handle);
}

scope<ResourceTable> CreateResourceTableObject(const WGPUResourceTable handle) {
    return createScope<WgpuResourceTableImpl>(handle);
}

scope<Sampler> CreateSamplerObject(const WGPUSampler handle) {
    return createScope<WgpuSamplerImpl>(handle);
}

scope<ShaderModule> CreateShaderModuleObject(const WGPUShaderModule handle) {
    return createScope<WgpuShaderModuleImpl>(handle);
}

scope<SharedBufferMemory> CreateSharedBufferMemoryObject(const WGPUSharedBufferMemory handle) {
    return createScope<WgpuSharedBufferMemoryImpl>(handle);
}

scope<SharedFence> CreateSharedFenceObject(const WGPUSharedFence handle) {
    return createScope<WgpuSharedFenceImpl>(handle);
}

scope<SharedTextureMemory> CreateSharedTextureMemoryObject(const WGPUSharedTextureMemory handle) {
    return createScope<WgpuSharedTextureMemoryImpl>(handle);
}

scope<Texture> CreateTextureObject(const WGPUTexture handle) {
    return createScope<WgpuTextureImpl>(handle);
}

} // namespace woki::rhi::wgpu
