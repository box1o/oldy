#include <utility>

#include "wgpu_enums.hpp"
#include "wgpu_device.hpp"
#include "wgpu_adapter.hpp"
#include "wgpu_objects.hpp"
#include "detail/string.hpp"
#include "wgpu_instance.hpp"
#include "wgpu_swapchain.hpp"
#include "wgpu_command_encoder.hpp"
#include "detail/device_features.hpp"
#include "detail/device_descriptor.hpp"
#include "detail/pipeline_descriptor.hpp"
#include "detail/resource_descriptor.hpp"
#include "detail/external_texture_descriptor.hpp"

namespace woki::rhi::wgpu {
namespace {

using convert::FromWgpu;
using convert::ToWgpu;

[[nodiscard]] Future MakeInvalidFuture(std::string message) {
    Future future{};
    future.message = std::move(message);
    return future;
}

struct PopErrorScopeCallbackState {
    PopErrorScopeCallback callback;
};

void PopErrorScopeThunk(WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message, void* userdata1, void*) {
    auto state = scope<PopErrorScopeCallbackState>(static_cast<PopErrorScopeCallbackState*>(userdata1));
    if (state == nullptr || !state->callback) {
        return;
    }

    state->callback(FromWgpu(status), FromWgpu(type), detail::StringFromView(message));
}

struct CreateComputePipelineCallbackState {
    CreateComputePipelineCallback callback;
};

void CreateComputePipelineThunk(WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline, WGPUStringView message, void* userdata1, void*) {
    auto state = scope<CreateComputePipelineCallbackState>(static_cast<CreateComputePipelineCallbackState*>(userdata1));
    if (state == nullptr || !state->callback) {
        if (pipeline != nullptr) {
            wgpuComputePipelineRelease(pipeline);
        }
        return;
    }

    scope<ComputePipeline> pipeline_scope{};
    if (status == WGPUCreatePipelineAsyncStatus_Success && pipeline != nullptr) {
        pipeline_scope = CreateComputePipelineObject(pipeline);
    } else if (pipeline != nullptr) {
        wgpuComputePipelineRelease(pipeline);
    }

    state->callback(FromWgpu(status), std::move(pipeline_scope), detail::StringFromView(message));
}

struct CreateRenderPipelineCallbackState {
    CreateRenderPipelineCallback callback;
};

void CreateRenderPipelineThunk(WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, WGPUStringView message, void* userdata1, void*) {
    auto state = scope<CreateRenderPipelineCallbackState>(static_cast<CreateRenderPipelineCallbackState*>(userdata1));
    if (state == nullptr || !state->callback) {
        if (pipeline != nullptr) {
            wgpuRenderPipelineRelease(pipeline);
        }
        return;
    }

    scope<RenderPipeline> pipeline_scope{};
    if (status == WGPUCreatePipelineAsyncStatus_Success && pipeline != nullptr) {
        pipeline_scope = CreateRenderPipelineObject(pipeline);
    } else if (pipeline != nullptr) {
        wgpuRenderPipelineRelease(pipeline);
    }

    state->callback(FromWgpu(status), std::move(pipeline_scope), detail::StringFromView(message));
}

void LoggingCallbackThunk(WGPULoggingType type, WGPUStringView message, void* userdata1, void*) {
    const auto* callback = static_cast<LoggingCallback*>(userdata1);
    if (callback == nullptr || !*callback) {
        return;
    }

    (*callback)(FromWgpu(type), detail::StringFromView(message));
}

template <typename ResultType, typename CreateFn, typename WrapFn>
[[nodiscard]] Result<scope<ResultType>> CreateResource(WGPUDevice device, CreateFn create_fn, WrapFn wrap_fn, std::string_view resource_name) {
    if (device == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Device is invalid");
    }

    const auto native_handle = create_fn(device);
    if (native_handle == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, std::string("Failed to create ") + std::string(resource_name));
    }

    return Ok(wrap_fn(native_handle));
}

} // namespace

WgpuDeviceImpl::WgpuDeviceImpl(WGPUInstance instance, WGPUAdapter adapter, WGPUDevice device, ref<DeviceLostCallback> device_lost_callback, ref<UncapturedErrorCallback> uncaptured_error_callback)
    : instance_(detail::InstanceHandle::Retain(instance)),
      adapter_handle_(detail::AdapterHandle::Retain(adapter)),
      device_lost_callback_(std::move(device_lost_callback)),
      uncaptured_error_callback_(std::move(uncaptured_error_callback)),
      device_(device),
      queue_(device != nullptr ? wgpuDeviceGetQueue(device) : nullptr) {}

