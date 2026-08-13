#pragma once

#include <string>
#include <vector>
#include <webgpu/webgpu.h>

#include <woki/rhi/descriptors.hpp>

#include "string.hpp"
#include "../wgpu_enums.hpp"
#include "constant_entry.hpp"
#include "native_helpers.hpp"

namespace woki::rhi::wgpu::detail {

using convert::ToWgpu;
using convert::ToWgpuColorWriteMask;

[[nodiscard]] inline WGPUVertexAttribute ToWgpu(const VertexAttributeDesc& desc) noexcept {
    WGPUVertexAttribute native = WGPU_VERTEX_ATTRIBUTE_INIT;
    native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
    native.format = ToWgpu(desc.format);
    native.offset = desc.offset;
    native.shaderLocation = desc.shader_location;
    return native;
}

[[nodiscard]] inline WGPUOptionalBool ToWgpuOptionalBool(const std::optional<bool>& value) noexcept {
    if (!value.has_value()) {
        return WGPUOptionalBool_Undefined;
    }
    return *value ? WGPUOptionalBool_True : WGPUOptionalBool_False;
}

struct VertexBufferLayoutStorage final {
    std::vector<WGPUVertexAttribute> attributes{};
    WGPUVertexBufferLayout native = WGPU_VERTEX_BUFFER_LAYOUT_INIT;

    explicit VertexBufferLayoutStorage(const VertexBufferLayoutDesc& desc) {
        attributes.reserve(desc.attributes.size());
        for (const VertexAttributeDesc& attribute : desc.attributes) {
            attributes.push_back(ToWgpu(attribute));
        }

        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.stepMode = ToWgpu(desc.step_mode);
        native.arrayStride = desc.array_stride;
        native.attributeCount = attributes.size();
        native.attributes = attributes.empty() ? nullptr : attributes.data();
    }
};

struct VertexStateStorage final {
    std::vector<VertexBufferLayoutStorage> buffer_storage{};
    std::vector<WGPUVertexBufferLayout> buffers{};
    std::string entry_point{};
    ConstantEntryStorage constants;
    WGPUVertexState native = WGPU_VERTEX_STATE_INIT;

    explicit VertexStateStorage(const VertexStateDesc& desc)
        : constants(desc.constants) {
        buffer_storage.reserve(desc.buffers.size());
        buffers.reserve(desc.buffers.size());
        for (const VertexBufferLayoutDesc& buffer : desc.buffers) {
            buffer_storage.emplace_back(buffer);
            buffers.push_back(buffer_storage.back().native);
        }

        entry_point = desc.entry_point;
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.module = NativeShaderModule(desc.module);
        native.entryPoint = ToStringView(entry_point);
        native.constantCount = constants.entries.size();
        native.constants = constants.entries.empty() ? nullptr : constants.entries.data();
        native.bufferCount = buffers.size();
        native.buffers = buffers.empty() ? nullptr : buffers.data();
    }
};

struct ColorTargetStateStorage final {
    std::optional<BlendStateDesc> blend_storage{};
    WGPUBlendState native_blend = WGPU_BLEND_STATE_INIT;
    WGPUColorTargetState native = WGPU_COLOR_TARGET_STATE_INIT;

    explicit ColorTargetStateStorage(const ColorTargetStateDesc& desc) {
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.format = ToWgpu(desc.format);
        if (desc.blend != nullptr) {
            blend_storage = *desc.blend;
            native_blend.color.operation = ToWgpu(blend_storage->color.operation);
            native_blend.color.srcFactor = ToWgpu(blend_storage->color.src_factor);
            native_blend.color.dstFactor = ToWgpu(blend_storage->color.dst_factor);
            native_blend.alpha.operation = ToWgpu(blend_storage->alpha.operation);
            native_blend.alpha.srcFactor = ToWgpu(blend_storage->alpha.src_factor);
            native_blend.alpha.dstFactor = ToWgpu(blend_storage->alpha.dst_factor);
            native.blend = &native_blend;
        }
        native.writeMask = ToWgpuColorWriteMask(desc.write_mask);
    }
};

struct FragmentStateStorage final {
    std::vector<ColorTargetStateStorage> target_storage{};
    std::vector<WGPUColorTargetState> targets{};
    std::string entry_point{};
    ConstantEntryStorage constants;
    WGPUFragmentState native = WGPU_FRAGMENT_STATE_INIT;

