#include "wgpu_enums.hpp"
#include "wgpu_device.hpp"
#include "wgpu_adapter.hpp"
#include "detail/limits.hpp"
#include "detail/string.hpp"
#include "wgpu_instance.hpp"
#include "detail/adapter_info.hpp"
#include "detail/device_descriptor.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <utility>

namespace woki::rhi::wgpu {
namespace {

using convert::FromWgpu;
using convert::ToWgpu;

struct RequestDeviceCallbackState {
    std::function<void(WGPURequestDeviceStatus, WGPUDevice, std::string_view)> callback;
};

void RequestDeviceThunk(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void*) {
    auto state = scope<RequestDeviceCallbackState>(static_cast<RequestDeviceCallbackState*>(userdata1));
    if (state == nullptr || !state->callback) {
        return;
    }

    state->callback(status, device, detail::StringFromView(message));
}

[[nodiscard]] ref<Device> CreateDeviceFromNative(WGPUInstance instance,
    WGPUAdapter adapter,
    WGPUDevice device,
    ref<DeviceLostCallback> device_lost_callback,
    ref<UncapturedErrorCallback> uncaptured_error_callback,
    ref<WgpuDeviceLossState> loss_state) {
    if (device == nullptr) {
        return nullptr;
    }

    return createRef<WgpuDeviceImpl>(instance, adapter, device, std::move(device_lost_callback), std::move(uncaptured_error_callback), std::move(loss_state));
}

DeviceDesc WithLossTracking(const DeviceDesc& desc, const ref<WgpuDeviceLossState>& state) {
    DeviceDesc tracked = desc;
    tracked.device_lost_callback = [state, callback = desc.device_lost_callback](const DeviceLostReason reason, const std::string_view message) {
        state->reason.store(reason, std::memory_order_release);
        state->lost.store(true, std::memory_order_release);
        if (callback)
            callback(reason, message);
    };
    return tracked;
}

} // namespace

WgpuAdapterImpl::WgpuAdapterImpl(WGPUInstance instance, WGPUAdapter adapter)
    : instance_handle_(detail::InstanceHandle::Retain(instance)),
      adapter_(adapter),
      info_(detail::QueryAdapterInfo(adapter)),
      features_(detail::QuerySupportedFeatures(adapter)),
      limits_(detail::QueryLimits(adapter)) {}

WgpuAdapterImpl::~WgpuAdapterImpl() = default;

Result<ref<Device>> WgpuAdapterImpl::CreateDevice(const DeviceDesc& desc) {
    if (!adapter_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Adapter is invalid");
    }

#ifdef __EMSCRIPTEN__
    return RequestDevice(desc);
#else
    auto loss_state = createRef<WgpuDeviceLossState>();
    detail::DeviceDescriptorStorage storage(WithLossTracking(desc, loss_state));
    WGPUDevice device = wgpuAdapterCreateDevice(adapter_.get(), &storage.native_desc);
    if (device == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to create device '" + desc.label + "'");
    }

    auto device_scope = CreateDeviceFromNative(instance_handle_.get(), adapter_.get(), device, std::move(storage.device_lost_callback), std::move(storage.uncaptured_error_callback), std::move(loss_state));
    if (!device_scope) {
        wgpuDeviceRelease(device);
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to wrap created device");
    }

    return Ok(std::move(device_scope));
#endif
}

Result<ref<Device>> WgpuAdapterImpl::RequestDevice(const DeviceDesc& desc) {
    if (!adapter_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Adapter is invalid");
    }

    struct State {
        bool done{false};
        WGPUDevice device{nullptr};
        std::string message{};
    } state;

    auto loss_state = createRef<WgpuDeviceLossState>();
    detail::DeviceDescriptorStorage storage(WithLossTracking(desc, loss_state));

    WGPURequestDeviceCallbackInfo callback_info = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
#ifdef __EMSCRIPTEN__
    callback_info.mode = WGPUCallbackMode_AllowSpontaneous;
#else
    callback_info.mode = WGPUCallbackMode_AllowProcessEvents;
#endif
    callback_info.callback = [](const WGPURequestDeviceStatus status, WGPUDevice device, const WGPUStringView message, void*, void* userdata) {
        auto* state_ptr = static_cast<State*>(userdata);
        state_ptr->done = true;
        if (status == WGPURequestDeviceStatus_Success) {
            state_ptr->device = device;
        }
        state_ptr->message = detail::StringFromView(message);
    };
    callback_info.userdata2 = &state;

    const auto future = wgpuAdapterRequestDevice(adapter_.get(), &storage.native_desc, callback_info);
    if (future.id == 0) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "WebGPU device request failed to start");
    }

    while (!state.done) {
#ifdef __EMSCRIPTEN__
        emscripten_sleep(1);
#else
        wgpuInstanceProcessEvents(instance_handle_.get());
#endif
    }

    if (state.device == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, state.message.empty() ? "Failed to request WebGPU device" : state.message);
    }

    auto device_scope = CreateDeviceFromNative(instance_handle_.get(), adapter_.get(), state.device, std::move(storage.device_lost_callback), std::move(storage.uncaptured_error_callback), std::move(loss_state));
    if (!device_scope) {
        wgpuDeviceRelease(state.device);
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to wrap requested device");
    }

    return Ok(std::move(device_scope));
}

