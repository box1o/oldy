#include <bit>
#include <cstring>
#include <utility>

#include <woki/gfx/advanced/material_prepare.hpp>

namespace woki::gfx {
namespace {
template <typename T>
void Store(std::vector<std::byte>& output, const u32 offset, const T& value) {
    std::memcpy(output.data() + offset, &value, sizeof(value));
}

Result<void> Pack(std::vector<std::byte>& output, const MaterialPropertyPlan& property, const MaterialValue& value) {
    if (!MaterialValueMatches(property.type, value) || property.offset + property.size > output.size())
        return Err(ErrorCode::ValidationInvalidState, "material property cannot be packed");
    switch (property.type) {
        case MaterialValueType::Bool: {
            const u32 encoded = std::get<bool>(value) ? 1U : 0U;
            Store(output, property.offset, encoded);
            break;
        }
        case MaterialValueType::I32:
            Store(output, property.offset, std::get<i32>(value));
            break;
        case MaterialValueType::U32:
            Store(output, property.offset, std::get<u32>(value));
            break;
        case MaterialValueType::F32:
            Store(output, property.offset, std::get<f32>(value));
            break;
        case MaterialValueType::Vec2:
            Store(output, property.offset, std::get<std::array<f32, 2>>(value));
            break;
        case MaterialValueType::Vec3:
            Store(output, property.offset, std::get<std::array<f32, 3>>(value));
            break;
        case MaterialValueType::Vec4:
            Store(output, property.offset, std::get<std::array<f32, 4>>(value));
            break;
        case MaterialValueType::Mat4:
            Store(output, property.offset, std::get<std::array<f32, 16>>(value));
            break;
        default:
            return Err(ErrorCode::ValidationInvalidState, "resource property cannot be packed into material data");
    }
    return Ok();
}

u64 OverrideBits(const MaterialValue& value) {
    if (const auto item = std::get_if<bool>(&value))
        return *item ? 1 : 0;
    if (const auto item = std::get_if<i32>(&value))
        return static_cast<u32>(*item);
    if (const auto item = std::get_if<u32>(&value))
        return *item;
    return std::bit_cast<u32>(std::get<f32>(value));
}

class AllocationRollback final {
public:
    AllocationRollback(BufferPool& pool, AllocationId allocation)
        : pool_(&pool),
          allocation_(allocation) {}

    ~AllocationRollback() {
        if (pool_ != nullptr)
            static_cast<void>(pool_->Free(allocation_));
    }

