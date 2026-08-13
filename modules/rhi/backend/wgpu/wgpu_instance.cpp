#include <woki/rhi/instance.hpp>
#include <woki/window/window.hpp>

#include "wgpu_enums.hpp"
#include "wgpu_adapter.hpp"
#include "wgpu_surface.hpp"
#include "detail/string.hpp"
#include "wgpu_instance.hpp"
#include "../null/null_backend.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#ifndef __EMSCRIPTEN__
#include <dawn/native/DawnNative.h>
#endif

#include <limits>
#include <memory>
#include <utility>

namespace woki::rhi::wgpu {
namespace {

using convert::FromWgpu;
using convert::ToWgpu;

struct RequestAdapterCallbackState {
    std::function<void(WGPURequestAdapterStatus, WGPUAdapter, std::string_view)> callback;
};

void RequestAdapterThunk(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void*) {
    auto state = scope<RequestAdapterCallbackState>(static_cast<RequestAdapterCallbackState*>(userdata1));
    if (state == nullptr || !state->callback) {
        return;
    }

    const auto message_text = detail::StringFromView(message);
    state->callback(status, adapter, message_text);
}

[[nodiscard]] std::vector<WGPUInstanceFeatureName> BuildRequiredInstanceFeatures(const std::vector<InstanceFeatureName>& features) {
    std::vector<WGPUInstanceFeatureName> native_features;
    native_features.reserve(features.size());
    for (const InstanceFeatureName feature : features) {
        native_features.push_back(ToWgpu(feature));
    }
    return native_features;
}

[[nodiscard]] SupportedInstanceFeatures QueryInstanceFeatures() {
    SupportedInstanceFeatures features{};
    WGPUSupportedInstanceFeatures native_features = WGPU_SUPPORTED_INSTANCE_FEATURES_INIT;
    wgpuGetInstanceFeatures(&native_features);

    features.features.reserve(native_features.featureCount);
    for (size_t i = 0; i < native_features.featureCount; ++i) {
        features.features.push_back(FromWgpu(native_features.features[i]));
    }

    wgpuSupportedInstanceFeaturesFreeMembers(native_features);
    return features;
}

} // namespace

WgpuInstanceImpl::WgpuInstanceImpl(InstanceDesc desc)
    : desc_(std::move(desc)) {
    (void)Initialize();
}

WgpuInstanceImpl::~WgpuInstanceImpl() = default;

bool WgpuInstanceImpl::IsValid() const noexcept {
    return static_cast<bool>(handle_);
}

const InstanceDesc& WgpuInstanceImpl::GetDesc() const noexcept {
    return desc_;
}

Result<ref<Surface>> WgpuInstanceImpl::CreateSurface(const SurfaceDescriptor& desc) {
    if (!handle_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Instance is invalid");
    }

    WGPUSurfaceDescriptor native_desc = WGPU_SURFACE_DESCRIPTOR_INIT;
    native_desc.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    native_desc.label = detail::ToStringView(desc.label);

    WGPUSurface native_surface = wgpuInstanceCreateSurface(handle_.get(), &native_desc);
    if (native_surface == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to create surface '" + desc.label + "'");
    }

    return Ok(createRef<WgpuSurfaceImpl>(handle_.get(), native_surface));
}

Result<ref<Surface>> WgpuInstanceImpl::CreateSurface(Window& window, SurfaceDesc desc) {
    if (!handle_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Instance is invalid");
    }

    WGPUSurface native_surface = CreateNativeSurface(handle_.get(), window);
    if (native_surface == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to create surface '" + desc.label + "'");
    }

    return Ok(createRef<WgpuSurfaceImpl>(handle_.get(), native_surface));
}

void WgpuInstanceImpl::GetWGSLLanguageFeatures(SupportedWGSLLanguageFeatures& features) const {
    features.features.clear();
    if (!handle_) {
        return;
    }

    WGPUSupportedWGSLLanguageFeatures native_features = WGPU_SUPPORTED_WGSL_LANGUAGE_FEATURES_INIT;
    wgpuInstanceGetWGSLLanguageFeatures(handle_.get(), &native_features);

    features.features.reserve(native_features.featureCount);
    for (size_t i = 0; i < native_features.featureCount; ++i) {
        features.features.push_back(FromWgpu(native_features.features[i]));
    }

    wgpuSupportedWGSLLanguageFeaturesFreeMembers(native_features);
}

bool WgpuInstanceImpl::HasWGSLLanguageFeature(const WGSLLanguageFeatureName feature) const noexcept {
    return handle_ && wgpuInstanceHasWGSLLanguageFeature(handle_.get(), ToWgpu(feature));
}

void WgpuInstanceImpl::ProcessEvents() const noexcept {
    if (handle_) {
        wgpuInstanceProcessEvents(handle_.get());
    }
}

WaitStatus WgpuInstanceImpl::WaitAny(Future& future, const u64 timeout_ns) {
    FutureWaitInfo wait_info{.future = future};
    const auto status = WaitAny(wait_info, timeout_ns);
    future = wait_info.future;
    return status;
}

WaitStatus WgpuInstanceImpl::WaitAny(FutureWaitInfo& wait_info, const u64 timeout_ns) {
    return WaitAny(std::span<FutureWaitInfo>(&wait_info, 1), timeout_ns);
}

WaitStatus WgpuInstanceImpl::WaitAny(const std::span<FutureWaitInfo> wait_infos, const u64 timeout_ns) {
    if (wait_infos.empty()) {
        return WaitStatus::Error;
    }

    std::vector<WGPUFutureWaitInfo> native_futures(wait_infos.size(), WGPU_FUTURE_WAIT_INFO_INIT);
    for (size_t i = 0; i < wait_infos.size(); ++i) {
        native_futures[i].future.id = wait_infos[i].future.id;
        native_futures[i].completed = wait_infos[i].completed;
    }

    const auto status = WaitAnyNative(native_futures.size(), native_futures.data(), timeout_ns);
    for (size_t i = 0; i < wait_infos.size(); ++i) {
        wait_infos[i].completed = native_futures[i].completed != 0;
    }

    return convert::FromWgpu(status);
}

WGPURequestAdapterOptions WgpuInstanceImpl::BuildRequestAdapterOptions(const RequestAdapterDesc& desc) const {
    WGPURequestAdapterOptions options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    options.featureLevel = ToWgpu(desc.feature_level);
    options.powerPreference = ToWgpu(desc.power_preference);
    options.backendType = ToWgpu(desc.backend_type);
    options.forceFallbackAdapter = desc.force_fallback_adapter;

    if (desc.compatible_surface != nullptr) {
        const auto* surface = dynamic_cast<const WgpuSurfaceImpl*>(desc.compatible_surface);
        if (surface != nullptr) {
            options.compatibleSurface = surface->GetNativeSurface();
        }
    }

    return options;
}

Result<scope<Adapter>> WgpuInstanceImpl::RequestAdapter(RequestAdapterDesc desc) {
    if (desc.backend_type == BackendType::Null)
        return null::CreateAdapter();
    if (!handle_) {
        return Err(ErrorCode::GraphicsInitFailed, "Instance is invalid");
    }

    struct State {
        bool done{false};
        WGPUAdapter adapter{nullptr};
        std::string message{};
    } state;

    const auto options = BuildRequestAdapterOptions(desc);

    WGPURequestAdapterCallbackInfo callback_info = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
#ifdef __EMSCRIPTEN__
    callback_info.mode = WGPUCallbackMode_AllowSpontaneous;
#else
    callback_info.mode = WGPUCallbackMode_AllowProcessEvents;
#endif
    callback_info.callback = [](const WGPURequestAdapterStatus status, WGPUAdapter adapter, const WGPUStringView message, void*, void* userdata) {
        auto* state_ptr = static_cast<State*>(userdata);
        state_ptr->done = true;
        if (status == WGPURequestAdapterStatus_Success) {
            state_ptr->adapter = adapter;
        }
        state_ptr->message = detail::StringFromView(message);
    };
    callback_info.userdata2 = &state;

    const auto future = wgpuInstanceRequestAdapter(handle_.get(), &options, callback_info);
    if (future.id == 0) {
        return Err(ErrorCode::GraphicsInitFailed, "WebGPU adapter request failed to start");
    }

    while (!state.done) {
#ifdef __EMSCRIPTEN__
        emscripten_sleep(1);
#else
        ProcessEvents();
#endif
    }

    if (state.adapter == nullptr) {
        return Err(ErrorCode::GraphicsInitFailed, state.message.empty() ? "Failed to request WebGPU adapter" : state.message);
    }

    return Ok(createScope<WgpuAdapterImpl>(handle_.get(), state.adapter));
}

Future WgpuInstanceImpl::RequestAdapter(RequestAdapterDesc desc, CallbackMode callback_mode, RequestAdapterCallback callback) {
    Future future{};
    if (!handle_) {
        future.message = "Instance is invalid";
        return future;
    }

    if (!callback) {
        future.message = "RequestAdapter requires a callback";
        return future;
    }

    const auto options = BuildRequestAdapterOptions(desc);
    auto retained_instance = handle_;

    auto callback_state = createScope<RequestAdapterCallbackState>(RequestAdapterCallbackState{
        .callback =
            [callback = std::move(callback), retained_instance = std::move(retained_instance)](const WGPURequestAdapterStatus status, WGPUAdapter adapter, const std::string_view message) mutable {
                scope<Adapter> adapter_scope{};
                if (status == WGPURequestAdapterStatus_Success && adapter != nullptr) {
                    adapter_scope = createScope<WgpuAdapterImpl>(retained_instance.get(), adapter);
                } else if (adapter != nullptr) {
                    wgpuAdapterRelease(adapter);
                }

                callback(FromWgpu(status), std::move(adapter_scope), message);
            },
    });

    WGPURequestAdapterCallbackInfo callback_info = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    callback_info.mode = ToWgpu(callback_mode);
    callback_info.callback = RequestAdapterThunk;
    callback_info.userdata1 = callback_state.get();

    auto* transferred_state = callback_state.release();
    const auto native_future = wgpuInstanceRequestAdapter(handle_.get(), &options, callback_info);
    future.id = native_future.id;

    if (future.id == 0) {
        callback_state.reset(transferred_state);
        future.message = "WebGPU adapter request failed to start";
    }

    return future;
}

NativeHandles WgpuInstanceImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.instance = handle_.get();
    return handles;
}

