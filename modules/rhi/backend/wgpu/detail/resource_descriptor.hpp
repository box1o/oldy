#pragma once

#include <span>
#include <string>
#include <vector>
#include <webgpu/webgpu.h>

#include <woki/rhi/objects.hpp>
#include <woki/rhi/descriptors.hpp>

#include "string.hpp"
#include "copy_convert.hpp"
#include "native_helpers.hpp"
#include "bind_group_descriptor.hpp"
#include "external_texture_descriptor.hpp"

namespace woki::rhi::wgpu::detail {

using convert::ToWgpu;

struct ShaderModuleDescriptorStorage final {
    std::string code{};
    WGPUShaderSourceWGSL wgsl_source = WGPU_SHADER_SOURCE_WGSL_INIT;
    WGPUShaderModuleDescriptor native = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;

    explicit ShaderModuleDescriptorStorage(const ShaderModuleDesc& desc) {
        native.label = ToStringView(desc.label);
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);

        if (!desc.code.empty()) {
            code = desc.code;
            wgsl_source = WGPU_SHADER_SOURCE_WGSL_INIT;
            wgsl_source.chain.sType = WGPUSType_ShaderSourceWGSL;
            wgsl_source.chain.next = native.nextInChain;
            wgsl_source.code = ToStringView(code);
            native.nextInChain = &wgsl_source.chain;
        }
    }
};

struct TextureDescriptorStorage final {
    std::vector<WGPUTextureFormat> view_formats{};
    WGPUTextureDescriptor native = WGPU_TEXTURE_DESCRIPTOR_INIT;

    explicit TextureDescriptorStorage(const TextureDesc& desc) {
        view_formats.reserve(desc.view_formats.size());
        for (const TextureFormat format : desc.view_formats) {
            view_formats.push_back(ToWgpu(format));
        }

        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.usage = TextureUsageToWgpu(desc.usage);
        native.dimension = ToWgpu(desc.dimension);
        native.size = copy::ToWgpu(desc.size);
        native.format = ToWgpu(desc.format);
        native.mipLevelCount = desc.mip_level_count;
        native.sampleCount = desc.sample_count;
        native.viewFormatCount = view_formats.size();
        native.viewFormats = view_formats.empty() ? nullptr : view_formats.data();
    }
};

struct BufferDescriptorStorage final {
    WGPUBufferDescriptor native = WGPU_BUFFER_DESCRIPTOR_INIT;

    explicit BufferDescriptorStorage(const BufferDesc& desc) {
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.usage = static_cast<WGPUBufferUsage>(static_cast<u64>(desc.usage));
        native.size = desc.size;
        native.mappedAtCreation = desc.mapped_at_creation ? WGPU_TRUE : WGPU_FALSE;
    }
};

struct TextureViewDescriptorStorage final {
    WGPUTextureViewDescriptor native = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;

    explicit TextureViewDescriptorStorage(const TextureViewDesc& desc) {
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.format = ToWgpu(desc.format);
        native.dimension = ToWgpu(desc.dimension);
        native.baseMipLevel = desc.base_mip_level;
        native.mipLevelCount = desc.mip_level_count;
        native.baseArrayLayer = desc.base_array_layer;
        native.arrayLayerCount = desc.array_layer_count;
        native.aspect = ToWgpu(desc.aspect);
        native.usage = static_cast<WGPUTextureUsage>(static_cast<u64>(desc.usage));
    }
};

struct SamplerDescriptorStorage final {
    WGPUSamplerDescriptor native = WGPU_SAMPLER_DESCRIPTOR_INIT;

    explicit SamplerDescriptorStorage(const SamplerDesc& desc) {
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.addressModeU = ToWgpu(desc.address_mode_u);
        native.addressModeV = ToWgpu(desc.address_mode_v);
        native.addressModeW = ToWgpu(desc.address_mode_w);
        native.magFilter = ToWgpu(desc.mag_filter);
        native.minFilter = ToWgpu(desc.min_filter);
        native.mipmapFilter = ToWgpu(desc.mipmap_filter);
        native.lodMinClamp = desc.lod_min_clamp;
        native.lodMaxClamp = desc.lod_max_clamp;
        native.compare = ToWgpu(desc.compare);
        native.maxAnisotropy = desc.max_anisotropy;
    }
};

struct QuerySetDescriptorStorage final {
    WGPUQuerySetDescriptor native = WGPU_QUERY_SET_DESCRIPTOR_INIT;

