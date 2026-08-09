#include "detail/string.hpp"
#include "detail/native_helpers.hpp"
#include "wgpu_compute_pass_encoder.hpp"

namespace woki::rhi::wgpu {

WgpuComputePassEncoderImpl::WgpuComputePassEncoderImpl(const WGPUComputePassEncoder encoder) noexcept
    : encoder_(encoder) {}

void WgpuComputePassEncoderImpl::DispatchWorkgroups(const u32 workgroup_count_x, const u32 workgroup_count_y, const u32 workgroup_count_z) {
    if (encoder_) {
        wgpuComputePassEncoderDispatchWorkgroups(encoder_.get(), workgroup_count_x, workgroup_count_y, workgroup_count_z);
    }
}

void WgpuComputePassEncoderImpl::DispatchWorkgroupsIndirect(const Buffer& indirect_buffer, const u64 indirect_offset) {
    if (encoder_) {
        wgpuComputePassEncoderDispatchWorkgroupsIndirect(encoder_.get(), detail::NativeBuffer(indirect_buffer), indirect_offset);
    }
}

void WgpuComputePassEncoderImpl::End() {
    if (encoder_) {
        wgpuComputePassEncoderEnd(encoder_.get());
    }
}

void WgpuComputePassEncoderImpl::InsertDebugMarker(const std::string_view marker_label) {
    if (encoder_) {
        wgpuComputePassEncoderInsertDebugMarker(encoder_.get(), detail::ToStringView(marker_label));
    }
}

void WgpuComputePassEncoderImpl::PopDebugGroup() {
    if (encoder_) {
        wgpuComputePassEncoderPopDebugGroup(encoder_.get());
    }
}

void WgpuComputePassEncoderImpl::PushDebugGroup(const std::string_view group_label) {
    if (encoder_) {
        wgpuComputePassEncoderPushDebugGroup(encoder_.get(), detail::ToStringView(group_label));
    }
}

void WgpuComputePassEncoderImpl::SetBindGroup(const u32 group_index, const BindGroup* group, const std::span<const u32> dynamic_offsets) {
    if (!encoder_) {
        return;
    }

    wgpuComputePassEncoderSetBindGroup(encoder_.get(), group_index, detail::NativeBindGroup(group), dynamic_offsets.size(), dynamic_offsets.empty() ? nullptr : dynamic_offsets.data());
}

void WgpuComputePassEncoderImpl::SetImmediates(const u32 offset, const void* data, const size_t size) {
    if (encoder_) {
        wgpuComputePassEncoderSetImmediates(encoder_.get(), offset, data, size);
    }
}

void WgpuComputePassEncoderImpl::SetLabel(const std::string_view label) {
    if (encoder_) {
        wgpuComputePassEncoderSetLabel(encoder_.get(), detail::ToStringView(label));
    }
}

void WgpuComputePassEncoderImpl::SetPipeline(const ComputePipeline& pipeline) {
    if (encoder_) {
        wgpuComputePassEncoderSetPipeline(encoder_.get(), detail::NativeComputePipeline(pipeline));
    }
}

void WgpuComputePassEncoderImpl::SetResourceTable(const ResourceTable* table) {
    if (encoder_) {
        wgpuComputePassEncoderSetResourceTable(encoder_.get(), detail::NativeResourceTable(table));
    }
}

void WgpuComputePassEncoderImpl::WriteTimestamp(const QuerySet& query_set, const u32 query_index) {
    if (encoder_) {
        wgpuComputePassEncoderWriteTimestamp(encoder_.get(), detail::NativeQuerySet(query_set), query_index);
    }
}

NativeHandles WgpuComputePassEncoderImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = encoder_.get();
    return handles;
}

} // namespace woki::rhi::wgpu
