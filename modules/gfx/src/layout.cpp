#include <algorithm>

#include <woki/gfx/advanced/layout.hpp>

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

rhi::BindGroupLayoutEntryDesc MakeEntry(const BindingInfo& binding, const bool dynamic_offset) {
    rhi::BindGroupLayoutEntryDesc entry{.binding = binding.binding, .visibility = binding.stages, .binding_array_size = binding.array_size};
    switch (binding.kind) {
        case ResourceKind::UniformBuffer:
            entry.buffer = {.type = rhi::BufferBindingType::Uniform, .has_dynamic_offset = dynamic_offset, .min_binding_size = binding.min_binding_size};
            break;
        case ResourceKind::StorageBuffer:
            entry.buffer = {.type = rhi::BufferBindingType::Storage, .has_dynamic_offset = dynamic_offset, .min_binding_size = binding.min_binding_size};
            break;
        case ResourceKind::ReadOnlyStorageBuffer:
            entry.buffer = {.type = rhi::BufferBindingType::ReadOnlyStorage, .has_dynamic_offset = dynamic_offset, .min_binding_size = binding.min_binding_size};
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

Result<PipelineLayoutKey> MakePipelineLayoutKey(const ShaderInterface& interface, const std::span<const DynamicBufferBindingPolicy> dynamic_buffers) {
    for (size_t index = 0; index < interface.bindings.size(); ++index) {
        const auto& binding = interface.bindings[index];
        for (size_t other = index + 1; other < interface.bindings.size(); ++other) {
            const auto& candidate = interface.bindings[other];
            if (binding.group != candidate.group || binding.binding != candidate.binding)
                continue;
            if (binding.kind != candidate.kind || binding.dimension != candidate.dimension || binding.sample_type != candidate.sample_type || binding.storage_access != candidate.storage_access
                || binding.storage_format != candidate.storage_format || binding.min_binding_size != candidate.min_binding_size || binding.array_size != candidate.array_size)
                return Err(ErrorCode::ValidationInvalidState, "conflicting duplicate shader binding declaration");
        }
    }
    ShaderInterface normalized = interface;
    NormalizeInterface(normalized);

    std::vector<DynamicBufferBindingPolicy> policies(dynamic_buffers.begin(), dynamic_buffers.end());
    std::ranges::sort(policies, {}, [](const DynamicBufferBindingPolicy& policy) { return std::pair{policy.group, policy.binding}; });
    policies.erase(std::ranges::unique(policies).begin(), policies.end());

    PipelineLayoutKey key;
    for (const BindingInfo& binding : normalized.bindings) {
        if (binding.group > 3)
            return Err(ErrorCode::OutOfRange, "shader bind group violates the frame/view/material/object group policy");
        while (key.groups.size() <= binding.group)
            key.groups.push_back({static_cast<u32>(key.groups.size()), {}, {}, {}});
        key.groups[binding.group].bindings.push_back({
            .group = binding.group,
            .binding = binding.binding,
            .stages = binding.stages,
            .kind = binding.kind,
            .dimension = binding.dimension,
            .sample_type = binding.sample_type,
            .storage_access = binding.storage_access,
            .storage_format = binding.storage_format,
            .min_binding_size = binding.min_binding_size,
            .array_size = binding.array_size,
            .semantic = {},
            .group_semantic = {},
            .visible_entries = {},
            .buffer_type = {},
            .buffer_members = {},
        });
    }
    while (key.groups.size() < 4)
        key.groups.push_back({static_cast<u32>(key.groups.size()), {}, {}, {}});

    for (const DynamicBufferBindingPolicy& policy : policies) {
        const auto binding = std::ranges::find_if(normalized.bindings, [&](const BindingInfo& candidate) { return candidate.group == policy.group && candidate.binding == policy.binding; });
        if (binding == normalized.bindings.end())
            return Err(ErrorCode::InvalidArgument, "dynamic-offset policy refers to a missing shader binding");
        if (binding->kind != ResourceKind::UniformBuffer && binding->kind != ResourceKind::StorageBuffer && binding->kind != ResourceKind::ReadOnlyStorageBuffer)
            return Err(ErrorCode::InvalidArgument, "dynamic-offset policy requires a buffer binding");
        key.groups[policy.group].dynamic_buffer_bindings.push_back(policy.binding);
    }

    std::string canonical;
    for (auto& group : key.groups) {
        ShaderInterface temporary;
        temporary.bindings = group.bindings;
        NormalizeInterface(temporary);
        std::string group_canonical = temporary.hash.Hex();
        for (const u32 binding : group.dynamic_buffer_bindings)
            group_canonical += ":" + std::to_string(binding);
        group.hash = Sha256(group_canonical);
        canonical += std::to_string(group.group) + ":" + group.hash.Hex() + ";";
    }
    key.hash = Sha256(canonical);
    return Ok(std::move(key));
}

Result<BorrowedLayout> LayoutCache::GetOrCreate(const PipelineLayoutKey& key) {
    for (auto& entry : entries_)
        if (entry.key == key)
            return Ok(BorrowedLayout{entry.generation});
    auto generation = createRef<LayoutGeneration>();
    std::vector<rhi::BindGroupLayout*> pointers;
    for (const auto& group : key.groups) {
        std::vector<rhi::BindGroupLayoutEntryDesc> descriptors;
        descriptors.reserve(group.bindings.size());
        for (const auto& binding : group.bindings) {
            const bool dynamic_offset = std::ranges::binary_search(group.dynamic_buffer_bindings, binding.binding);
            descriptors.push_back(MakeEntry(binding, dynamic_offset));
        }
        auto layout = device_->CreateBindGroupLayout({.entries = descriptors, .label = "Shader group " + std::to_string(group.group)});
        if (!layout)
            return Err(std::move(layout).error());
        pointers.push_back(layout->get());
        generation->groups.push_back(std::move(*layout));
    }
    auto pipeline = device_->CreatePipelineLayout({.bind_group_layouts = pointers, .label = "Shader pipeline layout " + key.hash.Hex()});
    if (!pipeline)
        return Err(std::move(pipeline).error());
    generation->pipeline = std::move(*pipeline);
    generation->ordered_groups = pointers;
    ref<const LayoutGeneration> retained = std::move(generation);
    entries_.push_back({.key = key, .generation = retained, .last_used = {}});
    return Ok(BorrowedLayout{std::move(retained)});
}

void LayoutCache::MarkUsed(const BorrowedLayout& layout, const rhi::SubmissionTicket submission) {
    if (!submission.IsValid() || !layout.generation)
        return;
    const auto found = std::ranges::find_if(entries_, [&](const Entry& entry) { return entry.generation == layout.generation; });
    if (found != entries_.end() && found->last_used < submission)
        found->last_used = submission;
}

size_t LayoutCache::Prune(DeferredReleaseQueue& releases) {
    const size_t before = entries_.size();
    std::erase_if(entries_, [&](Entry& entry) {
        if (!entry.last_used.IsValid() || entry.generation.use_count() != 1)
            return false;
        releases.Retire(std::move(entry.generation), entry.last_used);
        return true;
    });
    return before - entries_.size();
}

} // namespace woki::gfx