    explicit FragmentStateStorage(const FragmentStateDesc& desc)
        : constants(desc.constants) {
        target_storage.reserve(desc.targets.size());
        targets.reserve(desc.targets.size());
        for (const ColorTargetStateDesc& target : desc.targets) {
            target_storage.emplace_back(target);
            targets.push_back(target_storage.back().native);
        }

        entry_point = desc.entry_point;
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.module = NativeShaderModule(desc.module);
        native.entryPoint = ToStringView(entry_point);
        native.constantCount = constants.entries.size();
        native.constants = constants.entries.empty() ? nullptr : constants.entries.data();
        native.targetCount = targets.size();
        native.targets = targets.empty() ? nullptr : targets.data();
    }
};

struct PrimitiveStateStorage final {
    WGPUPrimitiveState native = WGPU_PRIMITIVE_STATE_INIT;

    explicit PrimitiveStateStorage(const PrimitiveStateDesc& desc) {
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.topology = ToWgpu(desc.topology);
        native.stripIndexFormat = ToWgpu(desc.strip_index_format);
        native.frontFace = ToWgpu(desc.front_face);
        native.cullMode = ToWgpu(desc.cull_mode);
        native.unclippedDepth = desc.unclipped_depth ? WGPU_TRUE : WGPU_FALSE;
    }
};

struct DepthStencilStateStorage final {
    WGPUDepthStencilState native = WGPU_DEPTH_STENCIL_STATE_INIT;

    explicit DepthStencilStateStorage(const DepthStencilStateDesc& desc) {
        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.format = ToWgpu(desc.format);
        native.depthWriteEnabled = ToWgpuOptionalBool(desc.depth_write_enabled);
        native.depthCompare = ToWgpu(desc.depth_compare);
        native.stencilFront.compare = ToWgpu(desc.stencil_front.compare);
        native.stencilFront.failOp = ToWgpu(desc.stencil_front.fail_op);
        native.stencilFront.depthFailOp = ToWgpu(desc.stencil_front.depth_fail_op);
        native.stencilFront.passOp = ToWgpu(desc.stencil_front.pass_op);
        native.stencilBack.compare = ToWgpu(desc.stencil_back.compare);
        native.stencilBack.failOp = ToWgpu(desc.stencil_back.fail_op);
        native.stencilBack.depthFailOp = ToWgpu(desc.stencil_back.depth_fail_op);
        native.stencilBack.passOp = ToWgpu(desc.stencil_back.pass_op);
        native.stencilReadMask = desc.stencil_read_mask;
        native.stencilWriteMask = desc.stencil_write_mask;
        native.depthBias = desc.depth_bias;
        native.depthBiasSlopeScale = desc.depth_bias_slope_scale;
        native.depthBiasClamp = desc.depth_bias_clamp;
    }
};

[[nodiscard]] inline WGPUMultisampleState ToWgpu(const MultisampleStateDesc& desc) noexcept {
    WGPUMultisampleState native = WGPU_MULTISAMPLE_STATE_INIT;
    native.count = desc.count;
    native.mask = desc.mask;
    native.alphaToCoverageEnabled = desc.alpha_to_coverage_enabled ? WGPU_TRUE : WGPU_FALSE;
    return native;
}

struct RenderPipelineTypedDescriptorStorage final {
    std::optional<VertexStateStorage> vertex{};
    std::optional<PrimitiveStateStorage> primitive{};
    std::optional<DepthStencilStateStorage> depth_stencil{};
    std::optional<FragmentStateStorage> fragment{};
    WGPUMultisampleState multisample = WGPU_MULTISAMPLE_STATE_INIT;
    WGPURenderPipelineDescriptor native = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;

    explicit RenderPipelineTypedDescriptorStorage(const RenderPipelineDescTyped& desc) {
        if (desc.vertex != nullptr) {
            vertex.emplace(*desc.vertex);
            native.vertex = vertex->native;
        }
        if (desc.primitive != nullptr) {
            primitive.emplace(*desc.primitive);
            native.primitive = primitive->native;
        }
        if (desc.depth_stencil != nullptr) {
            depth_stencil.emplace(*desc.depth_stencil);
            native.depthStencil = &depth_stencil->native;
        }
        multisample = ToWgpu(desc.multisample);
        native.multisample = multisample;
        if (desc.fragment != nullptr) {
            fragment.emplace(*desc.fragment);
            native.fragment = &fragment->native;
        }

        native.nextInChain = static_cast<WGPUChainedStruct*>(desc.next_in_chain);
        native.label = ToStringView(desc.label);
        native.layout = NativePipelineLayout(desc.layout);
    }
};

} // namespace woki::rhi::wgpu::detail
