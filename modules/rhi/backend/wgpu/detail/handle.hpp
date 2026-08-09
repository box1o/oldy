#pragma once

#include <utility>
#include <webgpu/webgpu.h>

namespace woki::rhi::wgpu::detail {

template <typename Handle, void (*AddRefFn)(Handle), void (*ReleaseFn)(Handle)>
class GpuHandle {
public:
    GpuHandle() = default;

    explicit GpuHandle(Handle handle) noexcept
        : handle_(handle) {}

    [[nodiscard]] static GpuHandle Retain(Handle handle) noexcept {
        if (handle != nullptr) {
            AddRefFn(handle);
        }
        return GpuHandle(handle);
    }

    ~GpuHandle() {
        reset();
    }

    GpuHandle(GpuHandle&& other) noexcept
        : handle_(other.release()) {}

    GpuHandle(const GpuHandle& other) noexcept
        : GpuHandle(Retain(other.handle_)) {}

    GpuHandle& operator=(GpuHandle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    GpuHandle& operator=(const GpuHandle& other) noexcept {
        if (this != &other) {
            GpuHandle copy(other);
            swap(copy);
        }
        return *this;
    }

    [[nodiscard]] Handle get() const noexcept {
        return handle_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return handle_ != nullptr;
    }

    void reset(Handle handle = nullptr) noexcept {
        if (handle_ != nullptr) {
            ReleaseFn(handle_);
        }
        handle_ = handle;
    }

    [[nodiscard]] Handle release() noexcept {
        return std::exchange(handle_, nullptr);
    }

    void swap(GpuHandle& other) noexcept {
        std::swap(handle_, other.handle_);
    }

private:
    Handle handle_{nullptr};
};

using InstanceHandle = GpuHandle<WGPUInstance, wgpuInstanceAddRef, wgpuInstanceRelease>;
using SurfaceHandle = GpuHandle<WGPUSurface, wgpuSurfaceAddRef, wgpuSurfaceRelease>;
using AdapterHandle = GpuHandle<WGPUAdapter, wgpuAdapterAddRef, wgpuAdapterRelease>;
using TextureHandle = GpuHandle<WGPUTexture, wgpuTextureAddRef, wgpuTextureRelease>;
using TextureViewHandle = GpuHandle<WGPUTextureView, wgpuTextureViewAddRef, wgpuTextureViewRelease>;
using DeviceHandle = GpuHandle<WGPUDevice, wgpuDeviceAddRef, wgpuDeviceRelease>;
using QueueHandle = GpuHandle<WGPUQueue, wgpuQueueAddRef, wgpuQueueRelease>;
using BufferHandle = GpuHandle<WGPUBuffer, wgpuBufferAddRef, wgpuBufferRelease>;
using CommandBufferHandle = GpuHandle<WGPUCommandBuffer, wgpuCommandBufferAddRef, wgpuCommandBufferRelease>;
using CommandEncoderHandle = GpuHandle<WGPUCommandEncoder, wgpuCommandEncoderAddRef, wgpuCommandEncoderRelease>;
using ComputePassEncoderHandle = GpuHandle<WGPUComputePassEncoder, wgpuComputePassEncoderAddRef, wgpuComputePassEncoderRelease>;
using RenderPassEncoderHandle = GpuHandle<WGPURenderPassEncoder, wgpuRenderPassEncoderAddRef, wgpuRenderPassEncoderRelease>;
using TexelBufferViewHandle = GpuHandle<WGPUTexelBufferView, wgpuTexelBufferViewAddRef, wgpuTexelBufferViewRelease>;
using RenderBundleHandle = GpuHandle<WGPURenderBundle, wgpuRenderBundleAddRef, wgpuRenderBundleRelease>;
using BindGroupHandle = GpuHandle<WGPUBindGroup, wgpuBindGroupAddRef, wgpuBindGroupRelease>;
using BindGroupLayoutHandle = GpuHandle<WGPUBindGroupLayout, wgpuBindGroupLayoutAddRef, wgpuBindGroupLayoutRelease>;
using ComputePipelineHandle = GpuHandle<WGPUComputePipeline, wgpuComputePipelineAddRef, wgpuComputePipelineRelease>;
using ExternalTextureHandle = GpuHandle<WGPUExternalTexture, wgpuExternalTextureAddRef, wgpuExternalTextureRelease>;
using PipelineLayoutHandle = GpuHandle<WGPUPipelineLayout, wgpuPipelineLayoutAddRef, wgpuPipelineLayoutRelease>;
using QuerySetHandle = GpuHandle<WGPUQuerySet, wgpuQuerySetAddRef, wgpuQuerySetRelease>;
using RenderBundleEncoderHandle = GpuHandle<WGPURenderBundleEncoder, wgpuRenderBundleEncoderAddRef, wgpuRenderBundleEncoderRelease>;
using RenderPipelineHandle = GpuHandle<WGPURenderPipeline, wgpuRenderPipelineAddRef, wgpuRenderPipelineRelease>;
using ResourceTableHandle = GpuHandle<WGPUResourceTable, wgpuResourceTableAddRef, wgpuResourceTableRelease>;
using SamplerHandle = GpuHandle<WGPUSampler, wgpuSamplerAddRef, wgpuSamplerRelease>;
using ShaderModuleHandle = GpuHandle<WGPUShaderModule, wgpuShaderModuleAddRef, wgpuShaderModuleRelease>;
using SharedBufferMemoryHandle = GpuHandle<WGPUSharedBufferMemory, wgpuSharedBufferMemoryAddRef, wgpuSharedBufferMemoryRelease>;
using SharedFenceHandle = GpuHandle<WGPUSharedFence, wgpuSharedFenceAddRef, wgpuSharedFenceRelease>;
using SharedTextureMemoryHandle = GpuHandle<WGPUSharedTextureMemory, wgpuSharedTextureMemoryAddRef, wgpuSharedTextureMemoryRelease>;

} // namespace woki::rhi::wgpu::detail
