#include "wgpu_enums.hpp"
#include "wgpu_device.hpp"
#include "wgpu_objects.hpp"
#include "wgpu_surface.hpp"
#include "detail/string.hpp"
#include "wgpu_swapchain.hpp"

namespace woki::rhi::wgpu {
namespace {

using convert::ToWgpu;

} // namespace

WgpuSwapchainImpl::WgpuSwapchainImpl(ref<WgpuDeviceImpl> device, ref<WgpuSurfaceImpl> surface, SwapchainDesc desc)
    : device_(std::move(device)),
      surface_(std::move(surface)),
      desc_(std::move(desc)),
      width_(desc_.width),
      height_(desc_.height) {
    if (auto result = Configure(); !result) {
        slog::Error("Failed to configure swapchain '{}': {}", desc_.label, result.error().Message());
        return;
    }
    if (auto result = CreateOrResizeDepth(); !result) {
        slog::Error("Failed to create swapchain depth '{}': {}", desc_.label, result.error().Message());
    }
}

WgpuSwapchainImpl::~WgpuSwapchainImpl() {
    ReleaseCurrentTexture();
    ReleaseDepthResources();
    if (surface_ && configured_) {
        (void)surface_->Unconfigure();
    }
    surface_.reset();
    device_.reset();
}

TextureFormat WgpuSwapchainImpl::ColorFormat() const noexcept {
    return desc_.format;
}

TextureFormat WgpuSwapchainImpl::DepthFormat() const noexcept {
    return desc_.depth_format;
}

u32 WgpuSwapchainImpl::Width() const noexcept {
    return width_;
}

u32 WgpuSwapchainImpl::Height() const noexcept {
    return height_;
}

void WgpuSwapchainImpl::Resize(const u32 width, const u32 height) {
    if (width == 0 || height == 0 || (width == width_ && height == height_)) {
        return;
    }

    ReleaseCurrentTexture();

    width_ = width;
    height_ = height;
    desc_.width = width;
    desc_.height = height;

    if (auto result = Configure(); !result) {
        slog::Error("Failed to reconfigure swapchain '{}': {}", desc_.label, result.error().Message());
        return;
    }
    if (auto result = CreateOrResizeDepth(); !result) {
        slog::Error("Failed to resize swapchain depth '{}': {}", desc_.label, result.error().Message());
    }
}

Result<Frame> WgpuSwapchainImpl::AcquireNextFrame() {
    if (!surface_ || !device_ || !configured_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Swapchain is invalid");
    }

    if (acquired_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "A swapchain frame is already acquired");
    }

    WGPUSurfaceTexture surface_texture = WGPU_SURFACE_TEXTURE_INIT;
    wgpuSurfaceGetCurrentTexture(surface_->GetNativeSurface(), &surface_texture);

    if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal && surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        if (surface_texture.texture != nullptr) {
            wgpuTextureRelease(surface_texture.texture);
        }
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to acquire surface texture");
    }

    current_texture_.reset(surface_texture.texture);
    detail::TextureViewHandle current_view(wgpuTextureCreateView(current_texture_.get(), nullptr));
    if (!current_view) {
        ReleaseCurrentTexture();
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Surface texture view is null");
    }

    auto color_view = CreateTextureViewObject(current_view.release());
    if (!color_view) {
        ReleaseCurrentTexture();
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to wrap surface texture view");
    }

    ref<TextureView> frame_depth_view{};
    if (desc_.enable_depth && depth_view_) {
        const auto handles = depth_view_->GetNativeHandles();
        auto retained_view = detail::TextureViewHandle::Retain(static_cast<WGPUTextureView>(handles.resource));
        frame_depth_view = CreateTextureViewObject(retained_view.release());
    }

    if (desc_.enable_depth && !frame_depth_view) {
        ReleaseCurrentTexture();
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Swapchain depth view is unavailable");
    }

    acquired_ = true;
    return Ok(MakeFrame(ref<TextureView>(std::move(color_view)), std::move(frame_depth_view), width_, height_));
}

void WgpuSwapchainImpl::Discard() noexcept {
    ReleaseCurrentTexture();
}

