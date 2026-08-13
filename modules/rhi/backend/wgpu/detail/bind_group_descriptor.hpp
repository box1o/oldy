#pragma once

#include <vector>
#include <webgpu/webgpu.h>

#include <woki/rhi/descriptors.hpp>

#include "string.hpp"
#include "../wgpu_enums.hpp"
#include "native_helpers.hpp"

namespace woki::rhi::wgpu::detail {

using convert::ToWgpu;

[[nodiscard]] inline WGPUBufferBindingLayout ToWgpu(const BufferBindingLayoutDesc& desc) noexcept {
    WGPUBufferBindingLayout native = WGPU_BUFFER_BINDING_LAYOUT_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    native.type = ToWgpu(desc.type);
    native.hasDynamicOffset = desc.has_dynamic_offset ? WGPU_TRUE : WGPU_FALSE;
    native.minBindingSize = desc.min_binding_size;
    return native;
}

[[nodiscard]] inline WGPUSamplerBindingLayout ToWgpu(const SamplerBindingLayoutDesc& desc) noexcept {
    WGPUSamplerBindingLayout native = WGPU_SAMPLER_BINDING_LAYOUT_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    native.type = ToWgpu(desc.type);
    return native;
}

[[nodiscard]] inline WGPUTextureBindingLayout ToWgpu(const TextureBindingLayoutDesc& desc) noexcept {
    WGPUTextureBindingLayout native = WGPU_TEXTURE_BINDING_LAYOUT_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    native.sampleType = ToWgpu(desc.sample_type);
    native.viewDimension = ToWgpu(desc.view_dimension);
    native.multisampled = desc.multisampled ? WGPU_TRUE : WGPU_FALSE;
    return native;
}

[[nodiscard]] inline WGPUStorageTextureBindingLayout ToWgpu(const StorageTextureBindingLayoutDesc& desc) noexcept {
    WGPUStorageTextureBindingLayout native = WGPU_STORAGE_TEXTURE_BINDING_LAYOUT_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    native.access = ToWgpu(desc.access);
    native.format = ToWgpu(desc.format);
    native.viewDimension = ToWgpu(desc.view_dimension);
    return native;
}

[[nodiscard]] inline WGPUBindGroupLayoutEntry ToWgpu(const BindGroupLayoutEntryDesc& desc) noexcept {
    WGPUBindGroupLayoutEntry native = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    native.binding = desc.binding;
    native.visibility = static_cast<WGPUShaderStage>(desc.visibility);
    native.bindingArraySize = desc.binding_array_size;
    native.buffer = ToWgpu(desc.buffer);
    native.sampler = ToWgpu(desc.sampler);
    native.texture = ToWgpu(desc.texture);
    native.storageTexture = ToWgpu(desc.storage_texture);
    return native;
}

[[nodiscard]] inline WGPUBindGroupEntry ToWgpu(const BindGroupEntryDesc& desc) noexcept {
    WGPUBindGroupEntry native = WGPU_BIND_GROUP_ENTRY_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    native.binding = desc.binding;
    native.buffer = desc.buffer == nullptr ? nullptr : NativeBuffer(*desc.buffer);
    native.offset = desc.offset;
    native.size = desc.size;
    native.sampler = NativeSampler(desc.sampler);
    native.textureView = NativeTextureView(desc.texture_view);
    return native;
}

#ifndef __EMSCRIPTEN__
[[nodiscard]] inline WGPUBindingResource ToWgpu(const BindingResourceDesc& desc) noexcept {
    WGPUBindingResource native = WGPU_BINDING_RESOURCE_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    native.buffer = desc.buffer == nullptr ? nullptr : NativeBuffer(*desc.buffer);
    native.offset = desc.offset;
    native.size = desc.size;
    native.sampler = NativeSampler(desc.sampler);
    native.textureView = NativeTextureView(desc.texture_view);
    return native;
}
#endif

struct BindGroupLayoutDescriptorStorage final {
    std::vector<WGPUBindGroupLayoutEntry> entries{};
    WGPUBindGroupLayoutDescriptor native = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;

    explicit BindGroupLayoutDescriptorStorage(const BindGroupLayoutDesc& desc) {
        entries.reserve(desc.entries.size());
        for (const BindGroupLayoutEntryDesc& entry : desc.entries) {
            entries.push_back(ToWgpu(entry));
        }

        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.entryCount = entries.size();
        native.entries = entries.empty() ? nullptr : entries.data();
    }
};

struct BindGroupDescriptorStorage final {
    std::vector<WGPUBindGroupEntry> entries{};
    WGPUBindGroupDescriptor native = WGPU_BIND_GROUP_DESCRIPTOR_INIT;

    explicit BindGroupDescriptorStorage(const BindGroupDesc& desc) {
        entries.reserve(desc.entries.size());
        for (const BindGroupEntryDesc& entry : desc.entries) {
            entries.push_back(ToWgpu(entry));
        }

        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.layout = NativeBindGroupLayout(desc.layout);
        native.entryCount = entries.size();
        native.entries = entries.empty() ? nullptr : entries.data();
    }
};

} // namespace woki::rhi::wgpu::detail