Future WgpuAdapterImpl::RequestDevice(const DeviceDesc& desc, CallbackMode callback_mode, RequestDeviceCallback callback) {
    Future future{};
    if (!adapter_) {
        future.message = "Adapter is invalid";
        return future;
    }

    if (!callback) {
        future.message = "RequestDevice requires a callback";
        return future;
    }

    auto loss_state = createRef<WgpuDeviceLossState>();
    auto storage = createScope<detail::DeviceDescriptorStorage>(WithLossTracking(desc, loss_state));
    auto device_lost_callback = storage->device_lost_callback;
    auto uncaptured_error_callback = storage->uncaptured_error_callback;
    auto retained_instance = instance_handle_;
    auto retained_adapter = adapter_;

    auto callback_state = createScope<RequestDeviceCallbackState>(RequestDeviceCallbackState{
        .callback =
            [callback = std::move(callback), device_lost_callback = std::move(device_lost_callback), uncaptured_error_callback = std::move(uncaptured_error_callback), loss_state = std::move(loss_state),
                retained_instance = std::move(retained_instance), retained_adapter = std::move(retained_adapter)](const WGPURequestDeviceStatus status, WGPUDevice device, const std::string_view message) mutable {
                ref<Device> device_scope{};
                if (status == WGPURequestDeviceStatus_Success && device != nullptr) {
                    device_scope = CreateDeviceFromNative(retained_instance.get(), retained_adapter.get(), device, std::move(device_lost_callback), std::move(uncaptured_error_callback), std::move(loss_state));
                    if (!device_scope) {
                        wgpuDeviceRelease(device);
                    }
                } else if (device != nullptr) {
                    wgpuDeviceRelease(device);
                }

                callback(FromWgpu(status), std::move(device_scope), message);
            },
    });

    WGPURequestDeviceCallbackInfo callback_info = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    callback_info.mode = ToWgpu(callback_mode);
    callback_info.callback = RequestDeviceThunk;
    callback_info.userdata1 = callback_state.get();

    auto* transferred_state = callback_state.release();
    const auto native_future = wgpuAdapterRequestDevice(adapter_.get(), &storage->native_desc, callback_info);
    future.id = native_future.id;

    if (future.id == 0) {
        callback_state.reset(transferred_state);
        future.message = "WebGPU device request failed to start";
    }

    return future;
}

AdapterInfo WgpuAdapterImpl::GetInfo() const {
    return detail::QueryAdapterInfo(adapter_.get());
}

Result<void> WgpuAdapterImpl::GetInfo(AdapterInfo& info) const {
    return detail::FillAdapterInfo(adapter_.get(), info);
}

void WgpuAdapterImpl::GetFeatures(SupportedFeatures& features) const {
    features = detail::QuerySupportedFeatures(adapter_.get());
}

SupportedFeatures WgpuAdapterImpl::GetFeatures() const {
    return detail::QuerySupportedFeatures(adapter_.get());
}

Result<void> WgpuAdapterImpl::GetFormatCapabilities(const TextureFormat format, DawnFormatCapabilities& capabilities) const {
#ifdef __EMSCRIPTEN__
    (void)format;
    (void)capabilities;
    return Err(ErrorCode::GraphicsUnsupportedApi, "Format capability queries are unavailable with emdawnwebgpu");
#else
    if (!adapter_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Adapter is invalid");
    }

    WGPUDawnFormatCapabilities native_capabilities = WGPU_DAWN_FORMAT_CAPABILITIES_INIT;
    native_capabilities.nextInChain = static_cast<WGPUChainedStruct*>(capabilities.next_in_chain);

    if (wgpuAdapterGetFormatCapabilities(adapter_.get(), ToWgpu(format), &native_capabilities) != WGPUStatus_Success) {
        return Err(ErrorCode::GraphicsInvalidFormat, "Failed to query format capabilities");
    }

    capabilities.next_in_chain = native_capabilities.nextInChain;
    return Ok();
#endif
}

Limits WgpuAdapterImpl::GetLimits() const {
    return detail::QueryLimits(adapter_.get());
}

Result<void> WgpuAdapterImpl::GetLimits(Limits& limits) const {
    return detail::FillLimits(adapter_.get(), limits);
}

bool WgpuAdapterImpl::HasFeature(const FeatureName feature) const noexcept {
    return detail::AdapterHasFeature(adapter_.get(), feature);
}

NativeHandles WgpuAdapterImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.instance = instance_handle_.get();
    handles.adapter = adapter_.get();
    return handles;
}

WGPUAdapter WgpuAdapterImpl::GetNativeAdapter() const noexcept {
    return adapter_.get();
}

WGPUInstance WgpuAdapterImpl::GetNativeInstance() const noexcept {
    return instance_handle_.get();
}

} // namespace woki::rhi::wgpu
