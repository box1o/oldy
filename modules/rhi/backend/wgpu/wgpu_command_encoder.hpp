#pragma once

#include <woki/rhi/command_encoder.hpp>

#include "detail/handle.hpp"

namespace woki::rhi::wgpu {

class WgpuCommandEncoderImpl final : public CommandEncoder {
public:
    explicit WgpuCommandEncoderImpl(WGPUCommandEncoder encoder) noexcept;
    ~WgpuCommandEncoderImpl() override = default;

    [[nodiscard]] Result<scope<ComputePassEncoder>> BeginComputePass(const ComputePassDesc& desc) override;
    [[nodiscard]] Result<scope<RenderPassEncoder>> BeginRenderPass(const RenderPassDesc& desc) override;
    [[nodiscard]] Result<scope<RenderPassEncoder>> BeginRenderPass(const RenderPassDescTyped& desc) override;

    [[nodiscard]] Result<void> ClearBuffer(const Buffer& buffer, u64 offset, u64 size) override;

    [[nodiscard]] Result<void> CopyBufferToBuffer(const Buffer& source, u64 source_offset, const Buffer& destination, u64 destination_offset, u64 size) override;

    [[nodiscard]] Result<void> CopyBufferToTexture(const TexelCopyBufferInfo& source, const TexelCopyTextureInfo& destination, const Extent3D& copy_size) override;

    [[nodiscard]] Result<void> CopyTextureToBuffer(const TexelCopyTextureInfo& source, const TexelCopyBufferInfo& destination, const Extent3D& copy_size) override;

    [[nodiscard]] Result<void> CopyTextureToTexture(const TexelCopyTextureInfo& source, const TexelCopyTextureInfo& destination, const Extent3D& copy_size) override;

    [[nodiscard]] Result<scope<CommandBuffer>> Finish(const CommandBufferDesc& desc) override;

    void InjectValidationError(std::string_view message) override;
    void InsertDebugMarker(std::string_view marker_label) override;
    void PopDebugGroup() override;
    void PushDebugGroup(std::string_view group_label) override;

    [[nodiscard]] Result<void> ResolveQuerySet(const QuerySet& query_set, u32 first_query, u32 query_count, const Buffer& destination, u64 destination_offset) override;

    void SetLabel(std::string_view label) override;

    [[nodiscard]] Result<void> WriteBuffer(const Buffer& buffer, u64 buffer_offset, const u8* data, u64 size) override;

    [[nodiscard]] Result<void> WriteTimestamp(const QuerySet& query_set, u32 query_index) override;

    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

private:
    detail::CommandEncoderHandle encoder_;
};

} // namespace woki::rhi::wgpu
