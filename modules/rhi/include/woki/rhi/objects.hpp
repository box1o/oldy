#pragma once

#include <span>
#include <cstddef>
#include <string_view>

#include <woki/core.hpp>

#include "types.hpp"
#include "../enums.hpp"
#include "descriptors.hpp"

namespace woki::rhi {

class RenderPipeline;
class ResourceTable;
class Texture;

class BindGroup {
public:
    virtual ~BindGroup() = default;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    BindGroup() = default;
};

class BindGroupLayout {
public:
    virtual ~BindGroupLayout() = default;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    BindGroupLayout() = default;
};

class TexelBufferView {
public:
    virtual ~TexelBufferView() = default;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    TexelBufferView() = default;
};

class TextureView {
public:
    virtual ~TextureView() = default;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    TextureView() = default;
};

class Buffer {
public:
    virtual ~Buffer() = default;

    [[nodiscard]] virtual Result<scope<TexelBufferView>> CreateTexelView(const TexelBufferViewDesc& desc = {}) const = 0;
    virtual void Destroy() = 0;
    [[nodiscard]] virtual const void* GetConstMappedRange(size_t offset = 0, size_t size = kWholeMapSize) const = 0;
    [[nodiscard]] virtual void* GetMappedRange(size_t offset = 0, size_t size = kWholeMapSize) = 0;
    [[nodiscard]] virtual BufferMapState GetMapState() const = 0;
    [[nodiscard]] virtual u64 GetSize() const = 0;
    [[nodiscard]] virtual BufferUsage GetUsage() const = 0;
    [[nodiscard]] virtual Future MapAsync(MapMode mode, size_t offset, size_t size, CallbackMode callback_mode, MapAsyncCallback callback) const = 0;
    [[nodiscard]] virtual Result<void> ReadMappedRange(size_t offset, void* data, size_t size) const = 0;
    virtual void SetLabel(std::string_view label) = 0;
    virtual void Unmap() = 0;
    [[nodiscard]] virtual Result<void> WriteMappedRange(size_t offset, const void* data, size_t size) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    Buffer() = default;
};

class ComputePipeline {
public:
    virtual ~ComputePipeline() = default;
    [[nodiscard]] virtual scope<BindGroupLayout> GetBindGroupLayout(u32 group_index) const = 0;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    ComputePipeline() = default;
};

class ExternalTexture {
public:
    virtual ~ExternalTexture() = default;
    virtual void Destroy() = 0;
    virtual void Expire() = 0;
    virtual void Refresh() = 0;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    ExternalTexture() = default;
};

class PipelineLayout {
public:
    virtual ~PipelineLayout() = default;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    PipelineLayout() = default;
};

class QuerySet {
public:
    virtual ~QuerySet() = default;
    virtual void Destroy() = 0;
    [[nodiscard]] virtual u32 GetCount() const = 0;
    [[nodiscard]] virtual QueryType GetType() const = 0;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    QuerySet() = default;
};

class RenderBundle {
public:
    virtual ~RenderBundle() = default;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    RenderBundle() = default;
};

class RenderBundleEncoder {
public:
    virtual ~RenderBundleEncoder() = default;

