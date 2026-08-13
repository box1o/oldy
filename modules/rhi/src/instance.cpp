#include <woki/rhi/instance.hpp>

#include "backend.hpp"

namespace woki::rhi {

const DeviceCapabilities& Device::Capabilities() const noexcept {
    static const DeviceCapabilities unsupported{};
    return unsupported;
}

Result<scope<Instance>> Instance::Create(InstanceDesc desc) {
    return backend::CreateInstance(std::move(desc));
}

SupportedInstanceFeatures Instance::GetInstanceFeatures() {
    SupportedInstanceFeatures features{};
    GetInstanceFeatures(features);
    return features;
}

void Instance::GetInstanceFeatures(SupportedInstanceFeatures& features) {
    backend::GetInstanceFeatures(features);
}

Result<InstanceLimits> Instance::GetInstanceLimits() {
    return backend::GetInstanceLimits();
}

bool Instance::HasInstanceFeature(const InstanceFeatureName feature) noexcept {
    return backend::HasInstanceFeature(feature);
}

Proc Instance::GetProcAddress(const std::string_view proc_name) noexcept {
    return backend::GetProcAddress(proc_name);
}

} // namespace woki::rhi