WgpuDeviceImpl::~WgpuDeviceImpl() {
    if (device_ && logging_callback_) {
        wgpuDeviceSetLoggingCallback(device_.get(), WGPU_LOGGING_CALLBACK_INFO_INIT);
    }
}

Result<scope<BindGroup>> WgpuDeviceImpl::CreateBindGroup(const BindGroupDesc& desc) {
    const detail::BindGroupDescriptorStorage storage(desc);
    return CreateResource<BindGroup>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateBindGroup(device, &storage.native); }, CreateBindGroupObject, "bind group");
}

Result<scope<BindGroupLayout>> WgpuDeviceImpl::CreateBindGroupLayout(const BindGroupLayoutDesc& desc) {
    const detail::BindGroupLayoutDescriptorStorage storage(desc);
    return CreateResource<BindGroupLayout>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateBindGroupLayout(device, &storage.native); }, CreateBindGroupLayoutObject, "bind group layout");
}

Result<scope<Buffer>> WgpuDeviceImpl::CreateBuffer(const BufferDesc& desc) {
    const detail::BufferDescriptorStorage storage(desc);
    return CreateResource<Buffer>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateBuffer(device, &storage.native); }, CreateBufferObject, "buffer");
}

Result<scope<CommandEncoder>> WgpuDeviceImpl::CreateCommandEncoder(const CommandEncoderDesc& desc) {
    const detail::CommandEncoderDescriptorStorage storage(desc);
    return CreateResource<CommandEncoder>(
        device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateCommandEncoder(device, &storage.native); }, [](WGPUCommandEncoder handle) { return createScope<WgpuCommandEncoderImpl>(handle); }, "command encoder"
    );
}

Result<scope<ComputePipeline>> WgpuDeviceImpl::CreateComputePipeline(const ComputePipelineDesc& desc) {
    const detail::ComputePipelineDescriptorStorage storage(desc);
    return CreateResource<ComputePipeline>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateComputePipeline(device, &storage.native); }, CreateComputePipelineObject, "compute pipeline");
}

Future WgpuDeviceImpl::CreateComputePipelineAsync(const ComputePipelineDesc& desc, CallbackMode callback_mode, CreateComputePipelineCallback callback) {
    if (!device_) {
        return MakeInvalidFuture("Device is invalid");
    }
    if (!callback) {
        return MakeInvalidFuture("CreateComputePipelineAsync requires a callback");
    }

    auto callback_state = createScope<CreateComputePipelineCallbackState>(CreateComputePipelineCallbackState{
        .callback = std::move(callback),
    });
    const detail::ComputePipelineDescriptorStorage descriptor(desc);

    WGPUCreateComputePipelineAsyncCallbackInfo callback_info = WGPU_CREATE_COMPUTE_PIPELINE_ASYNC_CALLBACK_INFO_INIT;
    callback_info.mode = ToWgpu(callback_mode);
    callback_info.callback = CreateComputePipelineThunk;
    callback_info.userdata1 = callback_state.get();

    auto* transferred_state = callback_state.release();
    const WGPUFuture native_future = wgpuDeviceCreateComputePipelineAsync(device_.get(), &descriptor.native, callback_info);

    Future future{};
    future.id = native_future.id;
    if (future.id == 0) {
        callback_state.reset(transferred_state);
        future.message = "Compute pipeline request failed to start";
    }
    return future;
}

Result<scope<Buffer>> WgpuDeviceImpl::CreateErrorBuffer(const BufferDesc& desc) {
    const detail::BufferDescriptorStorage storage(desc);
    return CreateResource<Buffer>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateErrorBuffer(device, &storage.native); }, CreateBufferObject, "error buffer");
}

Result<scope<ExternalTexture>> WgpuDeviceImpl::CreateErrorExternalTexture() {
    return CreateResource<ExternalTexture>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateErrorExternalTexture(device); }, CreateExternalTextureObject, "error external texture");
}

Result<scope<ShaderModule>> WgpuDeviceImpl::CreateErrorShaderModule(const ShaderModuleDesc& desc, const std::string_view error_message) {
    const detail::ShaderModuleDescriptorStorage storage(desc);
    return CreateResource<ShaderModule>(
        device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateErrorShaderModule(device, &storage.native, detail::ToStringView(error_message)); }, CreateShaderModuleObject, "error shader module"
    );
}

