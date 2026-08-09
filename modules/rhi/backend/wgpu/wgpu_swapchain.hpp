#pragma once

#include <woki/rhi/swapchain.hpp>

#include "detail/handle.hpp"

namespace woki::rhi::wgpu {

class WgpuDeviceImpl;
class WgpuSurfaceImpl;

class WgpuSwapchainImpl final : public Swapchain {
public:
    WgpuSwapchainImpl(ref<WgpuDeviceImpl> device, ref<WgpuSurfaceImpl> surface, SwapchainDesc desc);
    ~WgpuSwapchainImpl() override;

    [[nodiscard]] TextureFormat ColorFormat() const noexcept override;
    [[nodiscard]] TextureFormat DepthFormat() const noexcept override;
    [[nodiscard]] u32 Width() const noexcept override;
    [[nodiscard]] u32 Height() const noexcept override;

    void Resize(u32 width, u32 height) override;
    [[nodiscard]] Result<Frame> AcquireNextFrame() override;
    [[nodiscard]] Result<void> Present() override;

private:
    [[nodiscard]] Result<void> Configure();
    [[nodiscard]] Result<void> CreateOrResizeDepth();
    void ReleaseCurrentTexture() noexcept;
    void ReleaseDepthResources() noexcept;

    ref<WgpuDeviceImpl> device_{};
    ref<WgpuSurfaceImpl> surface_{};
    SwapchainDesc desc_{};
    u32 width_{0};
    u32 height_{0};
    detail::TextureHandle current_texture_;
    detail::TextureHandle depth_texture_;
    scope<TextureView> depth_view_;
    bool configured_{false};
};

[[nodiscard]] Result<scope<Swapchain>> CreateSwapchainObject(ref<WgpuDeviceImpl> device, ref<Surface> surface, SwapchainDesc desc);

} // namespace woki::rhi::wgpu
