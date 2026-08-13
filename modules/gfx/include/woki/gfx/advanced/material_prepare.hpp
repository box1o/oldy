#pragma once

#include <woki/core.hpp>

#include "material_library.hpp"
#include "runtime_services.hpp"
#include "pipeline_cache.hpp"

namespace woki::gfx {

struct MaterialBindingKey final {
    struct Texture final {
        u32 binding{};
        ResourceKind kind{ResourceKind::SampledTexture};
        asset::AssetId asset;
        ContentHash product;
        u64 generation{};
        u64 physical{};
        [[nodiscard]] friend auto operator<=>(const Texture&, const Texture&) noexcept = default;
    };

    struct Sampler final {
        u32 binding{};
        ResourceKind kind{ResourceKind::Sampler};
        SamplerKey key;
        [[nodiscard]] friend auto operator<=>(const Sampler&, const Sampler&) noexcept = default;
    };

    asset::AssetId definition;
    ContentHash interface_hash;
    ContentHash layout_hash;
    AllocationId parameter_allocation;
    u64 buffer_allocation_version{};
    u32 parameter_binding{};
    ResourceKind parameter_kind{ResourceKind::UniformBuffer};
    std::vector<Texture> textures;
    std::vector<Sampler> samplers;
    [[nodiscard]] friend auto operator<=>(const MaterialBindingKey&, const MaterialBindingKey&) noexcept = default;
};

struct MaterialGpuRecord final {
    MaterialInstanceHandle instance;
    ShaderVariantHandle shader_variant;
    ContentHash shader_product_identity;
    ContentHash shader_variant_identity;
    BufferSlice parameters;
    u32 parameter_index{};
    ref<rhi::BindGroup> bind_group;
    MaterialRenderState render_state;
    ContentHash render_state_identity;
    MaterialPhase phase{MaterialPhase::Opaque};
    ContentHash pipeline_layout;
    u64 shader_generation{};
    std::vector<MaterialPassProgram> programs;
    std::vector<PipelineOverride> overrides;
    u64 prepared_version{};
};

struct MaterialShaderIdentity final {
    ShaderVariantHandle variant;
    ContentHash product;
    ContentHash variant_hash;
    u64 generation{};
    ContentHash interface_hash;
};

struct PreparedMaterial final {
    MaterialGpuHandle material;
    ref<rhi::BindGroup> group;
    GraphicsPipelineKey pipeline;
    PipelineLayoutKey layout;
    MaterialRenderState render_state;

    [[nodiscard]] Result<PipelineRequest<rhi::RenderPipeline>> RequestPipeline(PipelineCache& cache, PipelineCache::GraphicsFactory create) const {
        return cache.Request(pipeline, std::move(create));
    }
};

class MaterialBindingCache final {
public:
    explicit MaterialBindingCache(ref<rhi::Device> device)
        : device_(std::move(device)) {}

    [[nodiscard]] Result<ref<rhi::BindGroup>> GetOrCreate(const MaterialBindingKey& key, rhi::BindGroupLayout& layout, std::span<const rhi::BindGroupEntryDesc> entries);
    [[nodiscard]] Result<ref<rhi::Sampler>> ResolveSampler(const SamplerKey& key);
    void MarkUsed(const MaterialBindingKey& key, rhi::SubmissionTicket submission);
    [[nodiscard]] size_t InvalidateDefinition(asset::AssetId id, DeferredReleaseQueue& releases);
    [[nodiscard]] size_t Prune(DeferredReleaseQueue& releases);

private:
    struct Entry {
        ref<rhi::BindGroup> bind_group;
        rhi::SubmissionTicket last_used;
    };

    ref<rhi::Device> device_;
    std::map<MaterialBindingKey, Entry> entries_;
    std::map<SamplerKey, ref<rhi::Sampler>> samplers_;
};

class MaterialPreparation final {
public:
    using ResolveShader = std::function<Result<MaterialShaderIdentity>(asset::AssetId)>;
    MaterialPreparation(MaterialLibrary& materials, TextureLibrary& textures, BufferPool& data_pool, UploadScheduler& uploads, LayoutCache& layouts, MaterialBindingCache& bindings, ResolveShader resolve_shader);
    [[nodiscard]] Result<std::vector<MaterialGpuHandle>> PrepareDirty();
    [[nodiscard]] std::optional<MaterialGpuHandle> Resolve(MaterialInstanceHandle handle) const noexcept;
    [[nodiscard]] const MaterialGpuRecord* TryGet(MaterialGpuHandle handle) const noexcept;
    [[nodiscard]] Result<PreparedMaterial> PipelineRequest(MaterialGpuHandle handle, MaterialPass pass, PipelineTargetSignature targets, u64 vertex_schema_id, bool skinned = false) const;
    [[nodiscard]] size_t InvalidateDefinition(asset::AssetId definition, DeferredReleaseQueue& releases);
    void MarkUsed(MaterialGpuHandle handle, rhi::SubmissionTicket submission);

private:
    struct Slot {
        MaterialGpuRecord record;
        MaterialBindingKey key;

        struct TextureUse final {
            TextureHandle handle;
            TextureSemantic semantic{TextureSemantic::Color};
            asset::AssetVersion version;
            u64 physical{};
        };

        std::vector<TextureUse> textures;
    };

    [[nodiscard]] Result<MaterialGpuHandle> Prepare(MaterialInstanceHandle handle);
    MaterialLibrary& materials_;
    TextureLibrary& textures_;
    BufferPool& pool_;
    UploadScheduler& uploads_;
    LayoutCache& layouts_;
    MaterialBindingCache& bindings_;
    ResolveShader resolve_shader_;
    SlotMap<Slot, MaterialGpuHandle> slots_;
    std::unordered_map<MaterialInstanceHandle, MaterialGpuHandle> prepared_;
    std::unordered_map<TextureHandle, std::unordered_set<MaterialInstanceHandle>> reverse_textures_;
};

class MaterialReloadCoordinator final {
public:
    MaterialReloadCoordinator(MaterialLibrary& materials, MaterialPreparation& preparation, PipelineCache& pipelines, DeferredReleaseQueue& releases);
    ~MaterialReloadCoordinator();
    MaterialReloadCoordinator(const MaterialReloadCoordinator&) = delete;
    MaterialReloadCoordinator& operator=(const MaterialReloadCoordinator&) = delete;

private:
    MaterialLibrary& materials_;
};

} // namespace woki::gfx
