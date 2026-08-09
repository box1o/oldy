#pragma once

#include <vector>
#include <optional>
#include <webgpu/webgpu.h>

#include <woki/rhi/descriptors.hpp>

#include "string.hpp"
#include "../wgpu_enums.hpp"
#include "native_helpers.hpp"

namespace woki::rhi::wgpu::detail {

using convert::ToWgpu;

[[nodiscard]] inline WGPUColor ToWgpuColor(const Color& color) noexcept {
    return WGPUColor{color.r, color.g, color.b, color.a};
}

[[nodiscard]] inline WGPURenderPassColorAttachment ToWgpu(const RenderPassColorAttachmentDesc& desc) noexcept {
    WGPURenderPassColorAttachment native = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    native.view = desc.view == nullptr ? nullptr : static_cast<WGPUTextureView>(desc.view->GetNativeHandles().resource);
    native.resolveTarget = desc.resolve_target == nullptr ? nullptr : static_cast<WGPUTextureView>(desc.resolve_target->GetNativeHandles().resource);
    native.depthSlice = desc.depth_slice;
    native.loadOp = ToWgpu(desc.load_op);
    native.storeOp = ToWgpu(desc.store_op);
    native.clearValue = ToWgpuColor(desc.clear_value);
    return native;
}

[[nodiscard]] inline WGPURenderPassDepthStencilAttachment ToWgpu(const RenderPassDepthStencilAttachmentDesc& desc) noexcept {
    WGPURenderPassDepthStencilAttachment native = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    native.view = desc.view == nullptr ? nullptr : static_cast<WGPUTextureView>(desc.view->GetNativeHandles().resource);
    native.depthLoadOp = ToWgpu(desc.depth_load_op);
    native.depthStoreOp = ToWgpu(desc.depth_store_op);
    native.depthClearValue = desc.depth_clear_value;
    native.depthReadOnly = desc.depth_read_only ? WGPU_TRUE : WGPU_FALSE;
    native.stencilLoadOp = ToWgpu(desc.stencil_load_op);
    native.stencilStoreOp = ToWgpu(desc.stencil_store_op);
    native.stencilClearValue = desc.stencil_clear_value;
    native.stencilReadOnly = desc.stencil_read_only ? WGPU_TRUE : WGPU_FALSE;
    return native;
}

[[nodiscard]] inline WGPUPassTimestampWrites ToWgpu(const PassTimestampWritesDesc& desc) noexcept {
    WGPUPassTimestampWrites native = WGPU_PASS_TIMESTAMP_WRITES_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    native.querySet = desc.query_set == nullptr ? nullptr : static_cast<WGPUQuerySet>(desc.query_set->GetNativeHandles().resource);
    native.beginningOfPassWriteIndex = desc.beginning_of_pass_write_index;
    native.endOfPassWriteIndex = desc.end_of_pass_write_index;
    return native;
}

struct RenderPassDescriptorStorage final {
    std::vector<WGPURenderPassColorAttachment> color_attachments{};
    WGPURenderPassDepthStencilAttachment depth_stencil_attachment = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
    std::optional<WGPUPassTimestampWrites> timestamp_writes{};
    WGPURenderPassDescriptor native = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    bool has_depth_stencil{false};

    explicit RenderPassDescriptorStorage(const RenderPassDescTyped& desc) {
        color_attachments.reserve(desc.color_attachments.size());
        for (const RenderPassColorAttachmentDesc& attachment : desc.color_attachments) {
            color_attachments.push_back(ToWgpu(attachment));
        }

        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.colorAttachmentCount = color_attachments.size();
        native.colorAttachments = color_attachments.empty() ? nullptr : color_attachments.data();
        if (desc.timestamp_writes != nullptr) {
            timestamp_writes = ToWgpu(*desc.timestamp_writes);
            native.timestampWrites = &*timestamp_writes;
        }
        native.occlusionQuerySet = desc.occlusion_query_set == nullptr ? nullptr : static_cast<WGPUQuerySet>(desc.occlusion_query_set->GetNativeHandles().resource);

        if (desc.depth_stencil_attachment != nullptr) {
            depth_stencil_attachment = ToWgpu(*desc.depth_stencil_attachment);
            native.depthStencilAttachment = &depth_stencil_attachment;
            has_depth_stencil = true;
        }
    }
};

} // namespace woki::rhi::wgpu::detail
