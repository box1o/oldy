#include <vector>

#include "wgpu_enums.hpp"
#include "detail/string.hpp"
#include "detail/native_helpers.hpp"
#include "wgpu_render_pass_encoder.hpp"

namespace woki::rhi::wgpu {

using convert::ToWgpu;

WgpuRenderPassEncoderImpl::WgpuRenderPassEncoderImpl(const WGPURenderPassEncoder encoder) noexcept
    : encoder_(encoder) {}

void WgpuRenderPassEncoderImpl::BeginOcclusionQuery(const u32 query_index) {
    if (encoder_) {
        wgpuRenderPassEncoderBeginOcclusionQuery(encoder_.get(), query_index);
    }
}

void WgpuRenderPassEncoderImpl::Draw(const u32 vertex_count, const u32 instance_count, const u32 first_vertex, const u32 first_instance) {
    if (encoder_) {
        wgpuRenderPassEncoderDraw(encoder_.get(), vertex_count, instance_count, first_vertex, first_instance);
    }
}

void WgpuRenderPassEncoderImpl::DrawIndexed(const u32 index_count, const u32 instance_count, const u32 first_index, const i32 base_vertex, const u32 first_instance) {
    if (encoder_) {
        wgpuRenderPassEncoderDrawIndexed(encoder_.get(), index_count, instance_count, first_index, base_vertex, first_instance);
    }
}

void WgpuRenderPassEncoderImpl::DrawIndexedIndirect(const Buffer& indirect_buffer, const u64 indirect_offset) {
    if (encoder_) {
        wgpuRenderPassEncoderDrawIndexedIndirect(encoder_.get(), detail::NativeBuffer(indirect_buffer), indirect_offset);
    }
}

void WgpuRenderPassEncoderImpl::DrawIndirect(const Buffer& indirect_buffer, const u64 indirect_offset) {
    if (encoder_) {
        wgpuRenderPassEncoderDrawIndirect(encoder_.get(), detail::NativeBuffer(indirect_buffer), indirect_offset);
    }
}

void WgpuRenderPassEncoderImpl::End() {
    if (encoder_) {
        wgpuRenderPassEncoderEnd(encoder_.get());
    }
}

void WgpuRenderPassEncoderImpl::EndOcclusionQuery() {
    if (encoder_) {
        wgpuRenderPassEncoderEndOcclusionQuery(encoder_.get());
    }
}

void WgpuRenderPassEncoderImpl::ExecuteBundles(const std::span<RenderBundle* const> bundles) {
    if (!encoder_) {
        return;
    }

    if (bundles.empty()) {
        wgpuRenderPassEncoderExecuteBundles(encoder_.get(), 0, nullptr);
        return;
    }

    std::vector<WGPURenderBundle> native_bundles{};
    native_bundles.reserve(bundles.size());
    for (RenderBundle* const bundle : bundles) {
        native_bundles.push_back(detail::NativeRenderBundle(bundle));
    }

    wgpuRenderPassEncoderExecuteBundles(encoder_.get(), native_bundles.size(), native_bundles.data());
}

void WgpuRenderPassEncoderImpl::InsertDebugMarker(const std::string_view marker_label) {
    if (encoder_) {
        wgpuRenderPassEncoderInsertDebugMarker(encoder_.get(), detail::ToStringView(marker_label));
    }
}

void WgpuRenderPassEncoderImpl::MultiDrawIndexedIndirect(const Buffer& indirect_buffer, const u64 indirect_offset, const u32 max_draw_count, const Buffer* draw_count_buffer, const u64 draw_count_buffer_offset) {
    if (encoder_) {
        wgpuRenderPassEncoderMultiDrawIndexedIndirect(encoder_.get(), detail::NativeBuffer(indirect_buffer), indirect_offset, max_draw_count,
            draw_count_buffer == nullptr ? nullptr : detail::NativeBuffer(*draw_count_buffer), draw_count_buffer_offset);
    }
}

void WgpuRenderPassEncoderImpl::MultiDrawIndirect(const Buffer& indirect_buffer, const u64 indirect_offset, const u32 max_draw_count, const Buffer* draw_count_buffer, const u64 draw_count_buffer_offset) {
    if (encoder_) {
        wgpuRenderPassEncoderMultiDrawIndirect(encoder_.get(), detail::NativeBuffer(indirect_buffer), indirect_offset, max_draw_count, draw_count_buffer == nullptr ? nullptr : detail::NativeBuffer(*draw_count_buffer),
            draw_count_buffer_offset);
    }
}

void WgpuRenderPassEncoderImpl::PixelLocalStorageBarrier() {
    if (encoder_) {
        wgpuRenderPassEncoderPixelLocalStorageBarrier(encoder_.get());
    }
}

void WgpuRenderPassEncoderImpl::PopDebugGroup() {
    if (encoder_) {
        wgpuRenderPassEncoderPopDebugGroup(encoder_.get());
    }
}

void WgpuRenderPassEncoderImpl::PushDebugGroup(const std::string_view group_label) {
    if (encoder_) {
        wgpuRenderPassEncoderPushDebugGroup(encoder_.get(), detail::ToStringView(group_label));
    }
}

void WgpuRenderPassEncoderImpl::SetBindGroup(const u32 group_index, const BindGroup* group, const std::span<const u32> dynamic_offsets) {
    if (!encoder_) {
        return;
    }

    wgpuRenderPassEncoderSetBindGroup(encoder_.get(), group_index, detail::NativeBindGroup(group), dynamic_offsets.size(), dynamic_offsets.empty() ? nullptr : dynamic_offsets.data());
}

void WgpuRenderPassEncoderImpl::SetBlendConstant(const Color& color) {
    if (encoder_) {
        const WGPUColor native_color{color.r, color.g, color.b, color.a};
        wgpuRenderPassEncoderSetBlendConstant(encoder_.get(), &native_color);
    }
}

void WgpuRenderPassEncoderImpl::SetImmediates(const u32 offset, const void* data, const size_t size) {
    if (encoder_) {
        wgpuRenderPassEncoderSetImmediates(encoder_.get(), offset, data, size);
    }
}

void WgpuRenderPassEncoderImpl::SetIndexBuffer(const Buffer& buffer, const IndexFormat format, const u64 offset, const u64 size) {
    if (encoder_) {
        wgpuRenderPassEncoderSetIndexBuffer(encoder_.get(), detail::NativeBuffer(buffer), ToWgpu(format), offset, size);
    }
}

void WgpuRenderPassEncoderImpl::SetLabel(const std::string_view label) {
    if (encoder_) {
        wgpuRenderPassEncoderSetLabel(encoder_.get(), detail::ToStringView(label));
    }
}

void WgpuRenderPassEncoderImpl::SetPipeline(const RenderPipeline& pipeline) {
    if (encoder_) {
        wgpuRenderPassEncoderSetPipeline(encoder_.get(), detail::NativeRenderPipeline(pipeline));
    }
}

void WgpuRenderPassEncoderImpl::SetResourceTable(const ResourceTable* table) {
    if (encoder_) {
        wgpuRenderPassEncoderSetResourceTable(encoder_.get(), detail::NativeResourceTable(table));
    }
}

void WgpuRenderPassEncoderImpl::SetScissorRect(const u32 x, const u32 y, const u32 width, const u32 height) {
    if (encoder_) {
        wgpuRenderPassEncoderSetScissorRect(encoder_.get(), x, y, width, height);
    }
}

void WgpuRenderPassEncoderImpl::SetStencilReference(const u32 reference) {
    if (encoder_) {
        wgpuRenderPassEncoderSetStencilReference(encoder_.get(), reference);
    }
}

void WgpuRenderPassEncoderImpl::SetVertexBuffer(const u32 slot, const Buffer* buffer, const u64 offset, const u64 size) {
    if (encoder_) {
        wgpuRenderPassEncoderSetVertexBuffer(encoder_.get(), slot, buffer == nullptr ? nullptr : detail::NativeBuffer(*buffer), offset, size);
    }
}

void WgpuRenderPassEncoderImpl::SetViewport(const f32 x, const f32 y, const f32 width, const f32 height, const f32 min_depth, const f32 max_depth) {
    if (encoder_) {
        wgpuRenderPassEncoderSetViewport(encoder_.get(), x, y, width, height, min_depth, max_depth);
    }
}

void WgpuRenderPassEncoderImpl::WriteTimestamp(const QuerySet& query_set, const u32 query_index) {
    if (encoder_) {
        wgpuRenderPassEncoderWriteTimestamp(encoder_.get(), detail::NativeQuerySet(query_set), query_index);
    }
}

NativeHandles WgpuRenderPassEncoderImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = encoder_.get();
    return handles;
}

} // namespace woki::rhi::wgpu
