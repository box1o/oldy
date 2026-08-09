#pragma once

#include <vector>
#include <webgpu/webgpu.h>

#include <woki/rhi/types.hpp>
#include <woki/rhi/descriptors.hpp>

#include "limits.hpp"
#include "string.hpp"

namespace woki::rhi::wgpu::detail {

using namespace woki::rhi::wgpu::convert;

struct DeviceDescriptorStorage final {
    std::vector<WGPUFeatureName> required_features{};
    WGPULimits required_limits = WGPU_LIMITS_INIT;
    WGPUQueueDescriptor default_queue = WGPU_QUEUE_DESCRIPTOR_INIT;
    WGPUDeviceDescriptor native_desc = WGPU_DEVICE_DESCRIPTOR_INIT;
    ref<DeviceLostCallback> device_lost_callback{};
    ref<UncapturedErrorCallback> uncaptured_error_callback{};

    explicit DeviceDescriptorStorage(const DeviceDesc& desc);
    DeviceDescriptorStorage(const DeviceDescriptorStorage&) = delete;
    DeviceDescriptorStorage(DeviceDescriptorStorage&&) = delete;
};

inline void DeviceLostThunk(WGPUDevice const*, WGPUDeviceLostReason reason, WGPUStringView message, void*, void* userdata) {
    const auto* callback = static_cast<DeviceLostCallback*>(userdata);
    if (callback == nullptr || !*callback) {
        return;
    }

    (*callback)(FromWgpu(reason), StringFromView(message));
}

inline void UncapturedErrorThunk(WGPUDevice const*, WGPUErrorType type, WGPUStringView message, void*, void* userdata) {
    const auto* callback = static_cast<UncapturedErrorCallback*>(userdata);
    if (callback == nullptr || !*callback) {
        return;
    }

    (*callback)(FromWgpu(type), StringFromView(message));
}

inline DeviceDescriptorStorage::DeviceDescriptorStorage(const DeviceDesc& desc) {
    required_features.reserve(desc.required_features.size());
    for (const FeatureName feature : desc.required_features) {
        required_features.push_back(ToWgpu(feature));
    }

    native_desc.label = ToStringView(desc.label);
    native_desc.requiredFeatureCount = required_features.size();
    native_desc.requiredFeatures = required_features.empty() ? nullptr : required_features.data();

    if (desc.required_limits.has_value()) {
        required_limits = ToWgpuLimits(*desc.required_limits);
        native_desc.requiredLimits = &required_limits;
    }

    default_queue.label = ToStringView(desc.default_queue.label);
    native_desc.defaultQueue = default_queue;

    if (desc.device_lost_callback) {
        device_lost_callback = createRef<DeviceLostCallback>(desc.device_lost_callback);
        native_desc.deviceLostCallbackInfo = WGPU_DEVICE_LOST_CALLBACK_INFO_INIT;
        native_desc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
        native_desc.deviceLostCallbackInfo.callback = DeviceLostThunk;
        native_desc.deviceLostCallbackInfo.userdata2 = device_lost_callback.get();
    }

    if (desc.uncaptured_error_callback) {
        uncaptured_error_callback = createRef<UncapturedErrorCallback>(desc.uncaptured_error_callback);
        native_desc.uncapturedErrorCallbackInfo = WGPU_UNCAPTURED_ERROR_CALLBACK_INFO_INIT;
        native_desc.uncapturedErrorCallbackInfo.callback = UncapturedErrorThunk;
        native_desc.uncapturedErrorCallbackInfo.userdata2 = uncaptured_error_callback.get();
    }
}

} // namespace woki::rhi::wgpu::detail
