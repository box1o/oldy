#pragma once

#include <span>
#include <string_view>

#include <woki/core.hpp>

#include "types.hpp"
#include "descriptors.hpp"

namespace woki::rhi {

class Buffer;
class CommandBuffer;

class Queue {
public:
    virtual ~Queue() = default;

    [[nodiscard]] virtual Result<void> CopyExternalTextureForBrowser(const ImageCopyExternalTexture& source,
        const TexelCopyTextureInfo& destination,
        const Extent3D& copy_size,
        const CopyTextureForBrowserOptions& options) const = 0;

    [[nodiscard]] virtual Result<void> CopyTextureForBrowser(const TexelCopyTextureInfo& source, const TexelCopyTextureInfo& destination, const Extent3D& copy_size, const CopyTextureForBrowserOptions& options) const = 0;

    [[nodiscard]] virtual Future OnSubmittedWorkDone(CallbackMode callback_mode, QueueWorkDoneCallback callback) const = 0;

    virtual void SetLabel(std::string_view label) const = 0;

    [[nodiscard]] virtual Result<void> Submit(std::span<CommandBuffer* const> commands) const = 0;

    [[nodiscard]] virtual Result<void> WriteBuffer(const Buffer& buffer, u64 buffer_offset, const void* data, u64 size) const = 0;

    [[nodiscard]] virtual Result<void> WriteTexture(const TexelCopyTextureInfo& destination, const void* data, u64 data_size, const TexelCopyBufferLayout& data_layout, const Extent3D& write_size) const = 0;

    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    Queue() = default;
};

} // namespace woki::rhi
