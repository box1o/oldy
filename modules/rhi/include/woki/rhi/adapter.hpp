#pragma once

#include <woki/core.hpp>

#include "types.hpp"
#include "descriptors.hpp"

namespace woki::rhi {

class Device;
class Instance;

class Adapter {
public:
    virtual ~Adapter() = default;

    [[nodiscard]] virtual Result<ref<Device>> CreateDevice(const DeviceDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<ref<Device>> RequestDevice(const DeviceDesc& desc = {}) = 0;
    [[nodiscard]] virtual Future RequestDevice(const DeviceDesc& desc, CallbackMode callback_mode, RequestDeviceCallback callback) = 0;

    [[nodiscard]] virtual AdapterInfo GetInfo() const = 0;
    [[nodiscard]] virtual Result<void> GetInfo(AdapterInfo& info) const = 0;

    virtual void GetFeatures(SupportedFeatures& features) const = 0;
    [[nodiscard]] virtual SupportedFeatures GetFeatures() const = 0;

    [[nodiscard]] virtual Result<void> GetFormatCapabilities(TextureFormat format, DawnFormatCapabilities& capabilities) const = 0;

    [[nodiscard]] virtual Limits GetLimits() const = 0;
    [[nodiscard]] virtual Result<void> GetLimits(Limits& limits) const = 0;

    [[nodiscard]] virtual bool HasFeature(FeatureName feature) const noexcept = 0;
    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    Adapter() = default;
};

} // namespace woki::rhi
