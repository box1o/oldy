#include <webgpu/webgpu.h>

#include "../backend.hpp"
#include "detail/string.hpp"
#include "wgpu_enums.hpp"
#include "wgpu_instance.hpp"

namespace woki::rhi::backend {

Result<scope<Instance>> CreateInstance(InstanceDesc desc) {
    auto instance = createScope<wgpu::WgpuInstanceImpl>(std::move(desc));
    if (instance == nullptr || !instance->IsValid())
        return Err(ErrorCode::GraphicsInitFailed, "Failed to create WebGPU instance");
    return Ok(std::move(instance));
}

void GetInstanceFeatures(SupportedInstanceFeatures& features) {
    features.features.clear();
    WGPUSupportedInstanceFeatures native_features = WGPU_SUPPORTED_INSTANCE_FEATURES_INIT;
    wgpuGetInstanceFeatures(&native_features);
    features.features.reserve(native_features.featureCount);
    for (size_t i = 0; i < native_features.featureCount; ++i)
        features.features.push_back(wgpu::convert::FromWgpu(native_features.features[i]));
    wgpuSupportedInstanceFeaturesFreeMembers(native_features);
}

Result<InstanceLimits> GetInstanceLimits() {
    WGPUInstanceLimits native_limits = WGPU_INSTANCE_LIMITS_INIT;
    if (wgpuGetInstanceLimits(&native_limits) != WGPUStatus_Success)
        return Err(ErrorCode::GraphicsInitFailed, "Failed to query instance limits");
    return Ok(InstanceLimits{.timed_wait_any_max_count = static_cast<u64>(native_limits.timedWaitAnyMaxCount)});
}

bool HasInstanceFeature(const InstanceFeatureName feature) noexcept {
    return wgpuHasInstanceFeature(wgpu::convert::ToWgpu(feature));
}

Proc GetProcAddress(const std::string_view proc_name) noexcept {
    return reinterpret_cast<Proc>(wgpuGetProcAddress(wgpu::detail::ToStringView(proc_name)));
}

} // namespace woki::rhi::backend
