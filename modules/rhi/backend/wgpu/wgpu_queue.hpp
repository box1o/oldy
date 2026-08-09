#pragma once

#include <woki/rhi/queue.hpp>

#include "detail/handle.hpp"

namespace woki::rhi::wgpu {

class WgpuDeviceImpl;

class WgpuQueueImpl final : public Queue {
public:
    explicit WgpuQueueImpl(WGPUQueue queue) noexcept;
    ~WgpuQueueImpl() override = default;

    [[nodiscard]] Result<void> CopyExternalTextureForBrowser(const ImageCopyExternalTexture& source,
        const TexelCopyTextureInfo& destination,
        const Extent3D& copy_size,
        const CopyTextureForBrowserOptions& options) const override;

    [[nodiscard]] Result<void> CopyTextureForBrowser(const TexelCopyTextureInfo& source, const TexelCopyTextureInfo& destination, const Extent3D& copy_size, const CopyTextureForBrowserOptions& options) const override;

    [[nodiscard]] Future OnSubmittedWorkDone(CallbackMode callback_mode, QueueWorkDoneCallback callback) const override;

    void SetLabel(std::string_view label) const override;

    [[nodiscard]] Result<void> Submit(std::span<CommandBuffer* const> commands) const override;

    [[nodiscard]] Result<void> WriteBuffer(const Buffer& buffer, u64 buffer_offset, const void* data, u64 size) const override;

    [[nodiscard]] Result<void> WriteTexture(const TexelCopyTextureInfo& destination, const void* data, u64 data_size, const TexelCopyBufferLayout& data_layout, const Extent3D& write_size) const override;

    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

    [[nodiscard]] WGPUQueue GetNativeQueue() const noexcept;

private:
    detail::QueueHandle queue_;
};

} // namespace woki::rhi::wgpu