Result<scope<Texture>> WgpuDeviceImpl::CreateErrorTexture(const TextureDesc& desc) {
    const detail::TextureDescriptorStorage storage(desc);
    return CreateResource<Texture>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateErrorTexture(device, &storage.native); }, CreateTextureObject, "error texture");
}

Result<scope<ExternalTexture>> WgpuDeviceImpl::CreateExternalTexture(const ExternalTextureDesc& desc) {
    const detail::ExternalTextureDescriptorStorage storage(desc);
    return CreateResource<ExternalTexture>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateExternalTexture(device, &storage.native); }, CreateExternalTextureObject, "external texture");
}

Result<scope<PipelineLayout>> WgpuDeviceImpl::CreatePipelineLayout(const PipelineLayoutDesc& desc) {
    const detail::PipelineLayoutDescriptorStorage storage(desc);
    return CreateResource<PipelineLayout>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreatePipelineLayout(device, &storage.native); }, CreatePipelineLayoutObject, "pipeline layout");
}

Result<scope<QuerySet>> WgpuDeviceImpl::CreateQuerySet(const QuerySetDesc& desc) {
    const detail::QuerySetDescriptorStorage storage(desc);
    return CreateResource<QuerySet>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateQuerySet(device, &storage.native); }, CreateQuerySetObject, "query set");
}

Result<scope<RenderBundleEncoder>> WgpuDeviceImpl::CreateRenderBundleEncoder(const RenderBundleEncoderDesc& desc) {
    const detail::RenderBundleEncoderDescriptorStorage storage(desc);
    return CreateResource<RenderBundleEncoder>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateRenderBundleEncoder(device, &storage.native); }, CreateRenderBundleEncoderObject, "render bundle encoder");
}

Result<scope<RenderPipeline>> WgpuDeviceImpl::CreateRenderPipeline(const RenderPipelineDesc& desc) {
    const detail::RenderPipelineDescriptorStorage storage(desc);
    return CreateResource<RenderPipeline>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateRenderPipeline(device, &storage.native); }, CreateRenderPipelineObject, "render pipeline");
}

Result<scope<RenderPipeline>> WgpuDeviceImpl::CreateRenderPipeline(const RenderPipelineDescTyped& desc) {
    const detail::RenderPipelineTypedDescriptorStorage storage(desc);
    return CreateResource<RenderPipeline>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateRenderPipeline(device, &storage.native); }, CreateRenderPipelineObject, "render pipeline");
}

Future WgpuDeviceImpl::CreateRenderPipelineAsync(const RenderPipelineDesc& desc, CallbackMode callback_mode, CreateRenderPipelineCallback callback) {
    if (!device_) {
        return MakeInvalidFuture("Device is invalid");
    }
    if (!callback) {
        return MakeInvalidFuture("CreateRenderPipelineAsync requires a callback");
    }

    auto callback_state = createScope<CreateRenderPipelineCallbackState>(CreateRenderPipelineCallbackState{
        .callback = std::move(callback),
    });
    const detail::RenderPipelineDescriptorStorage descriptor(desc);

    WGPUCreateRenderPipelineAsyncCallbackInfo callback_info = WGPU_CREATE_RENDER_PIPELINE_ASYNC_CALLBACK_INFO_INIT;
    callback_info.mode = ToWgpu(callback_mode);
    callback_info.callback = CreateRenderPipelineThunk;
    callback_info.userdata1 = callback_state.get();

    auto* transferred_state = callback_state.release();
    const WGPUFuture native_future = wgpuDeviceCreateRenderPipelineAsync(device_.get(), &descriptor.native, callback_info);

    Future future{};
    future.id = native_future.id;
    if (future.id == 0) {
        callback_state.reset(transferred_state);
        future.message = "Render pipeline request failed to start";
    }
    return future;
}

Result<scope<ResourceTable>> WgpuDeviceImpl::CreateResourceTable(const ResourceTableDesc& desc) {
    const detail::ResourceTableDescriptorStorage storage(desc);
    return CreateResource<ResourceTable>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateResourceTable(device, &storage.native); }, CreateResourceTableObject, "resource table");
}

Result<scope<Sampler>> WgpuDeviceImpl::CreateSampler(const SamplerDesc& desc) {
    const detail::SamplerDescriptorStorage storage(desc);
    return CreateResource<Sampler>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateSampler(device, &storage.native); }, CreateSamplerObject, "sampler");
}

