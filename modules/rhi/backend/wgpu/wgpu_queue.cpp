#include <atomic>
#include <limits>

#include <woki/rhi/objects.hpp>
#include <woki/rhi/command_buffer.hpp>

#include "wgpu_enums.hpp"
#include "wgpu_queue.hpp"
#include "detail/string.hpp"
#include "detail/copy_convert.hpp"

namespace woki::rhi::wgpu {

struct WgpuSubmissionState final {
    std::atomic<u64> completed{};
    std::atomic<SubmissionTrackingStatus> status{SubmissionTrackingStatus::Healthy};
    u64 next{1};
};

namespace {

using convert::FromWgpu;
using convert::ToWgpu;

struct QueueWorkDoneCallbackState {
    QueueWorkDoneCallback callback;
};

struct SubmissionCallbackState {
    std::shared_ptr<WgpuSubmissionState> state;
    u64 epoch{};
};

void QueueWorkDoneThunk(WGPUQueueWorkDoneStatus status, WGPUStringView message, void* userdata1, void*) {
    auto state = scope<QueueWorkDoneCallbackState>(static_cast<QueueWorkDoneCallbackState*>(userdata1));
    if (state == nullptr || !state->callback) {
        return;
    }

    state->callback(FromWgpu(status), detail::StringFromView(message));
}

void SubmissionDoneThunk(WGPUQueueWorkDoneStatus status, WGPUStringView, void* userdata1, void*) {
    auto callback = scope<SubmissionCallbackState>(static_cast<SubmissionCallbackState*>(userdata1));
    if (callback == nullptr)
        return;
    if (status != WGPUQueueWorkDoneStatus_Success) {
        callback->state->status.store(SubmissionTrackingStatus::CompletionCallbackFailed, std::memory_order_release);
        return;
    }

    // Queue completion is ordered, so completion of this submission proves all
    // lower tickets complete even if their individual callbacks ran later.
    u64 completed = callback->state->completed.load(std::memory_order_relaxed);
    while (completed < callback->epoch && !callback->state->completed.compare_exchange_weak(completed, callback->epoch, std::memory_order_release, std::memory_order_relaxed)) {
    }
}

} // namespace

WgpuQueueImpl::WgpuQueueImpl(WGPUQueue queue) noexcept
    : queue_(queue),
      submission_state_(std::make_shared<WgpuSubmissionState>()) {}

Result<void> WgpuQueueImpl::CopyExternalTextureForBrowser(const ImageCopyExternalTexture& source, const TexelCopyTextureInfo& destination, const Extent3D& copy_size, const CopyTextureForBrowserOptions& options) const {
#ifdef __EMSCRIPTEN__
    (void)source;
    (void)destination;
    (void)copy_size;
    (void)options;
    return Err(ErrorCode::GraphicsUnsupportedApi, "CopyExternalTextureForBrowser is unavailable with emdawnwebgpu");
#else
    if (!queue_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Queue is invalid");
    }

    const auto native_source = detail::copy::ToWgpu(source);
    const auto native_destination = detail::copy::ToWgpu(destination);
    const auto native_size = detail::copy::ToWgpu(copy_size);
    const auto native_options = detail::copy::ToWgpu(options);

    wgpuQueueCopyExternalTextureForBrowser(queue_.get(), &native_source, &native_destination, &native_size, &native_options);
    return Ok();
#endif
}

Result<void> WgpuQueueImpl::CopyTextureForBrowser(const TexelCopyTextureInfo& source, const TexelCopyTextureInfo& destination, const Extent3D& copy_size, const CopyTextureForBrowserOptions& options) const {
#ifdef __EMSCRIPTEN__
    (void)source;
    (void)destination;
    (void)copy_size;
    (void)options;
    return Err(ErrorCode::GraphicsUnsupportedApi, "CopyTextureForBrowser is unavailable with emdawnwebgpu");
#else
    if (!queue_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Queue is invalid");
    }

    const auto native_source = detail::copy::ToWgpu(source);
    const auto native_destination = detail::copy::ToWgpu(destination);
    const auto native_size = detail::copy::ToWgpu(copy_size);
    const auto native_options = detail::copy::ToWgpu(options);

    wgpuQueueCopyTextureForBrowser(queue_.get(), &native_source, &native_destination, &native_size, &native_options);
    return Ok();
#endif
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

Result<SubmissionTicket> WgpuQueueImpl::Submit(std::span<CommandBuffer* const> commands) const {
    if (!queue_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Queue is invalid");
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

    std::lock_guard lock(submission_mutex_);
    if (submission_state_->next == std::numeric_limits<u64>::max())
        return Err(ErrorCode::OutOfRange, "Queue submission epoch exhausted");

    wgpuQueueSubmit(queue_.get(), native_commands.size(), native_commands.empty() ? nullptr : native_commands.data());
    const u64 epoch = submission_state_->next++;

    auto callback_state = createScope<SubmissionCallbackState>(SubmissionCallbackState{submission_state_, epoch});
    WGPUQueueWorkDoneCallbackInfo callback_info = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    callback_info.mode = WGPUCallbackMode_AllowSpontaneous;
    callback_info.callback = SubmissionDoneThunk;
    callback_info.userdata1 = callback_state.release();
    const WGPUFuture future = wgpuQueueOnSubmittedWorkDone(queue_.get(), callback_info);
    if (future.id == 0) {
        callback_state.reset(static_cast<SubmissionCallbackState*>(callback_info.userdata1));
        submission_state_->status.store(SubmissionTrackingStatus::CompletionRegistrationFailed, std::memory_order_release);
    }
    return Ok(SubmissionTicket(epoch));
}

SubmissionEpoch WgpuQueueImpl::CompletedSubmission() const noexcept {
    return SubmissionEpoch(submission_state_->completed.load(std::memory_order_acquire));
}

SubmissionTrackingStatus WgpuQueueImpl::SubmissionTracking() const noexcept {
    return submission_state_->status.load(std::memory_order_acquire);
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

    if (size > std::numeric_limits<size_t>::max()) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "WriteBuffer data is too large for this platform");
    }

    wgpuQueueWriteBuffer(queue_.get(), native_buffer, buffer_offset, data, static_cast<size_t>(size));
    return Ok();
}

Result<void> WgpuQueueImpl::WriteTexture(const TexelCopyTextureInfo& destination, const void* data, const u64 data_size, const TexelCopyBufferLayout& data_layout, const Extent3D& write_size) const {
    if (!queue_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Queue is invalid");
    }

    if (data == nullptr && data_size != 0) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "WriteTexture data is null");
    }

    if (data_size > std::numeric_limits<size_t>::max()) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "WriteTexture data is too large for this platform");
    }

    const auto native_destination = detail::copy::ToWgpu(destination);
    const auto native_layout = detail::copy::ToWgpu(data_layout);
    const auto native_size = detail::copy::ToWgpu(write_size);

    wgpuQueueWriteTexture(queue_.get(), &native_destination, data, static_cast<size_t>(data_size), &native_layout, &native_size);
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