Result<void> WgpuSwapchainImpl::Present() {
    if (!surface_ || !configured_ || !acquired_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Swapchain surface is invalid");
    }

#ifdef __EMSCRIPTEN__
    ReleaseCurrentTexture();
    return Ok();
#else
    const auto status = wgpuSurfacePresent(surface_->GetNativeSurface());
    ReleaseCurrentTexture();
    if (status != WGPUStatus_Success) {
        return Err(ErrorCode::GraphicsFramebufferIncomplete, "Surface present failed");
    }
    return Ok();
#endif
}

bool WgpuSwapchainImpl::IsConfigured() const noexcept {
    return configured_ && (!desc_.enable_depth || depth_view_ != nullptr);
}

Result<void> WgpuSwapchainImpl::Configure() {
    if (!surface_ || !device_ || width_ == 0 || height_ == 0) {
        return Err(ErrorCode::GraphicsFramebufferIncomplete, "Swapchain configure prerequisites missing");
    }

    configured_ = false;
    ReleaseCurrentTexture();
    SurfaceConfiguration config{};
    config.device = device_.get();
    config.format = desc_.format;
    config.usage = desc_.usage;
    config.width = width_;
    config.height = height_;
    config.alpha_mode = desc_.alpha_mode;
    config.present_mode = desc_.present_mode;
    if (auto result = surface_->Configure(config); !result) {
        return result;
    }
    configured_ = true;
    return Ok();
}

Result<void> WgpuSwapchainImpl::CreateOrResizeDepth() {
    depth_view_.reset();
    depth_texture_.reset();

    if (!desc_.enable_depth || !device_ || width_ == 0 || height_ == 0) {
        return Ok();
    }

    const WGPUDevice native_device = device_->GetNativeDevice();
    if (native_device == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Device is invalid");
    }

    WGPUTextureDescriptor texture_desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
    texture_desc.dimension = WGPUTextureDimension_2D;
    texture_desc.size = WGPUExtent3D{width_, height_, 1};
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = ToWgpu(desc_.depth_format);
    texture_desc.usage = WGPUTextureUsage_RenderAttachment;
    const std::string depth_label = desc_.label + "Depth";
    texture_desc.label = detail::ToStringView(depth_label);

    WGPUTexture native_texture = wgpuDeviceCreateTexture(native_device, &texture_desc);
    if (native_texture == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to create swapchain depth texture");
    }

    WGPUTextureView native_view = wgpuTextureCreateView(native_texture, nullptr);
    if (native_view == nullptr) {
        wgpuTextureRelease(native_texture);
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to create swapchain depth view");
    }

    depth_texture_.reset(native_texture);
    depth_view_ = CreateTextureViewObject(native_view);
    if (!depth_view_) {
        depth_texture_.reset();
        wgpuTextureViewRelease(native_view);
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to wrap swapchain depth view");
    }

    return Ok();
}

void WgpuSwapchainImpl::ReleaseDepthResources() noexcept {
    depth_view_.reset();
    depth_texture_.reset();
}

void WgpuSwapchainImpl::ReleaseCurrentTexture() noexcept {
    current_texture_.reset();
    acquired_ = false;
}

Result<scope<Swapchain>> CreateSwapchainObject(ref<WgpuDeviceImpl> device, ref<Surface> surface, SwapchainDesc desc) {
    auto wgpu_surface = std::dynamic_pointer_cast<WgpuSurfaceImpl>(std::move(surface));
    if (wgpu_surface == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Surface backend mismatch");
    }

    if (desc.width == 0 || desc.height == 0) {
        return Err(ErrorCode::GraphicsFramebufferIncomplete, "SwapchainDesc width and height must be non-zero (use Swapchain::Builder::Size)");
    }

    auto swapchain = createScope<WgpuSwapchainImpl>(std::move(device), std::move(wgpu_surface), std::move(desc));
    if (!swapchain->IsConfigured()) {
        return Err(ErrorCode::GraphicsFramebufferIncomplete, "Failed to configure swapchain");
    }
    return Ok(std::move(swapchain));
}

} // namespace woki::rhi::wgpu
