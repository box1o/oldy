#pragma once

#include <span>
#include <string_view>

#include <woki/core.hpp>

#include "forward.hpp"
#include "descriptors.hpp"
#include "command_buffer.hpp"

namespace woki::rhi {

class CommandEncoder {
public:
    virtual ~CommandEncoder() = default;

    [[nodiscard]] virtual Result<scope<ComputePassEncoder>> BeginComputePass(const ComputePassDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<RenderPassEncoder>> BeginRenderPass(const RenderPassDesc& desc) = 0;
    [[nodiscard]] virtual Result<scope<RenderPassEncoder>> BeginRenderPass(const RenderPassDescTyped& desc) = 0;

    [[nodiscard]] virtual Result<void> ClearBuffer(const Buffer& buffer, u64 offset = 0, u64 size = kWholeSize) = 0;

    [[nodiscard]] virtual Result<void> CopyBufferToBuffer(const Buffer& source, u64 source_offset, const Buffer& destination, u64 destination_offset, u64 size) = 0;

    [[nodiscard]] virtual Result<void> CopyBufferToTexture(const TexelCopyBufferInfo& source, const TexelCopyTextureInfo& destination, const Extent3D& copy_size) = 0;

    [[nodiscard]] virtual Result<void> CopyTextureToBuffer(const TexelCopyTextureInfo& source, const TexelCopyBufferInfo& destination, const Extent3D& copy_size) = 0;

    [[nodiscard]] virtual Result<void> CopyTextureToTexture(const TexelCopyTextureInfo& source, const TexelCopyTextureInfo& destination, const Extent3D& copy_size) = 0;

    [[nodiscard]] virtual Result<scope<CommandBuffer>> Finish(const CommandBufferDesc& desc = {}) = 0;

    virtual void InjectValidationError(std::string_view message) = 0;
    virtual void InsertDebugMarker(std::string_view marker_label) = 0;
    virtual void PopDebugGroup() = 0;
    virtual void PushDebugGroup(std::string_view group_label) = 0;

    [[nodiscard]] virtual Result<void> ResolveQuerySet(const QuerySet& query_set, u32 first_query, u32 query_count, const Buffer& destination, u64 destination_offset) = 0;

    virtual void SetLabel(std::string_view label) = 0;

    [[nodiscard]] virtual Result<void> WriteBuffer(const Buffer& buffer, u64 buffer_offset, const u8* data, u64 size) = 0;

    [[nodiscard]] virtual Result<void> WriteTimestamp(const QuerySet& query_set, u32 query_index) = 0;

    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    CommandEncoder() = default;
};

} // namespace woki::rhi
