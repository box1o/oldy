#pragma once

#include <woki/rhi/adapter.hpp>

#include "detail/handle.hpp"

namespace woki::rhi::wgpu {

class WgpuAdapterImpl final : public Adapter {
public:
    WgpuAdapterImpl(WGPUInstance instance, WGPUAdapter adapter);
    ~WgpuAdapterImpl() override;

    [[nodiscard]] Result<ref<Device>> CreateDevice(const DeviceDesc& desc = {}) override;
    [[nodiscard]] Result<ref<Device>> RequestDevice(const DeviceDesc& desc = {}) override;
    [[nodiscard]] Future RequestDevice(const DeviceDesc& desc, CallbackMode callback_mode, RequestDeviceCallback callback) override;

    [[nodiscard]] AdapterInfo GetInfo() const override;
    [[nodiscard]] Result<void> GetInfo(AdapterInfo& info) const override;

    void GetFeatures(SupportedFeatures& features) const override;
    [[nodiscard]] SupportedFeatures GetFeatures() const override;

    [[nodiscard]] Result<void> GetFormatCapabilities(TextureFormat format, DawnFormatCapabilities& capabilities) const override;

    [[nodiscard]] Limits GetLimits() const override;
    [[nodiscard]] Result<void> GetLimits(Limits& limits) const override;

    [[nodiscard]] bool HasFeature(FeatureName feature) const noexcept override;
    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;

    [[nodiscard]] WGPUAdapter GetNativeAdapter() const noexcept;
    [[nodiscard]] WGPUInstance GetNativeInstance() const noexcept;

private:
    detail::InstanceHandle instance_handle_;
    detail::AdapterHandle adapter_;
    AdapterInfo info_;
    SupportedFeatures features_;
    Limits limits_;
};

} // namespace woki::rhi::wgpu
