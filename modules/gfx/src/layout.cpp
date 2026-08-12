#include <woki/gfx/layout.hpp>

#include <algorithm>

namespace woki::gfx {
namespace {

rhi::TextureViewDimension Dimension(const TextureDimension value) {
    switch (value) {
        case TextureDimension::D1:
            return rhi::TextureViewDimension::e1D;
        case TextureDimension::D2:
            return rhi::TextureViewDimension::e2D;
        case TextureDimension::D2Array:
            return rhi::TextureViewDimension::e2DArray;
        case TextureDimension::D3:
            return rhi::TextureViewDimension::e3D;
        case TextureDimension::Cube:
            return rhi::TextureViewDimension::Cube;
        case TextureDimension::CubeArray:
            return rhi::TextureViewDimension::CubeArray;
        case TextureDimension::None:
            return rhi::TextureViewDimension::Undefined;
    }
    return rhi::TextureViewDimension::Undefined;
}

rhi::TextureFormat Format(const StorageFormat value) {
    switch (value) {
        case StorageFormat::R32Uint:
            return rhi::TextureFormat::R32Uint;
        case StorageFormat::R32Sint:
            return rhi::TextureFormat::R32Sint;
        case StorageFormat::R32Float:
            return rhi::TextureFormat::R32Float;
        case StorageFormat::RG32Uint:
            return rhi::TextureFormat::RG32Uint;
        case StorageFormat::RG32Sint:
            return rhi::TextureFormat::RG32Sint;
        case StorageFormat::RG32Float:
            return rhi::TextureFormat::RG32Float;
        case StorageFormat::RGBA8Unorm:
            return rhi::TextureFormat::RGBA8Unorm;
        case StorageFormat::RGBA8Snorm:
            return rhi::TextureFormat::RGBA8Snorm;
        case StorageFormat::RGBA8Uint:
            return rhi::TextureFormat::RGBA8Uint;
        case StorageFormat::RGBA8Sint:
            return rhi::TextureFormat::RGBA8Sint;
        case StorageFormat::BGRA8Unorm:
            return rhi::TextureFormat::BGRA8Unorm;
        case StorageFormat::RGBA16Uint:
            return rhi::TextureFormat::RGBA16Uint;
        case StorageFormat::RGBA16Sint:
            return rhi::TextureFormat::RGBA16Sint;
        case StorageFormat::RGBA16Float:
            return rhi::TextureFormat::RGBA16Float;
        case StorageFormat::RGBA32Uint:
            return rhi::TextureFormat::RGBA32Uint;
        case StorageFormat::RGBA32Sint:
            return rhi::TextureFormat::RGBA32Sint;
        case StorageFormat::RGBA32Float:
            return rhi::TextureFormat::RGBA32Float;
        case StorageFormat::None:
            return rhi::TextureFormat::Undefined;
    }
    return rhi::TextureFormat::Undefined;
}

rhi::BindGroupLayoutEntryDesc MakeEntry(const BindingInfo& binding) {
    rhi::BindGroupLayoutEntryDesc entry{.binding = binding.binding, .visibility = binding.stages, .binding_array_size = binding.array_size};
    switch (binding.kind) {
        case ResourceKind::UniformBuffer:
            entry.buffer = {.type = rhi::BufferBindingType::Uniform, .min_binding_size = binding.min_binding_size};
            break;
        case ResourceKind::StorageBuffer:
            entry.buffer = {.type = rhi::BufferBindingType::Storage, .min_binding_size = binding.min_binding_size};
            break;
        case ResourceKind::ReadOnlyStorageBuffer:
            entry.buffer = {.type = rhi::BufferBindingType::ReadOnlyStorage, .min_binding_size = binding.min_binding_size};
            break;
        case ResourceKind::Sampler:
            entry.sampler = {.type = rhi::SamplerBindingType::Filtering};
            break;
        case ResourceKind::ComparisonSampler:
            entry.sampler = {.type = rhi::SamplerBindingType::Comparison};
            break;
        case ResourceKind::SampledTexture:
        case ResourceKind::MultisampledTexture:
        case ResourceKind::DepthTexture:
            entry.texture = {.sample_type = binding.sample_type == SampleType::Depth               ? rhi::TextureSampleType::Depth
                                            : binding.sample_type == SampleType::Sint              ? rhi::TextureSampleType::Sint
                                            : binding.sample_type == SampleType::Uint              ? rhi::TextureSampleType::Uint
                                            : binding.sample_type == SampleType::UnfilterableFloat ? rhi::TextureSampleType::UnfilterableFloat
                                                                                                   : rhi::TextureSampleType::Float,
                .view_dimension = Dimension(binding.dimension),
                .multisampled = binding.kind == ResourceKind::MultisampledTexture};
            break;
        case ResourceKind::StorageTexture:
            entry.storage_texture = {.access = binding.storage_access == StorageAccess::ReadOnly    ? rhi::StorageTextureAccess::ReadOnly
                                               : binding.storage_access == StorageAccess::ReadWrite ? rhi::StorageTextureAccess::ReadWrite
                                                                                                    : rhi::StorageTextureAccess::WriteOnly,
                .format = Format(binding.storage_format),
                .view_dimension = Dimension(binding.dimension)};
            break;
        case ResourceKind::ExternalTexture:
            break;
    }
    return entry;
}

} // namespace

Result<PipelineLayoutKey> MakePipelineLayoutKey(const ShaderInterface& interface) {
    PipelineLayoutKey key;
    for (const BindingInfo& binding : interface.bindings) {
        if (binding.group > 3)
            return Err(ErrorCode::OutOfRange, "shader bind group violates the frame/view/material/object group policy");
        while (key.groups.size() <= binding.group)
            key.groups.push_back({static_cast<u32>(key.groups.size()), {}, {}});
        key.groups[binding.group].bindings.push_back(binding);
    }
    std::string canonical;
    for (auto& group : key.groups) {
        std::ranges::sort(group.bindings, {}, &BindingInfo::binding);
        ShaderInterface temporary;
        temporary.bindings = group.bindings;
        NormalizeInterface(temporary);
        group.hash = temporary.hash;
        canonical += std::to_string(group.group) + group.hash.Hex();
    }
    key.hash = Sha256(canonical);
    return Ok(std::move(key));
}

Result<rhi::PipelineLayout*> LayoutCache::GetOrCreate(const PipelineLayoutKey& key) {
    for (auto& entry : entries_)
        if (entry.key == key)
            return Ok(entry.pipeline.get());
    Entry created{.key = key, .groups = {}, .pipeline = {}};
    std::vector<rhi::BindGroupLayout*> pointers;
    for (const auto& group : key.groups) {
        std::vector<rhi::BindGroupLayoutEntryDesc> descriptors;
        descriptors.reserve(group.bindings.size());
        for (const auto& binding : group.bindings)
            descriptors.push_back(MakeEntry(binding));
        auto layout = device_.CreateBindGroupLayout({.entries = descriptors, .label = "Shader group " + std::to_string(group.group)});
        if (!layout)
            return Err(std::move(layout).error());
        pointers.push_back(layout->get());
        created.groups.push_back(std::move(*layout));
    }
    auto pipeline = device_.CreatePipelineLayout({.bind_group_layouts = pointers, .label = "Shader pipeline layout " + key.hash.Hex()});
    if (!pipeline)
        return Err(std::move(pipeline).error());
    created.pipeline = std::move(*pipeline);
    rhi::PipelineLayout* result = created.pipeline.get();
    entries_.push_back(std::move(created));
    return Ok(result);
}

} // namespace woki::gfx