    explicit QuerySetDescriptorStorage(const QuerySetDesc& desc) {
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.type = ToWgpu(desc.type);
        native.count = desc.count;
    }
};

struct PipelineLayoutDescriptorStorage final {
    std::vector<WGPUBindGroupLayout> bind_group_layouts{};
    WGPUPipelineLayoutDescriptor native = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;

    explicit PipelineLayoutDescriptorStorage(const PipelineLayoutDesc& desc) {
        bind_group_layouts.reserve(desc.bind_group_layouts.size());
        for (BindGroupLayout* layout : desc.bind_group_layouts) {
            bind_group_layouts.push_back(NativeBindGroupLayout(layout));
        }

        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.bindGroupLayoutCount = bind_group_layouts.size();
        native.bindGroupLayouts = bind_group_layouts.empty() ? nullptr : bind_group_layouts.data();
        native.immediateSize = desc.immediate_size;
    }
};

struct ComputeStateStorage final {
    WGPUComputeState native = WGPU_COMPUTE_STATE_INIT;

    explicit ComputeStateStorage(const ComputeStateDesc& desc) {
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.module = NativeShaderModule(desc.module);
        native.entryPoint = ToStringView(desc.entry_point);
        native.constantCount = desc.constant_count;
        native.constants = static_cast<const WGPUConstantEntry*>(desc.constants);
    }
};

struct TexelBufferViewDescriptorStorage final {
    WGPUTexelBufferViewDescriptor native = WGPU_TEXEL_BUFFER_VIEW_DESCRIPTOR_INIT;

    explicit TexelBufferViewDescriptorStorage(const TexelBufferViewDesc& desc) {
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.format = ToWgpu(desc.format);
        native.offset = desc.offset;
        native.size = desc.size;
    }
};

struct ComputePipelineDescriptorStorage final {
    ComputeStateStorage compute;
    WGPUComputePipelineDescriptor native = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;

    explicit ComputePipelineDescriptorStorage(const ComputePipelineDesc& desc)
        : compute(desc.compute) {
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.layout = NativePipelineLayout(desc.layout);
        native.compute = compute.native;
    }
};

struct RenderPipelineDescriptorStorage final {
    WGPURenderPipelineDescriptor native = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;

    explicit RenderPipelineDescriptorStorage(const RenderPipelineDesc& desc) {
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.layout = NativePipelineLayout(desc.layout);
        if (desc.vertex != nullptr) {
            native.vertex = *static_cast<const WGPUVertexState*>(desc.vertex);
        }
        if (desc.primitive != nullptr) {
            native.primitive = *static_cast<const WGPUPrimitiveState*>(desc.primitive);
        }
        if (desc.depth_stencil != nullptr) {
            native.depthStencil = static_cast<const WGPUDepthStencilState*>(desc.depth_stencil);
        }
        if (desc.multisample != nullptr) {
            native.multisample = *static_cast<const WGPUMultisampleState*>(desc.multisample);
        }
        if (desc.fragment != nullptr) {
            native.fragment = static_cast<const WGPUFragmentState*>(desc.fragment);
        }
    }
};

struct RenderBundleEncoderDescriptorStorage final {
    std::vector<WGPUTextureFormat> color_formats{};
    WGPURenderBundleEncoderDescriptor native = WGPU_RENDER_BUNDLE_ENCODER_DESCRIPTOR_INIT;

    explicit RenderBundleEncoderDescriptorStorage(const RenderBundleEncoderDesc& desc) {
        color_formats.reserve(desc.color_formats.size());
        for (const TextureFormat format : desc.color_formats) {
            color_formats.push_back(ToWgpu(format));
        }

        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.colorFormatCount = color_formats.size();
        native.colorFormats = color_formats.empty() ? nullptr : color_formats.data();
        native.depthStencilFormat = ToWgpu(desc.depth_stencil_format);
        native.sampleCount = desc.sample_count;
        native.depthReadOnly = desc.depth_read_only;
        native.stencilReadOnly = desc.stencil_read_only;
    }
};

struct CommandEncoderDescriptorStorage final {
    WGPUCommandEncoderDescriptor native = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;

    explicit CommandEncoderDescriptorStorage(const CommandEncoderDesc& desc) {
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
    }
};

struct ResourceTableDescriptorStorage final {
    WGPUResourceTableDescriptor native = WGPU_RESOURCE_TABLE_DESCRIPTOR_INIT;

    explicit ResourceTableDescriptorStorage(const ResourceTableDesc& desc) {
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.size = desc.size;
    }
};

} // namespace woki::rhi::wgpu::detail
