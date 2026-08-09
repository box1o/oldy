#include <GLFW/glfw3.h>

#include <woki/rhi/device.hpp>
#include <woki/window/window.hpp>

#include "wgpu_enums.hpp"
#include "wgpu_adapter.hpp"
#include "wgpu_surface.hpp"
#include "detail/string.hpp"

#ifdef __EMSCRIPTEN__
#define GLFW_EXPOSE_NATIVE_EMSCRIPTEN
#ifndef GLFW_PLATFORM_EMSCRIPTEN
#define GLFW_PLATFORM_EMSCRIPTEN 0
#endif
#else
#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__) || defined(__MACH__)
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#include <GLFW/glfw3native.h>
#endif

namespace woki::rhi::wgpu {
namespace {

using convert::FromWgpu;
using convert::ToWgpu;

} // namespace

WgpuSurfaceImpl::WgpuSurfaceImpl(WGPUInstance instance, WGPUSurface surface)
    : instance_(detail::InstanceHandle::Retain(instance)),
      surface_(surface) {
    WOKI_ASSERT(instance_.get() != nullptr);
    WOKI_ASSERT(surface_.get() != nullptr);
}

WgpuSurfaceImpl::~WgpuSurfaceImpl() {
    (void)Unconfigure();
    surface_.reset();
    configured_device_.reset();
}

Result<void> WgpuSurfaceImpl::GetCapabilities(const Adapter& adapter, SurfaceCapabilities& capabilities) const {
    if (!surface_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Surface is invalid");
    }

    const auto* wgpu_adapter = dynamic_cast<const WgpuAdapterImpl*>(&adapter);
    if (wgpu_adapter == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Adapter backend mismatch");
    }

    WGPUSurfaceCapabilities native_capabilities = WGPU_SURFACE_CAPABILITIES_INIT;
    if (wgpuSurfaceGetCapabilities(surface_.get(), wgpu_adapter->GetNativeAdapter(), &native_capabilities) != WGPUStatus_Success) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to query surface capabilities");
    }

    capabilities.usages = detail::TextureUsageFromWgpu(native_capabilities.usages);
    capabilities.formats.clear();
    capabilities.formats.reserve(native_capabilities.formatCount);
    for (size_t i = 0; i < native_capabilities.formatCount; ++i) {
        capabilities.formats.push_back(FromWgpu(native_capabilities.formats[i]));
    }

    capabilities.present_modes.clear();
    capabilities.present_modes.reserve(native_capabilities.presentModeCount);
    for (size_t i = 0; i < native_capabilities.presentModeCount; ++i) {
        capabilities.present_modes.push_back(FromWgpu(native_capabilities.presentModes[i]));
    }

    capabilities.alpha_modes.clear();
    capabilities.alpha_modes.reserve(native_capabilities.alphaModeCount);
    for (size_t i = 0; i < native_capabilities.alphaModeCount; ++i) {
        capabilities.alpha_modes.push_back(FromWgpu(native_capabilities.alphaModes[i]));
    }

    wgpuSurfaceCapabilitiesFreeMembers(native_capabilities);
    return Ok();
}

void WgpuSurfaceImpl::GetCurrentTexture(SurfaceTexture& surface_texture) {
    surface_texture = SurfaceTexture{};
    if (!surface_) {
        surface_texture.status = SurfaceGetCurrentTextureStatus::Error;
        return;
    }

    ReleaseCurrentTexture();

    WGPUSurfaceTexture native_texture = WGPU_SURFACE_TEXTURE_INIT;
    wgpuSurfaceGetCurrentTexture(surface_.get(), &native_texture);
    surface_texture.status = FromWgpu(native_texture.status);
    surface_texture.suboptimal = native_texture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal;

    if (native_texture.texture == nullptr) {
        return;
    }

    current_texture_.reset(native_texture.texture);
    current_view_.reset(wgpuTextureCreateView(current_texture_.get(), nullptr));
    if (!current_view_) {
        current_texture_.reset();
        surface_texture.status = SurfaceGetCurrentTextureStatus::Error;
        return;
    }

    surface_texture.handles.instance = instance_.get();
    surface_texture.handles.surface = surface_.get();
    surface_texture.handles.resource = current_view_.get();
}

NativeHandles WgpuSurfaceImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.instance = instance_.get();
    handles.surface = surface_.get();
    handles.resource = current_view_.get();
    return handles;
}

Result<void> WgpuSurfaceImpl::Configure(const SurfaceConfiguration& config) {
    if (!surface_ || config.device == nullptr) {
        return Err(ErrorCode::GraphicsFramebufferIncomplete, "Surface or device is invalid");
    }

    if (config.width == 0 || config.height == 0) {
        return Err(ErrorCode::GraphicsFramebufferIncomplete, "Surface configure size must be non-zero");
    }

    const auto handles = config.device->GetNativeHandles();
    if (handles.device == nullptr) {
        return Err(ErrorCode::GraphicsFramebufferIncomplete, "Device native handle is invalid");
    }

    std::vector<WGPUTextureFormat> view_formats;
    view_formats.reserve(config.view_formats.size());
    for (const TextureFormat format : config.view_formats) {
        view_formats.push_back(ToWgpu(format));
    }

    WGPUSurfaceConfiguration native_config = WGPU_SURFACE_CONFIGURATION_INIT;
    native_config.device = static_cast<WGPUDevice>(handles.device);
    native_config.format = ToWgpu(config.format);
    native_config.usage = detail::TextureUsageToWgpu(config.usage);
    native_config.width = config.width;
    native_config.height = config.height;
    native_config.viewFormatCount = view_formats.size();
    native_config.viewFormats = view_formats.empty() ? nullptr : view_formats.data();
    native_config.alphaMode = ToWgpu(config.alpha_mode);
    native_config.presentMode = ToWgpu(config.present_mode);

    auto* native_device = static_cast<WGPUDevice>(handles.device);
    auto configured_device = detail::DeviceHandle::Retain(native_device);
    wgpuSurfaceConfigure(surface_.get(), &native_config);

    configured_device_ = std::move(configured_device);
    configured_ = true;
    return Ok();
}

