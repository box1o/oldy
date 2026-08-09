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

[[nodiscard]] inline DeviceDescriptorStorage BuildDeviceDescriptor(const DeviceDesc& desc) {
    DeviceDescriptorStorage storage{};
    storage.required_features.reserve(desc.required_features.size());
    for (const FeatureName feature : desc.required_features) {
        storage.required_features.push_back(ToWgpu(feature));
    }

    storage.native_desc = WGPU_DEVICE_DESCRIPTOR_INIT;
    storage.native_desc.label = ToStringView(desc.label);
    storage.native_desc.requiredFeatureCount = storage.required_features.size();
    storage.native_desc.requiredFeatures = storage.required_features.empty() ? nullptr : storage.required_features.data();

    if (desc.required_limits.has_value()) {
        storage.required_limits = ToWgpuLimits(*desc.required_limits);
        storage.native_desc.requiredLimits = &storage.required_limits;
    }

    storage.default_queue = WGPU_QUEUE_DESCRIPTOR_INIT;
    storage.default_queue.label = ToStringView(desc.default_queue.label);
    storage.native_desc.defaultQueue = storage.default_queue;

    if (desc.device_lost_callback) {
        storage.device_lost_callback = createRef<DeviceLostCallback>(desc.device_lost_callback);
        storage.native_desc.deviceLostCallbackInfo = WGPU_DEVICE_LOST_CALLBACK_INFO_INIT;
        storage.native_desc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
        storage.native_desc.deviceLostCallbackInfo.callback = DeviceLostThunk;
        storage.native_desc.deviceLostCallbackInfo.userdata2 = storage.device_lost_callback.get();
    }

    if (desc.uncaptured_error_callback) {
        storage.uncaptured_error_callback = createRef<UncapturedErrorCallback>(desc.uncaptured_error_callback);
        storage.native_desc.uncapturedErrorCallbackInfo = WGPU_UNCAPTURED_ERROR_CALLBACK_INFO_INIT;
        storage.native_desc.uncapturedErrorCallbackInfo.callback = UncapturedErrorThunk;
        storage.native_desc.uncapturedErrorCallbackInfo.userdata2 = storage.uncaptured_error_callback.get();
    }

    return storage;
}

} // namespace woki::rhi::wgpu::detail