Result<scope<ShaderModule>> WgpuDeviceImpl::CreateShaderModule(const ShaderModuleDesc& desc) {
    const detail::ShaderModuleDescriptorStorage storage(desc);
    return CreateResource<ShaderModule>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateShaderModule(device, &storage.native); }, CreateShaderModuleObject, "shader module");
}

Result<scope<Texture>> WgpuDeviceImpl::CreateTexture(const TextureDesc& desc) {
    const detail::TextureDescriptorStorage storage(desc);
    return CreateResource<Texture>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceCreateTexture(device, &storage.native); }, CreateTextureObject, "texture");
}

Result<scope<Swapchain>> WgpuDeviceImpl::CreateSwapchain(ref<Surface> surface, SwapchainDesc desc) {
    return CreateSwapchainObject(shared_from_this(), std::move(surface), std::move(desc));
}

void WgpuDeviceImpl::Destroy() {
    if (device_) {
        wgpuDeviceDestroy(device_.get());
    }
}

void WgpuDeviceImpl::ForceLoss(const DeviceLostReason reason, const std::string_view message) {
    if (device_) {
        wgpuDeviceForceLoss(device_.get(), ToWgpu(reason), detail::ToStringView(message));
    }
}

Result<scope<Adapter>> WgpuDeviceImpl::GetAdapter() const {
    if (!device_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Device is invalid");
    }
    WGPUAdapter native_adapter = wgpuDeviceGetAdapter(device_.get());
    if (native_adapter == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to get adapter from device");
    }

    return Ok(createScope<WgpuAdapterImpl>(instance_.get(), native_adapter));
}

Result<void> WgpuDeviceImpl::GetAdapterInfo(AdapterInfo& info) const {
    if (!device_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Device is invalid");
    }

    WGPUAdapterInfo native_info = WGPU_ADAPTER_INFO_INIT;
    if (wgpuDeviceGetAdapterInfo(device_.get(), &native_info) != WGPUStatus_Success) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to query adapter info from device");
    }

    info.vendor = detail::StringFromView(native_info.vendor);
    info.architecture = detail::StringFromView(native_info.architecture);
    info.device = detail::StringFromView(native_info.device);
    info.description = detail::StringFromView(native_info.description);
    info.backend_type = FromWgpu(native_info.backendType);
    info.adapter_type = FromWgpu(native_info.adapterType);
    info.vendor_id = native_info.vendorID;
    info.device_id = native_info.deviceID;
    info.subgroup_min_size = native_info.subgroupMinSize;
    info.subgroup_max_size = native_info.subgroupMaxSize;

    wgpuAdapterInfoFreeMembers(native_info);
    return Ok();
}

Result<void> WgpuDeviceImpl::GetAHardwareBufferProperties(void* handle, void* properties) const {
    if (!device_) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Device is invalid");
    }
    if (handle == nullptr || properties == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Invalid AHardwareBuffer arguments");
    }

    if (wgpuDeviceGetAHardwareBufferProperties(device_.get(), handle, static_cast<WGPUAHardwareBufferProperties*>(properties)) != WGPUStatus_Success) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to query AHardwareBuffer properties");
    }

    return Ok();
}

void WgpuDeviceImpl::GetFeatures(SupportedFeatures& features) const {
    detail::FillSupportedFeatures(device_.get(), features);
}

Result<void> WgpuDeviceImpl::GetLimits(Limits& limits) const {
    return detail::FillDeviceLimits(device_.get(), limits);
}

Future WgpuDeviceImpl::GetLostFuture() const {
    Future future{};
    if (!device_) {
        future.message = "Device is invalid";
        return future;
    }

    const WGPUFuture native_future = wgpuDeviceGetLostFuture(device_.get());
    future.id = native_future.id;
    return future;
}

Queue& WgpuDeviceImpl::GetQueue() const noexcept {
    return queue_;
}

bool WgpuDeviceImpl::HasFeature(const FeatureName feature) const noexcept {
    return detail::DeviceHasFeature(device_.get(), feature);
}

Result<scope<SharedBufferMemory>> WgpuDeviceImpl::ImportSharedBufferMemory(const SharedBufferMemoryDesc& desc) {
    WGPUSharedBufferMemoryDescriptor native = WGPU_SHARED_BUFFER_MEMORY_DESCRIPTOR_INIT;
    native.label = detail::ToStringView(desc.label);
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    return CreateResource<SharedBufferMemory>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceImportSharedBufferMemory(device, &native); }, CreateSharedBufferMemoryObject, "shared buffer memory");
}

