#pragma once

#include <webgpu/webgpu.h>

#include <woki/rhi/types.hpp>
#include <woki/rhi/descriptors.hpp>

#include "string.hpp"
#include "../wgpu_enums.hpp"

namespace woki::rhi::wgpu::detail {

using namespace woki::rhi::wgpu::convert;

inline void FillLimitsFromNative(const WGPULimits& native_limits, Limits& limits) {
    limits.max_texture_dimension_1d = native_limits.maxTextureDimension1D;
    limits.max_texture_dimension_2d = native_limits.maxTextureDimension2D;
    limits.max_texture_dimension_3d = native_limits.maxTextureDimension3D;
    limits.max_texture_array_layers = native_limits.maxTextureArrayLayers;
    limits.max_bind_groups = native_limits.maxBindGroups;
    limits.max_bind_groups_plus_vertex_buffers = native_limits.maxBindGroupsPlusVertexBuffers;
    limits.max_bindings_per_bind_group = native_limits.maxBindingsPerBindGroup;
    limits.max_dynamic_uniform_buffers_per_pipeline_layout = native_limits.maxDynamicUniformBuffersPerPipelineLayout;
    limits.max_dynamic_storage_buffers_per_pipeline_layout = native_limits.maxDynamicStorageBuffersPerPipelineLayout;
    limits.max_sampled_textures_per_shader_stage = native_limits.maxSampledTexturesPerShaderStage;
    limits.max_samplers_per_shader_stage = native_limits.maxSamplersPerShaderStage;
    limits.max_storage_buffers_per_shader_stage = native_limits.maxStorageBuffersPerShaderStage;
    limits.max_storage_textures_per_shader_stage = native_limits.maxStorageTexturesPerShaderStage;
    limits.max_uniform_buffers_per_shader_stage = native_limits.maxUniformBuffersPerShaderStage;
    limits.max_uniform_buffer_binding_size = native_limits.maxUniformBufferBindingSize;
    limits.max_storage_buffer_binding_size = native_limits.maxStorageBufferBindingSize;
    limits.min_uniform_buffer_offset_alignment = native_limits.minUniformBufferOffsetAlignment;
    limits.min_storage_buffer_offset_alignment = native_limits.minStorageBufferOffsetAlignment;
    limits.max_vertex_buffers = native_limits.maxVertexBuffers;
    limits.max_buffer_size = native_limits.maxBufferSize;
    limits.max_vertex_attributes = native_limits.maxVertexAttributes;
    limits.max_vertex_buffer_array_stride = native_limits.maxVertexBufferArrayStride;
    limits.max_inter_stage_shader_variables = native_limits.maxInterStageShaderVariables;
    limits.max_color_attachments = native_limits.maxColorAttachments;
    limits.max_color_attachment_bytes_per_sample = native_limits.maxColorAttachmentBytesPerSample;
    limits.max_compute_workgroup_storage_size = native_limits.maxComputeWorkgroupStorageSize;
    limits.max_compute_invocations_per_workgroup = native_limits.maxComputeInvocationsPerWorkgroup;
    limits.max_compute_workgroup_size_x = native_limits.maxComputeWorkgroupSizeX;
    limits.max_compute_workgroup_size_y = native_limits.maxComputeWorkgroupSizeY;
    limits.max_compute_workgroup_size_z = native_limits.maxComputeWorkgroupSizeZ;
    limits.max_compute_workgroups_per_dimension = native_limits.maxComputeWorkgroupsPerDimension;
    limits.max_immediate_size = native_limits.maxImmediateSize;
}

[[nodiscard]] inline WGPULimits ToWgpuLimits(const Limits& limits) {
    WGPULimits native_limits = WGPU_LIMITS_INIT;
    native_limits.nextInChain = static_cast<WGPUChainedStruct*>(limits.next_in_chain);

    if (limits.max_texture_dimension_1d != kLimitU32Undefined) {
        native_limits.maxTextureDimension1D = limits.max_texture_dimension_1d;
    }
    if (limits.max_texture_dimension_2d != kLimitU32Undefined) {
        native_limits.maxTextureDimension2D = limits.max_texture_dimension_2d;
    }
    if (limits.max_texture_dimension_3d != kLimitU32Undefined) {
        native_limits.maxTextureDimension3D = limits.max_texture_dimension_3d;
    }
    if (limits.max_texture_array_layers != kLimitU32Undefined) {
        native_limits.maxTextureArrayLayers = limits.max_texture_array_layers;
    }
    if (limits.max_bind_groups != kLimitU32Undefined) {
        native_limits.maxBindGroups = limits.max_bind_groups;
    }
    if (limits.max_bind_groups_plus_vertex_buffers != kLimitU32Undefined) {
        native_limits.maxBindGroupsPlusVertexBuffers = limits.max_bind_groups_plus_vertex_buffers;
    }
    if (limits.max_bindings_per_bind_group != kLimitU32Undefined) {
        native_limits.maxBindingsPerBindGroup = limits.max_bindings_per_bind_group;
    }
    if (limits.max_dynamic_uniform_buffers_per_pipeline_layout != kLimitU32Undefined) {
        native_limits.maxDynamicUniformBuffersPerPipelineLayout = limits.max_dynamic_uniform_buffers_per_pipeline_layout;
    }
    if (limits.max_dynamic_storage_buffers_per_pipeline_layout != kLimitU32Undefined) {
        native_limits.maxDynamicStorageBuffersPerPipelineLayout = limits.max_dynamic_storage_buffers_per_pipeline_layout;
    }
    if (limits.max_sampled_textures_per_shader_stage != kLimitU32Undefined) {
        native_limits.maxSampledTexturesPerShaderStage = limits.max_sampled_textures_per_shader_stage;
    }
    if (limits.max_samplers_per_shader_stage != kLimitU32Undefined) {
        native_limits.maxSamplersPerShaderStage = limits.max_samplers_per_shader_stage;
    }
    if (limits.max_storage_buffers_per_shader_stage != kLimitU32Undefined) {
        native_limits.maxStorageBuffersPerShaderStage = limits.max_storage_buffers_per_shader_stage;
    }
    if (limits.max_storage_textures_per_shader_stage != kLimitU32Undefined) {
        native_limits.maxStorageTexturesPerShaderStage = limits.max_storage_textures_per_shader_stage;
    }
    if (limits.max_uniform_buffers_per_shader_stage != kLimitU32Undefined) {
        native_limits.maxUniformBuffersPerShaderStage = limits.max_uniform_buffers_per_shader_stage;
    }
    if (limits.max_uniform_buffer_binding_size != kLimitU64Undefined) {
        native_limits.maxUniformBufferBindingSize = limits.max_uniform_buffer_binding_size;
    }
    if (limits.max_storage_buffer_binding_size != kLimitU64Undefined) {
        native_limits.maxStorageBufferBindingSize = limits.max_storage_buffer_binding_size;
    }
    if (limits.min_uniform_buffer_offset_alignment != kLimitU32Undefined) {
        native_limits.minUniformBufferOffsetAlignment = limits.min_uniform_buffer_offset_alignment;
    }
    if (limits.min_storage_buffer_offset_alignment != kLimitU32Undefined) {
        native_limits.minStorageBufferOffsetAlignment = limits.min_storage_buffer_offset_alignment;
    }
    if (limits.max_vertex_buffers != kLimitU32Undefined) {
        native_limits.maxVertexBuffers = limits.max_vertex_buffers;
    }
    if (limits.max_buffer_size != kLimitU64Undefined) {
        native_limits.maxBufferSize = limits.max_buffer_size;
    }
    if (limits.max_vertex_attributes != kLimitU32Undefined) {
        native_limits.maxVertexAttributes = limits.max_vertex_attributes;
    }
    if (limits.max_vertex_buffer_array_stride != kLimitU32Undefined) {
        native_limits.maxVertexBufferArrayStride = limits.max_vertex_buffer_array_stride;
    }
    if (limits.max_inter_stage_shader_variables != kLimitU32Undefined) {
        native_limits.maxInterStageShaderVariables = limits.max_inter_stage_shader_variables;
    }
    if (limits.max_color_attachments != kLimitU32Undefined) {
        native_limits.maxColorAttachments = limits.max_color_attachments;
    }
    if (limits.max_color_attachment_bytes_per_sample != kLimitU32Undefined) {
        native_limits.maxColorAttachmentBytesPerSample = limits.max_color_attachment_bytes_per_sample;
    }
    if (limits.max_compute_workgroup_storage_size != kLimitU32Undefined) {
        native_limits.maxComputeWorkgroupStorageSize = limits.max_compute_workgroup_storage_size;
    }
    if (limits.max_compute_invocations_per_workgroup != kLimitU32Undefined) {
        native_limits.maxComputeInvocationsPerWorkgroup = limits.max_compute_invocations_per_workgroup;
    }
    if (limits.max_compute_workgroup_size_x != kLimitU32Undefined) {
        native_limits.maxComputeWorkgroupSizeX = limits.max_compute_workgroup_size_x;
    }
    if (limits.max_compute_workgroup_size_y != kLimitU32Undefined) {
        native_limits.maxComputeWorkgroupSizeY = limits.max_compute_workgroup_size_y;
    }
    if (limits.max_compute_workgroup_size_z != kLimitU32Undefined) {
        native_limits.maxComputeWorkgroupSizeZ = limits.max_compute_workgroup_size_z;
    }
    if (limits.max_compute_workgroups_per_dimension != kLimitU32Undefined) {
        native_limits.maxComputeWorkgroupsPerDimension = limits.max_compute_workgroups_per_dimension;
    }
    if (limits.max_immediate_size != kLimitU32Undefined) {
        native_limits.maxImmediateSize = limits.max_immediate_size;
    }

    return native_limits;
}

[[nodiscard]] inline Result<void> FillLimits(WGPUAdapter adapter, Limits& limits) {
    if (adapter == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Adapter is invalid");
    }

    WGPULimits native_limits = WGPU_LIMITS_INIT;
    if (wgpuAdapterGetLimits(adapter, &native_limits) != WGPUStatus_Success) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to query adapter limits");
    }

    FillLimitsFromNative(native_limits, limits);
    return Ok();
}

[[nodiscard]] inline Limits QueryLimits(WGPUAdapter adapter) {
    Limits limits{};
    (void)FillLimits(adapter, limits);
    return limits;
}

} // namespace woki::rhi::wgpu::detail
