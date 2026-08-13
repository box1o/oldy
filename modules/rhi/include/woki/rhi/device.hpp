#pragma once

#include <string_view>

#include <woki/core.hpp>

#include "queue.hpp"
#include "types.hpp"
#include "adapter.hpp"
#include "forward.hpp"
#include "descriptors.hpp"
#include "swapchain_desc.hpp"
#include "capabilities.hpp"

namespace woki::rhi {

class Device {
public:
    virtual ~Device() = default;

    [[nodiscard]] virtual Result<scope<BindGroup>> CreateBindGroup(const BindGroupDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<BindGroupLayout>> CreateBindGroupLayout(const BindGroupLayoutDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<Buffer>> CreateBuffer(const BufferDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<CommandEncoder>> CreateCommandEncoder(const CommandEncoderDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<ComputePipeline>> CreateComputePipeline(const ComputePipelineDesc& desc = {}) = 0;
    [[nodiscard]] virtual Future CreateComputePipelineAsync(const ComputePipelineDesc& desc, CallbackMode callback_mode, CreateComputePipelineCallback callback) = 0;
    [[nodiscard]] virtual Result<scope<Buffer>> CreateErrorBuffer(const BufferDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<ExternalTexture>> CreateErrorExternalTexture() = 0;
    [[nodiscard]] virtual Result<scope<ShaderModule>> CreateErrorShaderModule(const ShaderModuleDesc& desc, std::string_view error_message) = 0;
    [[nodiscard]] virtual Result<scope<Texture>> CreateErrorTexture(const TextureDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<ExternalTexture>> CreateExternalTexture(const ExternalTextureDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<PipelineLayout>> CreatePipelineLayout(const PipelineLayoutDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<QuerySet>> CreateQuerySet(const QuerySetDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<RenderBundleEncoder>> CreateRenderBundleEncoder(const RenderBundleEncoderDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<RenderPipeline>> CreateRenderPipeline(const RenderPipelineDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<RenderPipeline>> CreateRenderPipeline(const RenderPipelineDescTyped& desc) = 0;
    [[nodiscard]] virtual Future CreateRenderPipelineAsync(const RenderPipelineDesc& desc, CallbackMode callback_mode, CreateRenderPipelineCallback callback) = 0;
    [[nodiscard]] virtual Result<scope<ResourceTable>> CreateResourceTable(const ResourceTableDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<Sampler>> CreateSampler(const SamplerDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<ShaderModule>> CreateShaderModule(const ShaderModuleDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<Texture>> CreateTexture(const TextureDesc& desc = {}) = 0;

    [[nodiscard]] virtual Result<scope<Swapchain>> CreateSwapchain(ref<Surface> surface, SwapchainDesc desc = {}) = 0;

    virtual void Destroy() = 0;
    virtual void ForceLoss(DeviceLostReason reason, std::string_view message) = 0;

    [[nodiscard]] virtual Result<scope<Adapter>> GetAdapter() const = 0;

    [[nodiscard]] virtual Result<void> GetAdapterInfo(AdapterInfo& info) const = 0;
    [[nodiscard]] AdapterInfo GetAdapterInfo() const;

    [[nodiscard]] virtual Result<void> GetAHardwareBufferProperties(void* handle, void* properties) const = 0;

    virtual void GetFeatures(SupportedFeatures& features) const = 0;
    [[nodiscard]] SupportedFeatures GetFeatures() const;

    [[nodiscard]] virtual Result<void> GetLimits(Limits& limits) const = 0;
    [[nodiscard]] Limits GetLimits() const;

    [[nodiscard]] virtual Future GetLostFuture() const = 0;
    [[nodiscard]] virtual Queue& GetQueue() const noexcept = 0;
    [[nodiscard]] virtual bool HasFeature(FeatureName feature) const noexcept = 0;
    [[nodiscard]] virtual const DeviceCapabilities& Capabilities() const noexcept;

    [[nodiscard]] virtual bool IsLost() const noexcept {
        return false;
    }

    [[nodiscard]] virtual DeviceLostReason LossReason() const noexcept {
        return DeviceLostReason::Unknown;
    }

    [[nodiscard]] virtual Result<scope<SharedBufferMemory>> ImportSharedBufferMemory(const SharedBufferMemoryDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<SharedFence>> ImportSharedFence(const SharedFenceDesc& desc = {}) = 0;
    [[nodiscard]] virtual Result<scope<SharedTextureMemory>> ImportSharedTextureMemory(const SharedTextureMemoryDesc& desc = {}) = 0;

    virtual void InjectError(ErrorType type, std::string_view message) = 0;

    [[nodiscard]] virtual Future PopErrorScope(CallbackMode callback_mode, PopErrorScopeCallback callback) const = 0;

    virtual void PushErrorScope(ErrorFilter filter) = 0;
    virtual void SetLabel(std::string_view label) = 0;
    virtual void SetLoggingCallback(LoggingCallback callback) = 0;
    virtual void Tick() const noexcept = 0;

    virtual void ValidateTextureDescriptor(const TextureDesc& desc) const = 0;

    [[nodiscard]] virtual NativeHandles GetNativeHandles() const noexcept = 0;

protected:
    Device() = default;
};

inline AdapterInfo Device::GetAdapterInfo() const {
    AdapterInfo info{};
    (void)GetAdapterInfo(info);
    return info;
}

inline SupportedFeatures Device::GetFeatures() const {
    SupportedFeatures features{};
    GetFeatures(features);
    return features;
}

inline Limits Device::GetLimits() const {
    Limits limits{};
    (void)GetLimits(limits);
    return limits;
}

} // namespace woki::rhi
