#pragma once

#include <webgpu/webgpu.h>

#include <woki/rhi/types.hpp>
#include <woki/rhi/descriptors.hpp>

#include "string.hpp"
#include "../wgpu_enums.hpp"

namespace woki::rhi::wgpu::detail {

using namespace woki::rhi::wgpu::convert;

[[nodiscard]] inline AdapterInfo QueryAdapterInfo(WGPUAdapter adapter) {
    AdapterInfo info{};
    if (adapter == nullptr) {
        return info;
    }

    WGPUAdapterInfo native_info = WGPU_ADAPTER_INFO_INIT;
    if (wgpuAdapterGetInfo(adapter, &native_info) != WGPUStatus_Success) {
        return info;
    }

    info.vendor = StringFromView(native_info.vendor);
    info.architecture = StringFromView(native_info.architecture);
    info.device = StringFromView(native_info.device);
    info.description = StringFromView(native_info.description);
    info.backend_type = FromWgpu(native_info.backendType);
    info.adapter_type = FromWgpu(native_info.adapterType);
    info.vendor_id = native_info.vendorID;
    info.device_id = native_info.deviceID;
    info.subgroup_min_size = native_info.subgroupMinSize;
    info.subgroup_max_size = native_info.subgroupMaxSize;

    wgpuAdapterInfoFreeMembers(native_info);
    return info;
}

[[nodiscard]] inline SupportedFeatures QuerySupportedFeatures(WGPUAdapter adapter) {
    SupportedFeatures features{};
    if (adapter == nullptr) {
        return features;
    }

    WGPUSupportedFeatures native_features = WGPU_SUPPORTED_FEATURES_INIT;
    wgpuAdapterGetFeatures(adapter, &native_features);

    features.values.reserve(native_features.featureCount);
    for (size_t i = 0; i < native_features.featureCount; ++i) {
        features.values.push_back(FromWgpu(native_features.features[i]));
    }

    wgpuSupportedFeaturesFreeMembers(native_features);
    return features;
}

[[nodiscard]] inline bool AdapterHasFeature(WGPUAdapter adapter, FeatureName feature) noexcept {
    return adapter != nullptr && wgpuAdapterHasFeature(adapter, ToWgpu(feature));
}

} // namespace woki::rhi::wgpu::detail