Result<void> WgpuSurfaceImpl::Unconfigure() {
    if (!surface_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Surface is invalid");
    }

    if (!configured_) {
        return Ok();
    }

    ReleaseCurrentTexture();
    wgpuSurfaceUnconfigure(surface_.get());
    configured_ = false;
    return Ok();
}

Result<void> WgpuSurfaceImpl::Present() {
    if (!surface_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Surface is invalid");
    }

#ifdef __EMSCRIPTEN__
    ReleaseCurrentTexture();
    return Ok();
#else
    const auto status = wgpuSurfacePresent(surface_.get());
    ReleaseCurrentTexture();
    if (status != WGPUStatus_Success) {
        return Err(ErrorCode::GraphicsFramebufferIncomplete, "Surface present failed");
    }
    return Ok();
#endif
}

void WgpuSurfaceImpl::SetLabel(const std::string_view label) {
    if (surface_) {
        wgpuSurfaceSetLabel(surface_.get(), detail::ToStringView(label));
    }
}

WGPUSurface WgpuSurfaceImpl::GetNativeSurface() const noexcept {
    return surface_.get();
}

WGPUTextureView WgpuSurfaceImpl::TakeCurrentTextureView() noexcept {
    return current_view_.release();
}

void WgpuSurfaceImpl::ReleaseCurrentTexture() noexcept {
    current_view_.reset();
    current_texture_.reset();
}

WGPUSurface CreateNativeSurface(WGPUInstance instance, Window& window) {
    if (instance == nullptr) {
        return nullptr;
    }

    WOKI_ASSERT(window.GetNativeHandle() != nullptr);

#ifdef __EMSCRIPTEN__
    WGPUSurfaceDescriptor descriptor = WGPU_SURFACE_DESCRIPTOR_INIT;
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas = WGPU_EMSCRIPTEN_SURFACE_SOURCE_CANVAS_HTML_SELECTOR_INIT;
    canvas.selector = detail::ToStringView("canvas");
    descriptor.nextInChain = &canvas.chain;
    return wgpuInstanceCreateSurface(instance, &descriptor);
#else
    auto* glfw_window = static_cast<GLFWwindow*>(window.GetNativeHandle());
    if (glfw_window == nullptr) {
        return nullptr;
    }

    const int platform = glfwGetPlatform();

#if defined(GLFW_EXPOSE_NATIVE_COCOA)
    if (platform == GLFW_PLATFORM_COCOA) {
        return CreateNativeSurfaceCocoa(instance, window);
    }
#endif

#if defined(GLFW_EXPOSE_NATIVE_X11) || defined(GLFW_EXPOSE_NATIVE_WAYLAND) || defined(GLFW_EXPOSE_NATIVE_WIN32)
    WGPUSurfaceDescriptor descriptor = WGPU_SURFACE_DESCRIPTOR_INIT;

#if defined(GLFW_EXPOSE_NATIVE_X11)
    if (platform == GLFW_PLATFORM_X11) {
        WGPUSurfaceSourceXlibWindow source = WGPU_SURFACE_SOURCE_XLIB_WINDOW_INIT;
        source.display = glfwGetX11Display();
        source.window = static_cast<uint64_t>(glfwGetX11Window(glfw_window));
        if (source.display != nullptr && source.window != 0) {
            descriptor.nextInChain = &source.chain;
            return wgpuInstanceCreateSurface(instance, &descriptor);
        }
    }
#endif

#if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
    if (platform == GLFW_PLATFORM_WAYLAND) {
        WGPUSurfaceSourceWaylandSurface source = WGPU_SURFACE_SOURCE_WAYLAND_SURFACE_INIT;
        source.display = glfwGetWaylandDisplay();
        source.surface = glfwGetWaylandWindow(glfw_window);
        if (source.display != nullptr && source.surface != nullptr) {
            descriptor.nextInChain = &source.chain;
            return wgpuInstanceCreateSurface(instance, &descriptor);
        }
    }
#endif

#if defined(GLFW_EXPOSE_NATIVE_WIN32)
    if (platform == GLFW_PLATFORM_WIN32) {
        WGPUSurfaceSourceWindowsHWND source = WGPU_SURFACE_SOURCE_WINDOWS_HWND_INIT;
        source.hwnd = glfwGetWin32Window(glfw_window);
        source.hinstance = GetModuleHandle(nullptr);
        if (source.hwnd != nullptr && source.hinstance != nullptr) {
            descriptor.nextInChain = &source.chain;
            return wgpuInstanceCreateSurface(instance, &descriptor);
        }
    }
#endif
#endif

    return nullptr;
#endif
}

} // namespace woki::rhi::wgpu
