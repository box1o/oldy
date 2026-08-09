#include <woki/rhi/objects.hpp>
#include <woki/rhi/command_buffer.hpp>

#include "wgpu_enums.hpp"
#include "wgpu_queue.hpp"
#include "detail/string.hpp"
#include "detail/copy_convert.hpp"

namespace woki::rhi::wgpu {
namespace {

using convert::FromWgpu;
using convert::ToWgpu;

struct QueueWorkDoneCallbackState {
    QueueWorkDoneCallback callback;
};

void QueueWorkDoneThunk(WGPUQueueWorkDoneStatus status, WGPUStringView message, void* userdata1, void*) {
    auto state = scope<QueueWorkDoneCallbackState>(static_cast<QueueWorkDoneCallbackState*>(userdata1));
    if (state == nullptr || !state->callback) {
        return;
    }

    state->callback(FromWgpu(status), detail::StringFromView(message));
}

} // namespace

WgpuQueueImpl::WgpuQueueImpl(WGPUQueue queue) noexcept
    : queue_(queue) {}

Result<void> WgpuQueueImpl::CopyExternalTextureForBrowser(const ImageCopyExternalTexture& source, const TexelCopyTextureInfo& destination, const Extent3D& copy_size, const CopyTextureForBrowserOptions& options) const {
    if (!queue_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Queue is invalid");
    }

    const auto native_source = detail::copy::ToWgpu(source);
    const auto native_destination = detail::copy::ToWgpu(destination);
    const auto native_size = detail::copy::ToWgpu(copy_size);
    const auto native_options = detail::copy::ToWgpu(options);

    wgpuQueueCopyExternalTextureForBrowser(queue_.get(), &native_source, &native_destination, &native_size, &native_options);
    return Ok();
}

Result<void> WgpuQueueImpl::CopyTextureForBrowser(const TexelCopyTextureInfo& source, const TexelCopyTextureInfo& destination, const Extent3D& copy_size, const CopyTextureForBrowserOptions& options) const {
    if (!queue_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Queue is invalid");
    }

    const auto native_source = detail::copy::ToWgpu(source);
    const auto native_destination = detail::copy::ToWgpu(destination);
    const auto native_size = detail::copy::ToWgpu(copy_size);
    const auto native_options = detail::copy::ToWgpu(options);

    wgpuQueueCopyTextureForBrowser(queue_.get(), &native_source, &native_destination, &native_size, &native_options);
    return Ok();
}

Future WgpuQueueImpl::OnSubmittedWorkDone(CallbackMode callback_mode, QueueWorkDoneCallback callback) const {
    Future future{};
    if (!queue_) {
        future.message = "Queue is invalid";
        return future;
    }

    if (!callback) {
        future.message = "OnSubmittedWorkDone requires a callback";
        return future;
    }

    auto callback_state = createScope<QueueWorkDoneCallbackState>(QueueWorkDoneCallbackState{.callback = std::move(callback)});

    WGPUQueueWorkDoneCallbackInfo callback_info = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    callback_info.mode = ToWgpu(callback_mode);
    callback_info.callback = QueueWorkDoneThunk;
    callback_info.userdata1 = callback_state.get();

    auto* transferred_state = callback_state.release();
    const WGPUFuture native_future = wgpuQueueOnSubmittedWorkDone(queue_.get(), callback_info);
    future.id = native_future.id;
    if (future.id == 0) {
        callback_state.reset(transferred_state);
        future.message = "Queue completion request failed to start";
    }
    return future;
}

void WgpuQueueImpl::SetLabel(const std::string_view label) const {
    if (queue_) {
        wgpuQueueSetLabel(queue_.get(), detail::ToStringView(label));
    }
}

Result<void> WgpuQueueImpl::Submit(std::span<CommandBuffer* const> commands) const {
    if (!queue_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Queue is invalid");
    }

    if (commands.empty()) {
        wgpuQueueSubmit(queue_.get(), 0, nullptr);
        return Ok();
    }

    std::vector<WGPUCommandBuffer> native_commands{};
    native_commands.reserve(commands.size());
    for (CommandBuffer* const command : commands) {
        if (command == nullptr) {
            return Err(ErrorCode::GraphicsResourceCreationFailed, "Submit received a null command buffer");
        }

        const auto handles = command->GetNativeHandles();
        native_commands.push_back(static_cast<WGPUCommandBuffer>(handles.resource));
    }

    wgpuQueueSubmit(queue_.get(), native_commands.size(), native_commands.data());
    return Ok();
}

Result<void> WgpuQueueImpl::WriteBuffer(const Buffer& buffer, const u64 buffer_offset, const void* data, const u64 size) const {
    if (!queue_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Queue is invalid");
    }

    if (data == nullptr && size != 0) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "WriteBuffer data is null");
    }

    const auto handles = buffer.GetNativeHandles();
    const auto native_buffer = static_cast<WGPUBuffer>(handles.resource);
    if (native_buffer == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Buffer is invalid");
    }

    wgpuQueueWriteBuffer(queue_.get(), native_buffer, buffer_offset, data, size);
    return Ok();
}

Result<void> WgpuQueueImpl::WriteTexture(const TexelCopyTextureInfo& destination, const void* data, const u64 data_size, const TexelCopyBufferLayout& data_layout, const Extent3D& write_size) const {
    if (!queue_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Queue is invalid");
    }

    if (data == nullptr && data_size != 0) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "WriteTexture data is null");
    }

    const auto native_destination = detail::copy::ToWgpu(destination);
    const auto native_layout = detail::copy::ToWgpu(data_layout);
    const auto native_size = detail::copy::ToWgpu(write_size);

    wgpuQueueWriteTexture(queue_.get(), &native_destination, data, data_size, &native_layout, &native_size);
    return Ok();
}

NativeHandles WgpuQueueImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.queue = queue_.get();
    return handles;
}

WGPUQueue WgpuQueueImpl::GetNativeQueue() const noexcept {
    return queue_.get();
}

} // namespace woki::rhi::wgpu
