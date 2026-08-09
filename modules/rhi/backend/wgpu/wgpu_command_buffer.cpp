#include "detail/string.hpp"
#include "wgpu_command_buffer.hpp"

namespace woki::rhi::wgpu {

WgpuCommandBufferImpl::WgpuCommandBufferImpl(const WGPUCommandBuffer command_buffer) noexcept
    : command_buffer_(command_buffer) {}

void WgpuCommandBufferImpl::SetLabel(const std::string_view label) {
    if (command_buffer_) {
        wgpuCommandBufferSetLabel(command_buffer_.get(), detail::ToStringView(label));
    }
}

NativeHandles WgpuCommandBufferImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.resource = command_buffer_.get();
    return handles;
}

WGPUCommandBuffer WgpuCommandBufferImpl::GetNativeCommandBuffer() const noexcept {
    return command_buffer_.get();
}

} // namespace woki::rhi::wgpu
