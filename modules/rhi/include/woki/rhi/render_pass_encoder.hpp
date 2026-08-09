#pragma once

#include <span>
#include <string_view>

#include <woki/core.hpp>

#include "types.hpp"
#include "forward.hpp"
#include "descriptors.hpp"

namespace woki::rhi {

class RenderPassEncoder {
public:
    virtual ~RenderPassEncoder() = default;

    virtual void BeginOcclusionQuery(u32 query_index) = 0;
    virtual void Draw(u32 vertex_count, u32 instance_count = 1, u32 first_vertex = 0, u32 first_instance = 0) = 0;
    virtual void DrawIndexed(u32 index_count, u32 instance_count = 1, u32 first_index = 0, i32 base_vertex = 0, u32 first_instance = 0) = 0;
    virtual void DrawIndexedIndirect(const Buffer& indirect_buffer, u64 indirect_offset) = 0;
    virtual void DrawIndirect(const Buffer& indirect_buffer, u64 indirect_offset) = 0;
    virtual void End() = 0;
    virtual void EndOcclusionQuery() = 0;
    virtual void ExecuteBundles(std::span<RenderBundle* const> bundles) = 0;
    virtual void InsertDebugMarker(std::string_view marker_label) = 0;
    virtual void MultiDrawIndexedIndirect(const Buffer& indirect_buffer, u64 indirect_offset, u32 max_draw_count, const Buffer* draw_count_buffer = nullptr, u64 draw_count_buffer_offset = 0) = 0;
    virtual void MultiDrawIndirect(const Buffer& indirect_buffer, u64 indirect_offset, u32 max_draw_count, const Buffer* draw_count_buffer = nullptr, u64 draw_count_buffer_offset = 0) = 0;
    virtual void PixelLocalStorageBarrier() = 0;
    virtual void PopDebugGroup() = 0;
    virtual void PushDebugGroup(std::string_view group_label) = 0;
    virtual void SetBindGroup(u32 group_index, const BindGroup* group = nullptr, std::span<const u32> dynamic_offsets = {}) = 0;
    virtual void SetBlendConstant(const Color& color) = 0;
    virtual void SetImmediates(u32 offset, const void* data, size_t size) = 0;
    virtual void SetIndexBuffer(const Buffer& buffer, IndexFormat format, u64 offset = 0, u64 size = kWholeSize) = 0;
    virtual void SetLabel(std::string_view label) = 0;
    virtual void SetPipeline(const RenderPipeline& pipeline) = 0;
    virtual void SetResourceTable(const ResourceTable* table = nullptr) = 0;
    virtual void SetScissorRect(u32 x, u32 y, u32 width, u32 height) = 0;
    virtual void SetStencilReference(u32 reference) = 0;
    virtual void SetVertexBuffer(u32 slot, const Buffer* buffer = nullptr, u64 offset = 0, u64 size = kWholeSize) = 0;
    virtual void SetViewport(f32 x, f32 y, f32 width, f32 height, f32 min_depth, f32 max_depth) = 0;
    virtual void WriteTimestamp(const QuerySet& query_set, u32 query_index) = 0;

    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    RenderPassEncoder() = default;
};

} // namespace woki::rhi
