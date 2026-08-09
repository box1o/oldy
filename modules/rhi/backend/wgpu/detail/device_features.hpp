#pragma once

#include <webgpu/webgpu.h>

#include <woki/rhi/descriptors.hpp>

#include "limits.hpp"
#include "string.hpp"
#include "../wgpu_enums.hpp"

namespace woki::rhi::wgpu::detail {

using namespace woki::rhi::wgpu::convert;

inline void FillSupportedFeatures(WGPUDevice device, SupportedFeatures& features) {
    features.values.clear();
    if (device == nullptr) {
        return;
    }

    WGPUSupportedFeatures native_features = WGPU_SUPPORTED_FEATURES_INIT;
    wgpuDeviceGetFeatures(device, &native_features);

    features.values.reserve(native_features.featureCount);
    for (size_t i = 0; i < native_features.featureCount; ++i) {
        features.values.push_back(FromWgpu(native_features.features[i]));
    }

    wgpuSupportedFeaturesFreeMembers(native_features);
}

[[nodiscard]] inline Result<void> FillDeviceLimits(WGPUDevice device, Limits& limits) {
    if (device == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Device is invalid");
    }

    WGPULimits native_limits = WGPU_LIMITS_INIT;
    if (wgpuDeviceGetLimits(device, &native_limits) != WGPUStatus_Success) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to query device limits");
    }

    FillLimitsFromNative(native_limits, limits);
    return Ok();
}

[[nodiscard]] inline bool DeviceHasFeature(WGPUDevice device, FeatureName feature) noexcept {
    return device != nullptr && wgpuDeviceHasFeature(device, ToWgpu(feature));
}

} // namespace woki::rhi::wgpu::detail
