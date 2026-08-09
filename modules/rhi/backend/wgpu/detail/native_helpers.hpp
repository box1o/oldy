#pragma once

#include <webgpu/webgpu.h>

#include <woki/rhi/objects.hpp>
#include <woki/rhi/command_buffer.hpp>

namespace woki::rhi::wgpu::detail {

[[nodiscard]] inline WGPUBuffer NativeBuffer(const Buffer& buffer) noexcept {
    return static_cast<WGPUBuffer>(buffer.GetNativeHandles().resource);
}

[[nodiscard]] inline WGPUCommandBuffer NativeCommandBuffer(const CommandBuffer& command_buffer) noexcept {
    return static_cast<WGPUCommandBuffer>(command_buffer.GetNativeHandles().resource);
}

[[nodiscard]] inline WGPUQuerySet NativeQuerySet(const QuerySet& query_set) noexcept {
    return static_cast<WGPUQuerySet>(query_set.GetNativeHandles().resource);
}

[[nodiscard]] inline WGPUBindGroup NativeBindGroup(const BindGroup* bind_group) noexcept {
    return bind_group == nullptr ? nullptr : static_cast<WGPUBindGroup>(bind_group->GetNativeHandles().resource);
}

[[nodiscard]] inline WGPUBindGroupLayout NativeBindGroupLayout(const BindGroupLayout* layout) noexcept {
    return layout == nullptr ? nullptr : static_cast<WGPUBindGroupLayout>(layout->GetNativeHandles().resource);
}

[[nodiscard]] inline WGPUPipelineLayout NativePipelineLayout(const PipelineLayout* layout) noexcept {
    return layout == nullptr ? nullptr : static_cast<WGPUPipelineLayout>(layout->GetNativeHandles().resource);
}

[[nodiscard]] inline WGPUShaderModule NativeShaderModule(const ShaderModule* module) noexcept {
    return module == nullptr ? nullptr : static_cast<WGPUShaderModule>(module->GetNativeHandles().resource);
}

[[nodiscard]] inline WGPUSampler NativeSampler(const Sampler* sampler) noexcept {
    return sampler == nullptr ? nullptr : static_cast<WGPUSampler>(sampler->GetNativeHandles().resource);
}

[[nodiscard]] inline WGPUTextureView NativeTextureView(const TextureView* view) noexcept {
    return view == nullptr ? nullptr : static_cast<WGPUTextureView>(view->GetNativeHandles().resource);
}

[[nodiscard]] inline WGPUComputePipeline NativeComputePipeline(const ComputePipeline& pipeline) noexcept {
    return static_cast<WGPUComputePipeline>(pipeline.GetNativeHandles().resource);
}

[[nodiscard]] inline WGPURenderPipeline NativeRenderPipeline(const RenderPipeline& pipeline) noexcept {
    return static_cast<WGPURenderPipeline>(pipeline.GetNativeHandles().resource);
}

[[nodiscard]] inline WGPUResourceTable NativeResourceTable(const ResourceTable* table) noexcept {
    return table == nullptr ? nullptr : static_cast<WGPUResourceTable>(table->GetNativeHandles().resource);
}

[[nodiscard]] inline WGPURenderBundle NativeRenderBundle(RenderBundle* bundle) noexcept {
    return bundle == nullptr ? nullptr : static_cast<WGPURenderBundle>(bundle->GetNativeHandles().resource);
}

} // namespace woki::rhi::wgpu::detail
