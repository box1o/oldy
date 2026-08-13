#pragma once
#include <atomic>

#include <woki/rhi/device.hpp>

#include "wgpu_queue.hpp"
#include "detail/handle.hpp"

namespace woki::rhi::wgpu {

struct WgpuDeviceLossState final {
    std::atomic_bool lost{false};
    std::atomic<DeviceLostReason> reason{DeviceLostReason::Unknown};
};

class WgpuDeviceImpl final : public Device, public ref_from_this<WgpuDeviceImpl> {
public:
    WgpuDeviceImpl(WGPUInstance instance,
        WGPUAdapter adapter,
        WGPUDevice device,
        ref<DeviceLostCallback> device_lost_callback,
        ref<UncapturedErrorCallback> uncaptured_error_callback,
        ref<WgpuDeviceLossState> loss_state);
    ~WgpuDeviceImpl() override;

    [[nodiscard]] Result<scope<BindGroup>> CreateBindGroup(const BindGroupDesc& desc = {}) override;
    [[nodiscard]] Result<scope<BindGroupLayout>> CreateBindGroupLayout(const BindGroupLayoutDesc& desc = {}) override;
    [[nodiscard]] Result<scope<Buffer>> CreateBuffer(const BufferDesc& desc = {}) override;
    [[nodiscard]] Result<scope<CommandEncoder>> CreateCommandEncoder(const CommandEncoderDesc& desc = {}) override;
    [[nodiscard]] Result<scope<ComputePipeline>> CreateComputePipeline(const ComputePipelineDesc& desc = {}) override;
    [[nodiscard]] Future CreateComputePipelineAsync(const ComputePipelineDesc& desc, CallbackMode callback_mode, CreateComputePipelineCallback callback) override;
    [[nodiscard]] Result<scope<Buffer>> CreateErrorBuffer(const BufferDesc& desc = {}) override;
    [[nodiscard]] Result<scope<ExternalTexture>> CreateErrorExternalTexture() override;
    [[nodiscard]] Result<scope<ShaderModule>> CreateErrorShaderModule(const ShaderModuleDesc& desc, std::string_view error_message) override;
    [[nodiscard]] Result<scope<Texture>> CreateErrorTexture(const TextureDesc& desc = {}) override;
    [[nodiscard]] Result<scope<ExternalTexture>> CreateExternalTexture(const ExternalTextureDesc& desc = {}) override;
    [[nodiscard]] Result<scope<PipelineLayout>> CreatePipelineLayout(const PipelineLayoutDesc& desc = {}) override;
    [[nodiscard]] Result<scope<QuerySet>> CreateQuerySet(const QuerySetDesc& desc = {}) override;
    [[nodiscard]] Result<scope<RenderBundleEncoder>> CreateRenderBundleEncoder(const RenderBundleEncoderDesc& desc = {}) override;
    [[nodiscard]] Result<scope<RenderPipeline>> CreateRenderPipeline(const RenderPipelineDesc& desc = {}) override;
    [[nodiscard]] Result<scope<RenderPipeline>> CreateRenderPipeline(const RenderPipelineDescTyped& desc) override;
    [[nodiscard]] Future CreateRenderPipelineAsync(const RenderPipelineDesc& desc, CallbackMode callback_mode, CreateRenderPipelineCallback callback) override;
    [[nodiscard]] Result<scope<ResourceTable>> CreateResourceTable(const ResourceTableDesc& desc = {}) override;
    [[nodiscard]] Result<scope<Sampler>> CreateSampler(const SamplerDesc& desc = {}) override;
    [[nodiscard]] Result<scope<ShaderModule>> CreateShaderModule(const ShaderModuleDesc& desc = {}) override;
    [[nodiscard]] Result<scope<Texture>> CreateTexture(const TextureDesc& desc = {}) override;

    [[nodiscard]] Result<scope<Swapchain>> CreateSwapchain(ref<Surface> surface, SwapchainDesc desc = {}) override;

    void Destroy() override;
    void ForceLoss(DeviceLostReason reason, std::string_view message) override;

    [[nodiscard]] Result<scope<Adapter>> GetAdapter() const override;
    [[nodiscard]] Result<void> GetAdapterInfo(AdapterInfo& info) const override;
    [[nodiscard]] Result<void> GetAHardwareBufferProperties(void* handle, void* properties) const override;

    void GetFeatures(SupportedFeatures& features) const override;
    [[nodiscard]] Result<void> GetLimits(Limits& limits) const override;
    [[nodiscard]] Future GetLostFuture() const override;
    [[nodiscard]] Queue& GetQueue() const noexcept override;
    [[nodiscard]] bool HasFeature(FeatureName feature) const noexcept override;
    [[nodiscard]] const DeviceCapabilities& Capabilities() const noexcept override;
    [[nodiscard]] bool IsLost() const noexcept override;
    [[nodiscard]] DeviceLostReason LossReason() const noexcept override;

    [[nodiscard]] Result<scope<SharedBufferMemory>> ImportSharedBufferMemory(const SharedBufferMemoryDesc& desc = {}) override;
    [[nodiscard]] Result<scope<SharedFence>> ImportSharedFence(const SharedFenceDesc& desc = {}) override;
    [[nodiscard]] Result<scope<SharedTextureMemory>> ImportSharedTextureMemory(const SharedTextureMemoryDesc& desc = {}) override;

    void InjectError(ErrorType type, std::string_view message) override;
    [[nodiscard]] Future PopErrorScope(CallbackMode callback_mode, PopErrorScopeCallback callback) const override;

    void PushErrorScope(ErrorFilter filter) override;
    void SetLabel(std::string_view label) override;
    void SetLoggingCallback(LoggingCallback callback) override;
    void Tick() const noexcept override;
    void ValidateTextureDescriptor(const TextureDesc& desc) const override;

    [[nodiscard]] NativeHandles GetNativeHandles() const noexcept override;
    [[nodiscard]] WGPUDevice GetNativeDevice() const noexcept;

private:
    detail::InstanceHandle instance_;
    detail::AdapterHandle adapter_handle_;
    ref<DeviceLostCallback> device_lost_callback_{};
    ref<UncapturedErrorCallback> uncaptured_error_callback_{};
    ref<LoggingCallback> logging_callback_;
    detail::DeviceHandle device_;
    mutable WgpuQueueImpl queue_;
    DeviceCapabilities capabilities_{};
    ref<WgpuDeviceLossState> loss_state_;
};

} // namespace woki::rhi::wgpu