    void Commit() noexcept {
        pool_ = nullptr;
    }

private:
    BufferPool* pool_;
    AllocationId allocation_;
};
} // namespace

Result<ref<rhi::BindGroup>> MaterialBindingCache::GetOrCreate(
    const MaterialBindingKey& key,
    rhi::BindGroupLayout& layout,
    const std::span<const rhi::BindGroupEntryDesc> descriptors
) {
    if (const auto found = entries_.find(key); found != entries_.end())
        return Ok(found->second.bind_group);
    auto group = device_->CreateBindGroup({.layout = &layout, .entries = descriptors, .label = "Material bind group"});
    if (!group)
        return Err(std::move(group).error());
    ref<rhi::BindGroup> retained(std::move(*group));
    entries_.emplace(key, Entry{retained, {}});
    return Ok(std::move(retained));
}

Result<ref<rhi::Sampler>> MaterialBindingCache::ResolveSampler(const SamplerKey& key) {
    if (const auto found = samplers_.find(key); found != samplers_.end())
        return Ok(found->second);
    auto sampler = device_->CreateSampler(
        {.address_mode_u = key.address_u,
            .address_mode_v = key.address_v,
            .address_mode_w = key.address_w,
            .mag_filter = key.mag_filter,
            .min_filter = key.min_filter,
            .mipmap_filter = key.mip_filter,
            .lod_min_clamp = std::bit_cast<f32>(key.lod_min_bits),
            .lod_max_clamp = std::bit_cast<f32>(key.lod_max_bits),
            .compare = key.compare,
            .max_anisotropy = key.max_anisotropy,
            .label = "Material sampler"}
    );
    if (!sampler)
        return Err(std::move(sampler).error());
    ref<rhi::Sampler> retained(std::move(*sampler));
    samplers_.emplace(key, retained);
    return Ok(std::move(retained));
}

void MaterialBindingCache::MarkUsed(const MaterialBindingKey& key, const rhi::SubmissionTicket submission) {
    if (const auto found = entries_.find(key); found != entries_.end() && found->second.last_used < submission)
        found->second.last_used = submission;
}

size_t MaterialBindingCache::InvalidateDefinition(const asset::AssetId id, DeferredReleaseQueue& releases) {
    const size_t before = entries_.size();
    std::erase_if(entries_, [&](auto& item) {
        if (item.first.definition != id)
            return false;
        if (item.second.last_used.IsValid())
            releases.Retire(std::move(item.second.bind_group), item.second.last_used);
        return true;
    });
    return before - entries_.size();
}

size_t MaterialBindingCache::Prune(DeferredReleaseQueue& releases) {
    const size_t before = entries_.size();
    std::erase_if(entries_, [&](auto& item) {
        if (item.second.bind_group.use_count() != 1 || !item.second.last_used.IsValid())
            return false;
        releases.Retire(std::move(item.second.bind_group), item.second.last_used);
        return true;
    });
    return before - entries_.size();
}

MaterialPreparation::MaterialPreparation(
    MaterialLibrary& materials,
    TextureLibrary& textures,
    BufferPool& data_pool,
    UploadScheduler& uploads,
    LayoutCache& layouts,
    MaterialBindingCache& bindings,
    ResolveShader resolve_shader
)
    : materials_(materials),
      textures_(textures),
      pool_(data_pool),
      uploads_(uploads),
      layouts_(layouts),
      bindings_(bindings),
      resolve_shader_(std::move(resolve_shader)) {}

Result<MaterialGpuHandle> MaterialPreparation::Prepare(const MaterialInstanceHandle handle) {
    const auto* instance = materials_.TryGet(handle);
    if (!instance)
        return Err(ErrorCode::InvalidArgument, "material instance handle is stale");
    BorrowedMaterialDefinition definition;
    TRY_ASSIGN(definition, materials_.Borrow(instance->definition));
    const auto& product = definition.Get();
    const auto& plan = product.binding_plan;
    if (!resolve_shader_)
        return Err(ErrorCode::InvalidState, "material preparation has no shader resolver");
    MaterialShaderIdentity shader;
    TRY_ASSIGN(shader, resolve_shader_(product.shader));
    if (shader.product != product.shader_product_hash || shader.variant_hash != product.shader_variant_hash
        || shader.interface_hash != plan.interface_hash || shader.generation == 0)
        return Err(
            ErrorCode::ValidationInvalidState,
            "resolved shader generation does not match the material definition"
        );
    std::vector<std::byte> bytes(plan.parameter_size);
    for (const auto& property : plan.properties) {
        const auto value = instance->values.find(property.id);
        if (value == instance->values.end())
            return Err(ErrorCode::ValidationInvalidState, "material instance lacks a parameter value");
        TRY_VOID(Pack(bytes, property, value->second));
    }
    BufferSlice allocation;
    TRY_ASSIGN(allocation, pool_.Allocate(bytes.size(), 256));
    AllocationRollback rollback(pool_, allocation.allocation);
    std::vector<rhi::BindGroupEntryDesc> descriptors;
    descriptors.push_back(
        {.binding = plan.parameter_binding,
            .buffer = allocation.buffer,
            .offset = allocation.offset,
            .size = plan.parameter_size}
    );
    MaterialBindingKey key{.definition = product.id,
        .interface_hash = plan.interface_hash,
        .layout_hash = plan.pipeline_layout.hash,
        .parameter_allocation = allocation.allocation,
        .buffer_allocation_version = allocation.allocation.Value(),
        .parameter_binding = plan.parameter_binding,
        .parameter_kind = ResourceKind::UniformBuffer,
        .textures = {},
        .samplers = {}};
    if (plan.pipeline_layout.groups.size() <= kMaterialGroup)
        return Err(ErrorCode::ValidationInvalidState, "material pipeline layout has no group 2");
    const auto parameter_layout = std::ranges::find_if(
        plan.pipeline_layout.groups[kMaterialGroup].bindings,
        [&](const BindingInfo& item) { return item.binding == plan.parameter_binding; }
    );
    if (parameter_layout == plan.pipeline_layout.groups[kMaterialGroup].bindings.end())
        return Err(ErrorCode::ValidationInvalidState, "material parameter binding is absent from group 2");
    key.parameter_kind = parameter_layout->kind;
    std::vector<Slot::TextureUse> texture_handles;
    for (const auto& binding : plan.textures) {
        const auto value = instance->values.find(binding.id);
        if (value == instance->values.end() || !std::holds_alternative<asset::AssetId>(value->second))
            return Err(ErrorCode::ValidationInvalidState, "material instance lacks a texture value");
        const auto asset = std::get<asset::AssetId>(value->second);
        TextureHandle texture;
        TRY_ASSIGN(texture, textures_.Request(asset));
        ResolvedTexture resolved;
        TRY_ASSIGN(resolved, textures_.Resolve(texture, binding.semantic));
        const auto version = textures_.Version(texture);
        key.textures.push_back(
            {binding.binding,
                ResourceKind::SampledTexture,
                asset,
                version.product_hash,
                version.generation,
                resolved.physical->id}
        );
        descriptors.push_back({.binding = binding.binding, .texture_view = resolved.physical->default_view.get()});
        texture_handles.push_back({texture, binding.semantic, version, resolved.physical->id});
    }
    for (const auto& binding : plan.samplers) {
        ref<rhi::Sampler> sampler;
        TRY_ASSIGN(sampler, bindings_.ResolveSampler(binding.key));
        key.samplers.push_back({binding.binding, ResourceKind::Sampler, binding.key});
        descriptors.push_back({.binding = binding.binding, .sampler = sampler.get()});
    }
    std::ranges::sort(descriptors, {}, &rhi::BindGroupEntryDesc::binding);
    std::ranges::sort(key.textures, {}, &MaterialBindingKey::Texture::binding);
    std::ranges::sort(key.samplers, {}, &MaterialBindingKey::Sampler::binding);
    BorrowedLayout layout;
    TRY_ASSIGN(layout, layouts_.GetOrCreate(plan.pipeline_layout));
    if (layout.BindGroupLayouts().size() <= kMaterialGroup)
        return Err(ErrorCode::ValidationInvalidState, "material pipeline layout has no group 2");
    ref<rhi::BindGroup> group;
    TRY_ASSIGN(group, bindings_.GetOrCreate(key, *layout.BindGroupLayouts()[kMaterialGroup], descriptors));
    TRY_VOID(uploads_.Enqueue(
        {.target = pool_.SharedBuffer(), .offset = allocation.offset, .bytes = std::move(bytes), .publication = {}}
    ));
    MaterialGpuHandle gpu;
    if (const auto found = prepared_.find(handle); found != prepared_.end()) {
        gpu = found->second;
        auto& old = slots_.Get(gpu);
        static_cast<void>(pool_.Free(old.record.parameters.allocation));
        for (const auto& texture : old.textures)
            reverse_textures_[texture.handle].erase(handle);
        old.record = {};
        old.key = {};
        old.textures.clear();
    } else {
        gpu = slots_.Emplace();
        prepared_.emplace(handle, gpu);
    }
    auto& slot = slots_.Get(gpu);
    slot.record = {.instance = handle,
        .shader_variant = shader.variant,
        .shader_product_identity = shader.product,
        .shader_variant_identity = shader.variant_hash,
        .parameters = allocation,
        .parameter_index = static_cast<u32>(allocation.offset / std::max<u32>(plan.parameter_size, 1)),
        .bind_group = std::move(group),
        .render_state = plan.render_state,
        .render_state_identity = plan.render_state_identity,
        .phase = plan.phase,
        .pipeline_layout = plan.pipeline_layout.hash,
        .shader_generation = shader.generation,
        .programs = product.programs,
        .overrides = {},
        .prepared_version = instance->content_version};
    for (const auto& variant : plan.variants) {
        const auto value = instance->values.find(variant.property);
        if (value == instance->values.end())
            return Err(ErrorCode::ValidationInvalidState, "material specialization property has no value");
        if (std::ranges::find(variant.allowed_values, value->second) == variant.allowed_values.end())
            return Err(
                ErrorCode::ValidationOutOfRange,
                "material specialization value is outside the cooked variant set"
            );
        slot.record.overrides.push_back({variant.override_id, OverrideBits(value->second)});
    }
    slot.key = std::move(key);
    slot.textures = texture_handles;
    for (const auto& texture : texture_handles)
        reverse_textures_[texture.handle].insert(handle);
    rollback.Commit();
    materials_.MarkPrepared(handle, instance->content_version);
    return Ok(gpu);
}

Result<std::vector<MaterialGpuHandle>> MaterialPreparation::PrepareDirty() {
    for (const auto& prepared : prepared_) {
        auto* stored = slots_.TryGet(prepared.second);
        if (stored == nullptr)
            continue;
        auto& slot = *stored;
        for (auto& use : slot.textures) {
            auto resolved = textures_.Resolve(use.handle, use.semantic);
            if (!resolved)
                continue;
            const auto version = textures_.Version(use.handle);
            if (version.generation != use.version.generation || version.product_hash != use.version.product_hash
                || resolved->physical->id != use.physical) {
                if (auto* record = materials_.TryGetMutable(slot.record.instance))
                    record->dirty = true;
                break;
            }
        }
    }
    std::vector<MaterialGpuHandle> result;
    for (const auto handle : materials_.DirtyInstances()) {
        const auto* instance = materials_.TryGet(handle);
        if (instance == nullptr)
            continue;
        const auto state = materials_.State(instance->definition);
        if (!state || state == MaterialState::Empty || state == MaterialState::Loading
            || state == MaterialState::Reloading)
            continue;
        if (state == MaterialState::Failed)
            return Err(
                ErrorCode::ValidationInvalidState,
                materials_.Diagnostic(instance->definition).empty()
                    ? "material definition failed to load"
                    : std::string(materials_.Diagnostic(instance->definition))
            );
        MaterialGpuHandle prepared;
        TRY_ASSIGN(prepared, Prepare(handle));
        result.push_back(prepared);
    }
    return Ok(std::move(result));
}

std::optional<MaterialGpuHandle> MaterialPreparation::Resolve(const MaterialInstanceHandle handle) const noexcept {
    const auto found = prepared_.find(handle);
    return found == prepared_.end() || TryGet(found->second) == nullptr ? std::nullopt : std::optional(found->second);
}

const MaterialGpuRecord* MaterialPreparation::TryGet(const MaterialGpuHandle handle) const noexcept {
    const auto* slot = slots_.TryGet(handle);
    return slot == nullptr ? nullptr : &slot->record;
}

void MaterialPreparation::MarkUsed(const MaterialGpuHandle handle, const rhi::SubmissionTicket submission) {
    if (!submission.IsValid() || !TryGet(handle))
        return;
    auto& slot = slots_.Get(handle);
    bindings_.MarkUsed(slot.key, submission);
    pool_.MarkUsed(submission);
    for (const auto& texture : slot.textures)
        textures_.MarkUsed(texture.handle, submission);
}

Result<PreparedMaterial> MaterialPreparation::PipelineRequest(
    const MaterialGpuHandle handle,
    const MaterialPass pass,
    PipelineTargetSignature targets,
    const u64 vertex_schema_id,
    const bool skinned
) const {
    const auto* record = TryGet(handle);
    if (!record)
        return Err(ErrorCode::InvalidArgument, "prepared material handle is stale");
    auto program = std::ranges::find(record->programs, pass, &MaterialPassProgram::pass);
    bool depth_only_fallback = false;
    if (program == record->programs.end() && (pass == MaterialPass::Depth || pass == MaterialPass::Shadow)) {
        program = std::ranges::find(record->programs, MaterialPass::Forward, &MaterialPassProgram::pass);
        depth_only_fallback = program != record->programs.end();
    }
    if (program == record->programs.end())
        return Err(ErrorCode::InvalidArgument, "material does not support the requested pass");
    std::string vertex_entry = program->vertex_entry;
    if (skinned) {
        constexpr std::string_view marker = "_static_vs";
        if (const auto offset = vertex_entry.rfind(marker); offset != std::string::npos)
            vertex_entry.replace(offset, marker.size(), "_skinned_vs");
    }
    GraphicsPipelineKey key{.shader_product = record->shader_product_identity,
        .shader_variant = record->shader_variant_identity,
        .shader_generation = record->shader_generation,
        .vertex_entry = StringId(vertex_entry),
        .fragment_entry = depth_only_fallback ? StringId{} : StringId(program->fragment_entry),
        .vertex_schema_id = vertex_schema_id,
        .render_state = record->render_state_identity,
        .targets = std::move(targets),
        .pipeline_layout = record->pipeline_layout,
        .overrides = record->overrides};
    const auto* instance = materials_.TryGet(record->instance);
    if (instance == nullptr)
        return Err(ErrorCode::InvalidState, "prepared material instance is unavailable");
    BorrowedMaterialDefinition definition;
    TRY_ASSIGN(definition, materials_.Borrow(instance->definition));
    auto render_state = record->render_state;
    if (pass == MaterialPass::Velocity) {
        render_state.depth_write = false;
        render_state.blend = MaterialBlendMode::Opaque;
    }
    return Ok(
        PreparedMaterial{handle,
            record->bind_group,
            std::move(key),
            definition.Get().binding_plan.pipeline_layout,
            render_state}
    );
}

size_t MaterialPreparation::InvalidateDefinition(const asset::AssetId definition, DeferredReleaseQueue& releases) {
    size_t count{};
    for (auto it = prepared_.begin(); it != prepared_.end();) {
        const auto gpu = it->second;
        auto& slot = slots_.Get(gpu);
        const auto instance = materials_.TryGet(it->first);
        auto borrowed = instance ? materials_.Borrow(instance->definition)
                                 : Result<BorrowedMaterialDefinition>(
                                       Err(ErrorCode::InvalidState, "material instance is unavailable")
                                   );
        if (!borrowed || borrowed->Get().id != definition) {
            ++it;
            continue;
        }
        static_cast<void>(pool_.Free(slot.record.parameters.allocation));
        for (const auto& texture : slot.textures)
            reverse_textures_[texture.handle].erase(it->first);
        static_cast<void>(slots_.Remove(gpu));
        it = prepared_.erase(it);
        ++count;
    }
    static_cast<void>(bindings_.InvalidateDefinition(definition, releases));
    return count;
}

MaterialReloadCoordinator::MaterialReloadCoordinator(
    MaterialLibrary& materials,
    MaterialPreparation& preparation,
    PipelineCache& pipelines,
    DeferredReleaseQueue& releases
)
    : materials_(materials) {
    materials_.SetDefinitionReloadObserver(
        [&preparation, &pipelines, &releases](const asset::AssetId definition, const ContentHash old_shader) {
            static_cast<void>(preparation.InvalidateDefinition(definition, releases));
            static_cast<void>(pipelines.InvalidateShader(old_shader, releases));
        }
    );
}

MaterialReloadCoordinator::~MaterialReloadCoordinator() {
    materials_.SetDefinitionReloadObserver({});
}

} // namespace woki::gfx
