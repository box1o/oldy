#include <woki/rhi/frame.hpp>
#include <woki/rhi/device.hpp>
#include <woki/rhi/swapchain.hpp>

namespace woki::rhi {

Frame Swapchain::MakeFrame(ref<TextureView> color_view, ref<TextureView> depth_view, const u32 width, const u32 height) {
    Frame frame{};
    frame.color_view_ = std::move(color_view);
    frame.depth_view_ = std::move(depth_view);
    frame.width_ = width;
    frame.height_ = height;
    return frame;
}

TextureView& Frame::ColorView() {
    WOKI_ASSERT(color_view_ != nullptr);
    return *color_view_;
}

const TextureView& Frame::ColorView() const {
    WOKI_ASSERT(color_view_ != nullptr);
    return *color_view_;
}

const ref<TextureView>& Frame::ColorViewRef() const noexcept {
    return color_view_;
}

TextureView* Frame::DepthView() noexcept {
    return depth_view_.get();
}

const TextureView* Frame::DepthView() const noexcept {
    return depth_view_.get();
}

Swapchain::Builder::Builder(ref<Device> device, ref<Surface> surface)
    : device_(std::move(device)),
      surface_(std::move(surface)) {}

Swapchain::Builder& Swapchain::Builder::Size(const u32 width, const u32 height) {
    desc_.width = width;
    desc_.height = height;
    return *this;
}

Swapchain::Builder& Swapchain::Builder::ColorFormat(const TextureFormat format) {
    desc_.format = format;
    return *this;
}

Swapchain::Builder& Swapchain::Builder::DepthFormat(const TextureFormat format) {
    desc_.depth_format = format;
    return *this;
}

Swapchain::Builder& Swapchain::Builder::PresentMode(const enum PresentMode mode) {
    desc_.present_mode = mode;
    return *this;
}

Swapchain::Builder& Swapchain::Builder::AlphaMode(const CompositeAlphaMode alpha_mode) {
    desc_.alpha_mode = alpha_mode;
    return *this;
}

Swapchain::Builder& Swapchain::Builder::EnableDepth(const bool enabled) {
    desc_.enable_depth = enabled;
    return *this;
}

Swapchain::Builder& Swapchain::Builder::Label(const std::string_view label) {
    desc_.label = std::string(label);
    return *this;
}

Result<scope<Swapchain>> Swapchain::Builder::Build() {
    if (device_ == nullptr || surface_ == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Swapchain builder requires device and surface");
    }

    return device_->CreateSwapchain(surface_, desc_);
}

} // namespace woki::rhi