    virtual void Draw(u32 vertex_count, u32 instance_count = 1, u32 first_vertex = 0, u32 first_instance = 0) = 0;
    virtual void DrawIndexed(u32 index_count, u32 instance_count = 1, u32 first_index = 0, i32 base_vertex = 0, u32 first_instance = 0) = 0;
    virtual void DrawIndexedIndirect(const Buffer& indirect_buffer, u64 indirect_offset) = 0;
    virtual void DrawIndirect(const Buffer& indirect_buffer, u64 indirect_offset) = 0;
    [[nodiscard]] virtual Result<scope<RenderBundle>> Finish(const RenderBundleDesc& desc = {}) = 0;
    virtual void InsertDebugMarker(std::string_view marker_label) = 0;
    virtual void PopDebugGroup() = 0;
    virtual void PushDebugGroup(std::string_view group_label) = 0;
    virtual void SetBindGroup(u32 group_index, const BindGroup* group = nullptr, std::span<const u32> dynamic_offsets = {}) = 0;
    virtual void SetImmediates(u32 offset, const void* data, size_t size) = 0;
    virtual void SetIndexBuffer(const Buffer& buffer, IndexFormat format, u64 offset = 0, u64 size = kWholeSize) = 0;
    virtual void SetLabel(std::string_view label) = 0;
    virtual void SetPipeline(const RenderPipeline& pipeline) = 0;
    virtual void SetResourceTable(const ResourceTable* table = nullptr) = 0;
    virtual void SetVertexBuffer(u32 slot, const Buffer* buffer = nullptr, u64 offset = 0, u64 size = kWholeSize) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    RenderBundleEncoder() = default;
};

class RenderPipeline {
public:
    virtual ~RenderPipeline() = default;
    [[nodiscard]] virtual scope<BindGroupLayout> GetBindGroupLayout(u32 group_index) const = 0;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    RenderPipeline() = default;
};

class ResourceTable {
public:
    virtual ~ResourceTable() = default;
    virtual void Destroy() = 0;
    [[nodiscard]] virtual u32 GetSize() const = 0;
    [[nodiscard]] virtual u32 InsertBinding(const BindingResourceDesc& resource) = 0;
    [[nodiscard]] virtual Result<void> RemoveBinding(u32 slot) = 0;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual Result<void> Update(u32 slot, const BindingResourceDesc& resource) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    ResourceTable() = default;
};

class Sampler {
public:
    virtual ~Sampler() = default;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    Sampler() = default;
};

class ShaderModule {
public:
    virtual ~ShaderModule() = default;
    [[nodiscard]] virtual Future GetCompilationInfo(CallbackMode callback_mode, ShaderModuleCompilationInfoCallback callback) const = 0;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    ShaderModule() = default;
};

class SharedBufferMemory {
public:
    virtual ~SharedBufferMemory() = default;
    [[nodiscard]] virtual Result<void> BeginAccess(const Buffer& buffer, const SharedBufferMemoryBeginAccessDesc& desc) const = 0;
    [[nodiscard]] virtual Result<scope<Buffer>> CreateBuffer(const BufferDesc& desc = {}) const = 0;
    [[nodiscard]] virtual Result<void> EndAccess(const Buffer& buffer, SharedBufferMemoryEndAccessState& state) const = 0;
    [[nodiscard]] virtual Result<void> GetProperties(SharedBufferMemoryProperties& properties) const = 0;
    [[nodiscard]] virtual bool IsDeviceLost() const = 0;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    SharedBufferMemory() = default;
};

class SharedFence {
public:
    virtual ~SharedFence() = default;
    virtual void ExportInfo(SharedFenceExportInfo& info) const = 0;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    SharedFence() = default;
};

class SharedTextureMemory {
public:
    virtual ~SharedTextureMemory() = default;
    [[nodiscard]] virtual Result<void> BeginAccess(const Texture& texture, const SharedTextureMemoryBeginAccessDesc& desc) const = 0;
    [[nodiscard]] virtual Result<scope<Texture>> CreateTexture(const TextureDesc& desc = {}) const = 0;
    [[nodiscard]] virtual Result<void> EndAccess(const Texture& texture, SharedTextureMemoryEndAccessState& state) const = 0;
    [[nodiscard]] virtual Result<void> GetProperties(SharedTextureMemoryProperties& properties) const = 0;
    [[nodiscard]] virtual bool IsDeviceLost() const = 0;
    virtual void SetLabel(std::string_view label) = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    SharedTextureMemory() = default;
};

class Texture {
public:
    virtual ~Texture() = default;

    [[nodiscard]] virtual scope<TextureView> CreateErrorView(const TextureViewDesc& desc = {}) const = 0;
    [[nodiscard]] virtual scope<TextureView> CreateView(const TextureViewDesc& desc = {}) const = 0;
    virtual void Destroy() = 0;
    [[nodiscard]] virtual u32 GetDepthOrArrayLayers() const = 0;
    [[nodiscard]] virtual TextureDimension GetDimension() const = 0;
    [[nodiscard]] virtual TextureFormat GetFormat() const = 0;
    [[nodiscard]] virtual u32 GetHeight() const = 0;
    [[nodiscard]] virtual u32 GetMipLevelCount() const = 0;
    [[nodiscard]] virtual u32 GetSampleCount() const = 0;
    [[nodiscard]] virtual TextureViewDimension GetTextureBindingViewDimension() const = 0;
    [[nodiscard]] virtual TextureUsage GetUsage() const = 0;
    [[nodiscard]] virtual u32 GetWidth() const = 0;
    virtual void Pin(TextureUsage usage) = 0;
    virtual void SetLabel(std::string_view label) = 0;
    virtual void SetOwnershipForMemoryDump(u64 owner_guid = 0) = 0;
    virtual void Unpin() = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    Texture() = default;
};

} // namespace woki::rhi
