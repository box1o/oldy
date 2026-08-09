#pragma once

#include <webgpu/webgpu.h>

#include <woki/rhi/objects.hpp>

#include "detail/handle.hpp"

namespace woki::rhi::wgpu {

[[nodiscard]] scope<BindGroup> CreateBindGroupObject(WGPUBindGroup handle);
[[nodiscard]] scope<BindGroupLayout> CreateBindGroupLayoutObject(WGPUBindGroupLayout handle);
[[nodiscard]] scope<Buffer> CreateBufferObject(WGPUBuffer handle);
[[nodiscard]] scope<ComputePipeline> CreateComputePipelineObject(WGPUComputePipeline handle);
[[nodiscard]] scope<ExternalTexture> CreateExternalTextureObject(WGPUExternalTexture handle);
[[nodiscard]] scope<PipelineLayout> CreatePipelineLayoutObject(WGPUPipelineLayout handle);
[[nodiscard]] scope<QuerySet> CreateQuerySetObject(WGPUQuerySet handle);
[[nodiscard]] scope<RenderBundleEncoder> CreateRenderBundleEncoderObject(WGPURenderBundleEncoder handle);
[[nodiscard]] scope<RenderPipeline> CreateRenderPipelineObject(WGPURenderPipeline handle);
[[nodiscard]] scope<ResourceTable> CreateResourceTableObject(WGPUResourceTable handle);
[[nodiscard]] scope<Sampler> CreateSamplerObject(WGPUSampler handle);
[[nodiscard]] scope<ShaderModule> CreateShaderModuleObject(WGPUShaderModule handle);
[[nodiscard]] scope<SharedBufferMemory> CreateSharedBufferMemoryObject(WGPUSharedBufferMemory handle);
[[nodiscard]] scope<SharedFence> CreateSharedFenceObject(WGPUSharedFence handle);
[[nodiscard]] scope<SharedTextureMemory> CreateSharedTextureMemoryObject(WGPUSharedTextureMemory handle);
[[nodiscard]] scope<Texture> CreateTextureObject(WGPUTexture handle);
[[nodiscard]] scope<TextureView> CreateTextureViewObject(WGPUTextureView handle);

} // namespace woki::rhi::wgpu