WGPUInstance WgpuInstanceImpl::GetNativeInstance() const noexcept {
    return handle_.get();
}

bool WgpuInstanceImpl::Initialize() noexcept {
    const auto native_features = BuildRequiredInstanceFeatures(desc_.required_features);
    const auto supported = QueryInstanceFeatures();
    for (const InstanceFeatureName feature : desc_.required_features) {
        if (!supported.Has(feature)) {
            slog::Error("Failed to create WebGPU instance '{}': unsupported required feature", desc_.label);
            return false;
        }
    }

    WGPUInstanceLimits required_native_limits = WGPU_INSTANCE_LIMITS_INIT;
    const WGPUInstanceLimits* limits_ptr = nullptr;
    if (desc_.required_limits.has_value()) {
        WOKI_ASSERT(desc_.required_limits->timed_wait_any_max_count <= static_cast<u64>(std::numeric_limits<size_t>::max()));
        required_native_limits.timedWaitAnyMaxCount = static_cast<size_t>(desc_.required_limits->timed_wait_any_max_count);
        limits_ptr = &required_native_limits;
    }

    WGPUInstanceDescriptor native_desc = WGPU_INSTANCE_DESCRIPTOR_INIT;
    native_desc.requiredFeatureCount = native_features.size();
    native_desc.requiredFeatures = native_features.empty() ? nullptr : native_features.data();
    native_desc.requiredLimits = limits_ptr;

#ifndef __EMSCRIPTEN__
    dawn::native::DawnInstanceDescriptor dawn_desc{};
    dawn_desc.nextInChain = reinterpret_cast<const ::wgpu::ChainedStruct*>(desc_.next_in_chain);
    dawn_desc.backendValidationLevel = desc_.enable_validation ? dawn::native::BackendValidationLevel::Full : dawn::native::BackendValidationLevel::Disabled;
    native_desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&dawn_desc);
#else
    native_desc.nextInChain = static_cast<WGPUChainedStruct*>(desc_.next_in_chain);
#endif

    handle_.reset(wgpuCreateInstance(&native_desc));
    if (!handle_) {
        slog::Error("Failed to create WebGPU instance '{}'", desc_.label);
        return false;
    }

    slog::Info("Created WebGPU instance '{}'", desc_.label);
    return true;
}

WGPUWaitStatus WgpuInstanceImpl::WaitAnyNative(const size_t future_count, WGPUFutureWaitInfo* futures, const u64 timeout_ns) const noexcept {
    if (!handle_ || future_count == 0 || futures == nullptr) {
        return WGPUWaitStatus_Error;
    }

    return wgpuInstanceWaitAny(handle_.get(), future_count, futures, timeout_ns);
}

} // namespace woki::rhi::wgpu
