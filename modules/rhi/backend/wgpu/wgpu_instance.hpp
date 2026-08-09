#pragma once

#include <woki/rhi/instance.hpp>

#include "detail/handle.hpp"

namespace woki::rhi::wgpu {

class WgpuSurfaceImpl;
class WgpuAdapterImpl;

class WgpuInstanceImpl final : public Instance {
public:
    explicit WgpuInstanceImpl(InstanceDesc desc);
    ~WgpuInstanceImpl() override;

    [[nodiscard]] bool IsValid() const noexcept override;
    [[nodiscard]] const InstanceDesc& GetDesc() const noexcept override;

    [[nodiscard]] Result<ref<Surface>> CreateSurface(const SurfaceDescriptor& desc) override;
    [[nodiscard]] Result<ref<Surface>> CreateSurface(Window& window, SurfaceDesc desc = {}) override;

    void GetWGSLLanguageFeatures(SupportedWGSLLanguageFeatures& features) const override;
    [[nodiscard]] bool HasWGSLLanguageFeature(WGSLLanguageFeatureName feature) const noexcept override;

    void ProcessEvents() const noexcept override;

    [[nodiscard]] WaitStatus WaitAny(Future& future, u64 timeout_ns) override;
    [[nodiscard]] WaitStatus WaitAny(FutureWaitInfo& wait_info, u64 timeout_ns) override;
    [[nodiscard]] WaitStatus WaitAny(std::span<FutureWaitInfo> wait_infos, u64 timeout_ns) override;

    [[nodiscard]] Result<scope<Adapter>> RequestAdapter(RequestAdapterDesc desc = {}) override;
    [[nodiscard]] Future RequestAdapter(RequestAdapterDesc desc, CallbackMode callback_mode, RequestAdapterCallback callback) override;

    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

    [[nodiscard]] WGPUInstance GetNativeInstance() const noexcept;

private:
    [[nodiscard]] bool Initialize() noexcept;
    [[nodiscard]] WGPURequestAdapterOptions BuildRequestAdapterOptions(const RequestAdapterDesc& desc) const;
    [[nodiscard]] WGPUWaitStatus WaitAnyNative(size_t future_count, WGPUFutureWaitInfo* futures, u64 timeout_ns) const noexcept;

    InstanceDesc desc_;
    detail::InstanceHandle handle_;
};

} // namespace woki::rhi::wgpu
