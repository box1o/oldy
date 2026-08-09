#include <webgpu/webgpu.h>

#include <woki/rhi/instance.hpp>

#include "wgpu/wgpu_enums.hpp"
#include "wgpu/detail/string.hpp"
#include "wgpu/wgpu_instance.hpp"

namespace woki::rhi {
namespace {

using wgpu::convert::FromWgpu;
using wgpu::convert::ToWgpu;

void QueryInstanceFeatures(SupportedInstanceFeatures& features) {
    features.features.clear();
    WGPUSupportedInstanceFeatures native_features = WGPU_SUPPORTED_INSTANCE_FEATURES_INIT;
    wgpuGetInstanceFeatures(&native_features);

    features.features.reserve(native_features.featureCount);
    for (size_t i = 0; i < native_features.featureCount; ++i) {
        features.features.push_back(FromWgpu(native_features.features[i]));
    }

    wgpuSupportedInstanceFeaturesFreeMembers(native_features);
}

} // namespace

Result<scope<Instance>> Instance::Create(InstanceDesc desc) {
    auto instance = createScope<wgpu::WgpuInstanceImpl>(std::move(desc));
    if (instance == nullptr || !instance->IsValid()) {
        return Err(ErrorCode::GraphicsInitFailed, "Failed to create WebGPU instance");
    }

    return Ok(std::move(instance));
}

SupportedInstanceFeatures Instance::GetInstanceFeatures() {
    SupportedInstanceFeatures features{};
    GetInstanceFeatures(features);
    return features;
}

void Instance::GetInstanceFeatures(SupportedInstanceFeatures& features) {
    QueryInstanceFeatures(features);
}

Result<InstanceLimits> Instance::GetInstanceLimits() {
    WGPUInstanceLimits native_limits = WGPU_INSTANCE_LIMITS_INIT;
    if (wgpuGetInstanceLimits(&native_limits) != WGPUStatus_Success) {
        return Err(ErrorCode::GraphicsInitFailed, "Failed to query instance limits");
    }

    InstanceLimits limits{};
    limits.timed_wait_any_max_count = static_cast<u64>(native_limits.timedWaitAnyMaxCount);
    return Ok(limits);
}

bool Instance::HasInstanceFeature(const InstanceFeatureName feature) noexcept {
    return wgpuHasInstanceFeature(ToWgpu(feature));
}

Proc Instance::GetProcAddress(const std::string_view proc_name) noexcept {
    return reinterpret_cast<Proc>(wgpuGetProcAddress(wgpu::detail::ToStringView(proc_name)));
}

} // namespace woki::rhi
