#include <optional>

#include "detail/string.hpp"
#include "detail/copy_convert.hpp"
#include "wgpu_command_buffer.hpp"
#include "wgpu_command_encoder.hpp"
#include "detail/native_helpers.hpp"
#include "wgpu_render_pass_encoder.hpp"
#include "wgpu_compute_pass_encoder.hpp"
#include "detail/render_pass_descriptor.hpp"

namespace woki::rhi::wgpu {

WgpuCommandEncoderImpl::WgpuCommandEncoderImpl(const WGPUCommandEncoder encoder) noexcept
    : encoder_(encoder) {}

Result<scope<ComputePassEncoder>> WgpuCommandEncoderImpl::BeginComputePass(const ComputePassDesc& desc) {
    if (!encoder_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Command encoder is invalid");
    }

    WGPUComputePassDescriptor native = WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
    native.label = detail::ToStringView(desc.label);
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    std::optional<WGPUPassTimestampWrites> timestamp_writes{};
    if (desc.timestamp_writes != nullptr) {
        timestamp_writes = detail::ToWgpu(*desc.timestamp_writes);
        native.timestampWrites = &*timestamp_writes;
    }

    WGPUComputePassEncoder pass = wgpuCommandEncoderBeginComputePass(encoder_.get(), &native);
    if (pass == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to begin compute pass");
    }

    return Ok(createScope<WgpuComputePassEncoderImpl>(pass));
}

Result<scope<RenderPassEncoder>> WgpuCommandEncoderImpl::BeginRenderPass(const RenderPassDesc& desc) {
    if (!encoder_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Command encoder is invalid");
    }

    WGPURenderPassDescriptor native = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    native.label = detail::ToStringView(desc.label);
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    native.colorAttachmentCount = desc.color_attachment_count;
    native.colorAttachments = static_cast<WGPURenderPassColorAttachment*>(desc.color_attachments);
    native.depthStencilAttachment = static_cast<WGPURenderPassDepthStencilAttachment*>(desc.depth_stencil_attachment);
    native.occlusionQuerySet = static_cast<WGPUQuerySet>(desc.occlusion_query_set);
    native.timestampWrites = static_cast<WGPUPassTimestampWrites*>(desc.timestamp_writes);

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder_.get(), &native);
    if (pass == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to begin render pass");
    }

    return Ok(createScope<WgpuRenderPassEncoderImpl>(pass));
}

Result<scope<RenderPassEncoder>> WgpuCommandEncoderImpl::BeginRenderPass(const RenderPassDescTyped& desc) {
    if (!encoder_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Command encoder is invalid");
    }

    const detail::RenderPassDescriptorStorage storage(desc);
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder_.get(), &storage.native);
    if (pass == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to begin render pass");
    }

    return Ok(createScope<WgpuRenderPassEncoderImpl>(pass));
}

Result<void> WgpuCommandEncoderImpl::ClearBuffer(const Buffer& buffer, const u64 offset, const u64 size) {
    if (!encoder_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Command encoder is invalid");
    }

    wgpuCommandEncoderClearBuffer(encoder_.get(), detail::NativeBuffer(buffer), offset, size);
    return Ok();
}

Result<void> WgpuCommandEncoderImpl::CopyBufferToBuffer(const Buffer& source, const u64 source_offset, const Buffer& destination, const u64 destination_offset, const u64 size) {
    if (!encoder_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Command encoder is invalid");
    }

    wgpuCommandEncoderCopyBufferToBuffer(encoder_.get(), detail::NativeBuffer(source), source_offset, detail::NativeBuffer(destination), destination_offset, size);
    return Ok();
}

Result<void> WgpuCommandEncoderImpl::CopyBufferToTexture(const TexelCopyBufferInfo& source, const TexelCopyTextureInfo& destination, const Extent3D& copy_size) {
    if (!encoder_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Command encoder is invalid");
    }

    const auto native_source = detail::copy::ToWgpu(source);
    const auto native_destination = detail::copy::ToWgpu(destination);
    const auto native_size = detail::copy::ToWgpu(copy_size);

    wgpuCommandEncoderCopyBufferToTexture(encoder_.get(), &native_source, &native_destination, &native_size);
    return Ok();
}

