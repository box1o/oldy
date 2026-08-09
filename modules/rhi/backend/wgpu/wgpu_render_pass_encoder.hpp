#pragma once

#include <woki/rhi/render_pass_encoder.hpp>

#include "detail/handle.hpp"

namespace woki::rhi::wgpu {

class WgpuRenderPassEncoderImpl final : public RenderPassEncoder {
public:
    explicit WgpuRenderPassEncoderImpl(WGPURenderPassEncoder encoder) noexcept;
    ~WgpuRenderPassEncoderImpl() override = default;

    void BeginOcclusionQuery(u32 query_index) override;
    void Draw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance) override;
    void DrawIndexed(u32 index_count, u32 instance_count, u32 first_index, i32 base_vertex, u32 first_instance) override;
    void DrawIndexedIndirect(const Buffer& indirect_buffer, u64 indirect_offset) override;
    void DrawIndirect(const Buffer& indirect_buffer, u64 indirect_offset) override;
    void End() override;
    void EndOcclusionQuery() override;
    void ExecuteBundles(std::span<RenderBundle* const> bundles) override;
    void InsertDebugMarker(std::string_view marker_label) override;
    void MultiDrawIndexedIndirect(const Buffer& indirect_buffer, u64 indirect_offset, u32 max_draw_count, const Buffer* draw_count_buffer, u64 draw_count_buffer_offset) override;
    void MultiDrawIndirect(const Buffer& indirect_buffer, u64 indirect_offset, u32 max_draw_count, const Buffer* draw_count_buffer, u64 draw_count_buffer_offset) override;
    void PixelLocalStorageBarrier() override;
    void PopDebugGroup() override;
    void PushDebugGroup(std::string_view group_label) override;
    void SetBindGroup(u32 group_index, const BindGroup* group, std::span<const u32> dynamic_offsets) override;
    void SetBlendConstant(const Color& color) override;
    void SetImmediates(u32 offset, const void* data, size_t size) override;
    void SetIndexBuffer(const Buffer& buffer, IndexFormat format, u64 offset, u64 size) override;
    void SetLabel(std::string_view label) override;
    void SetPipeline(const RenderPipeline& pipeline) override;
    void SetResourceTable(const ResourceTable* table) override;
    void SetScissorRect(u32 x, u32 y, u32 width, u32 height) override;
    void SetStencilReference(u32 reference) override;
    void SetVertexBuffer(u32 slot, const Buffer* buffer, u64 offset, u64 size) override;
    void SetViewport(f32 x, f32 y, f32 width, f32 height, f32 min_depth, f32 max_depth) override;
    void WriteTimestamp(const QuerySet& query_set, u32 query_index) override;

    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::RenderPassEncoderHandle encoder_;
};

} // namespace woki::rhi::wgpu