Result<scope<SharedFence>> WgpuDeviceImpl::ImportSharedFence(const SharedFenceDesc& desc) {
    WGPUSharedFenceDescriptor native = WGPU_SHARED_FENCE_DESCRIPTOR_INIT;
    native.label = detail::ToStringView(desc.label);
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    return CreateResource<SharedFence>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceImportSharedFence(device, &native); }, CreateSharedFenceObject, "shared fence");
}

Result<scope<SharedTextureMemory>> WgpuDeviceImpl::ImportSharedTextureMemory(const SharedTextureMemoryDesc& desc) {
    WGPUSharedTextureMemoryDescriptor native = WGPU_SHARED_TEXTURE_MEMORY_DESCRIPTOR_INIT;
    native.label = detail::ToStringView(desc.label);
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    return CreateResource<SharedTextureMemory>(device_.get(), [&](WGPUDevice device) { return wgpuDeviceImportSharedTextureMemory(device, &native); }, CreateSharedTextureMemoryObject, "shared texture memory");
}

void WgpuDeviceImpl::InjectError(const ErrorType type, const std::string_view message) {
    if (device_) {
        wgpuDeviceInjectError(device_.get(), ToWgpu(type), detail::ToStringView(message));
    }
}

Future WgpuDeviceImpl::PopErrorScope(CallbackMode callback_mode, PopErrorScopeCallback callback) const {
    if (!device_) {
        return MakeInvalidFuture("Device is invalid");
    }
    if (!callback) {
        return MakeInvalidFuture("PopErrorScope requires a callback");
    }

    auto callback_state = createScope<PopErrorScopeCallbackState>(PopErrorScopeCallbackState{.callback = std::move(callback)});

    WGPUPopErrorScopeCallbackInfo callback_info = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
    callback_info.mode = ToWgpu(callback_mode);
    callback_info.callback = PopErrorScopeThunk;
    callback_info.userdata1 = callback_state.get();

    auto* transferred_state = callback_state.release();
    const WGPUFuture native_future = wgpuDevicePopErrorScope(device_.get(), callback_info);

    Future future{};
    future.id = native_future.id;
    if (future.id == 0) {
        callback_state.reset(transferred_state);
        future.message = "PopErrorScope request failed to start";
    }
    return future;
}

void WgpuDeviceImpl::PushErrorScope(const ErrorFilter filter) {
    if (device_) {
        wgpuDevicePushErrorScope(device_.get(), ToWgpu(filter));
    }
}

void WgpuDeviceImpl::SetLabel(const std::string_view label) {
    if (device_) {
        wgpuDeviceSetLabel(device_.get(), detail::ToStringView(label));
    }
}

void WgpuDeviceImpl::SetLoggingCallback(LoggingCallback callback) {
    if (!device_) {
        return;
    }

    if (!callback) {
        wgpuDeviceSetLoggingCallback(device_.get(), WGPU_LOGGING_CALLBACK_INFO_INIT);
        logging_callback_.reset();
        return;
    }

    logging_callback_ = createRef<LoggingCallback>(std::move(callback));

    WGPULoggingCallbackInfo callback_info = WGPU_LOGGING_CALLBACK_INFO_INIT;
    callback_info.callback = LoggingCallbackThunk;
    callback_info.userdata1 = logging_callback_.get();
    wgpuDeviceSetLoggingCallback(device_.get(), callback_info);
}

void WgpuDeviceImpl::Tick() const noexcept {
    if (device_) {
        wgpuDeviceTick(device_.get());
    }
}

void WgpuDeviceImpl::ValidateTextureDescriptor(const TextureDesc& desc) const {
    if (!device_) {
        return;
    }

    const detail::TextureDescriptorStorage storage(desc);
    wgpuDeviceValidateTextureDescriptor(device_.get(), &storage.native);
}

NativeHandles WgpuDeviceImpl::GetNativeHandles() const noexcept {
    NativeHandles handles{};
    handles.instance = instance_.get();
    handles.adapter = adapter_handle_.get();
    handles.device = device_.get();
    handles.queue = queue_.GetNativeQueue();
    return handles;
}

WGPUDevice WgpuDeviceImpl::GetNativeDevice() const noexcept {
    return device_.get();
}

} // namespace woki::rhi::wgpu