Result<void> WgpuCommandEncoderImpl::CopyTextureToBuffer(const TexelCopyTextureInfo& source, const TexelCopyBufferInfo& destination, const Extent3D& copy_size) {
    if (!encoder_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Command encoder is invalid");
    }

    const auto native_source = detail::copy::ToWgpu(source);
    const auto native_destination = detail::copy::ToWgpu(destination);
    const auto native_size = detail::copy::ToWgpu(copy_size);

    wgpuCommandEncoderCopyTextureToBuffer(encoder_.get(), &native_source, &native_destination, &native_size);
    return Ok();
}

Result<void> WgpuCommandEncoderImpl::CopyTextureToTexture(const TexelCopyTextureInfo& source, const TexelCopyTextureInfo& destination, const Extent3D& copy_size) {
    if (!encoder_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Command encoder is invalid");
    }

    const auto native_source = detail::copy::ToWgpu(source);
    const auto native_destination = detail::copy::ToWgpu(destination);
    const auto native_size = detail::copy::ToWgpu(copy_size);

    wgpuCommandEncoderCopyTextureToTexture(encoder_.get(), &native_source, &native_destination, &native_size);
    return Ok();
}

Result<scope<CommandBuffer>> WgpuCommandEncoderImpl::Finish(const CommandBufferDesc& desc) {
    if (!encoder_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Command encoder is invalid");
    }

    WGPUCommandBufferDescriptor native = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
    native.label = detail::ToStringView(desc.label);
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);

    WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(encoder_.get(), &native);
    if (command_buffer == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to finish command encoder");
    }

    return Ok(createScope<WgpuCommandBufferImpl>(command_buffer));
}

void WgpuCommandEncoderImpl::InjectValidationError(const std::string_view message) {
#ifdef __EMSCRIPTEN__
    (void)message;
#else
    if (encoder_) {
        wgpuCommandEncoderInjectValidationError(encoder_.get(), detail::ToStringView(message));
    }
#endif
}

void WgpuCommandEncoderImpl::InsertDebugMarker(const std::string_view marker_label) {
    if (encoder_) {
        wgpuCommandEncoderInsertDebugMarker(encoder_.get(), detail::ToStringView(marker_label));
    }
}

void WgpuCommandEncoderImpl::PopDebugGroup() {
    if (encoder_) {
        wgpuCommandEncoderPopDebugGroup(encoder_.get());
    }
}

void WgpuCommandEncoderImpl::PushDebugGroup(const std::string_view group_label) {
    if (encoder_) {
        wgpuCommandEncoderPushDebugGroup(encoder_.get(), detail::ToStringView(group_label));
    }
}

Result<void> WgpuCommandEncoderImpl::ResolveQuerySet(const QuerySet& query_set, const u32 first_query, const u32 query_count, const Buffer& destination, const u64 destination_offset) {
    if (!encoder_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Command encoder is invalid");
    }

    wgpuCommandEncoderResolveQuerySet(encoder_.get(), detail::NativeQuerySet(query_set), first_query, query_count, detail::NativeBuffer(destination), destination_offset);
    return Ok();
}

void WgpuCommandEncoderImpl::SetLabel(const std::string_view label) {
    if (encoder_) {
        wgpuCommandEncoderSetLabel(encoder_.get(), detail::ToStringView(label));
    }
}

Result<void> WgpuCommandEncoderImpl::WriteBuffer(const Buffer& buffer, const u64 buffer_offset, const u8* data, const u64 size) {
#ifdef __EMSCRIPTEN__
    (void)buffer;
    (void)buffer_offset;
    (void)data;
    (void)size;
    return Err(ErrorCode::GraphicsUnsupportedApi, "Command encoder buffer writes are unavailable with emdawnwebgpu");
#else
    if (!encoder_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Command encoder is invalid");
    }

    if (data == nullptr && size != 0) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "WriteBuffer data is null");
    }

    wgpuCommandEncoderWriteBuffer(encoder_.get(), detail::NativeBuffer(buffer), buffer_offset, data, size);
    return Ok();
#endif
}

Result<void> WgpuCommandEncoderImpl::WriteTimestamp(const QuerySet& query_set, const u32 query_index) {
    if (!encoder_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Command encoder is invalid");
    }

    wgpuCommandEncoderWriteTimestamp(encoder_.get(), detail::NativeQuerySet(query_set), query_index);
    return Ok();
}

NativeHandles WgpuCommandEncoderImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = encoder_.get();
    return handles;
}

} // namespace woki::rhi::wgpu
