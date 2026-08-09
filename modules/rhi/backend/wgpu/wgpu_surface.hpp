#pragma once

#include <woki/rhi/surface.hpp>

#include "detail/handle.hpp"

namespace woki {
class Window;
}

namespace woki::rhi::wgpu {

class WgpuAdapterImpl;

class WgpuSurfaceImpl final : public Surface {
public:
    WgpuSurfaceImpl(WGPUInstance instance, WGPUSurface surface);
    ~WgpuSurfaceImpl() override;

    [[nodiscard]] Result<void> GetCapabilities(const Adapter& adapter, SurfaceCapabilities& capabilities) const override;
    void GetCurrentTexture(SurfaceTexture& surface_texture) override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;
    [[nodiscard]] Result<void> Configure(const SurfaceConfiguration& config) override;
    [[nodiscard]] Result<void> Unconfigure() override;
    [[nodiscard]] Result<void> Present() override;
    void SetLabel(std::string_view label) override;

    [[nodiscard]] WGPUSurface GetNativeSurface() const noexcept;
    [[nodiscard]] WGPUTextureView TakeCurrentTextureView() noexcept;

private:
    void ReleaseCurrentTexture() noexcept;

    detail::InstanceHandle instance_;
    detail::SurfaceHandle surface_;
    detail::DeviceHandle configured_device_;
    detail::TextureHandle current_texture_;
    detail::TextureViewHandle current_view_;
    bool configured_{false};
    bool acquired_{false};
};

[[nodiscard]] WGPUSurface CreateNativeSurface(WGPUInstance instance, Window& window);

#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
[[nodiscard]] WGPUSurface CreateNativeSurfaceCocoa(WGPUInstance instance, Window& window);
#endif

} // namespace woki::rhi::wgpu
