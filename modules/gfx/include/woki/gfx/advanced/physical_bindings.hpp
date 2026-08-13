#pragma once

#include <woki/math.hpp>
#include <woki/rhi/render_pass_encoder.hpp>

#include "animation.hpp"
#include "layout.hpp"
#include "library.hpp"
#include "upload.hpp"
#include "material_prepare.hpp"
#include "mesh_library.hpp"
#include "render_abi.hpp"
#include "render_world.hpp"
#include "environment.hpp"

namespace woki::gfx {

class PhysicalBindings final {
public:
    [[nodiscard]] static Result<scope<PhysicalBindings>> Create(ref<rhi::Device> device,
        LayoutCache& layouts,
        SkinPaletteRegistry& palettes,
        BufferPool& skin_pool,
        UploadScheduler& uploads,
        BorrowedShader diagnostic_shader);
    ~PhysicalBindings();
    [[nodiscard]] Result<void> Prepare(const RenderWorldSnapshot& world, const RenderView& view, f32 time, f32 delta_time);
    [[nodiscard]] Result<void> UseClusteredLighting(rhi::CommandEncoder& command, rhi::Buffer& lights, rhi::Buffer& grid, rhi::Buffer& indices, const abi::ClusterParams& params);
    [[nodiscard]] Result<void> UseShadows(rhi::CommandEncoder& command, rhi::TextureView& atlas, const abi::ShadowData& data);
    [[nodiscard]] Result<void> UseEnvironment(const ResolvedEnvironment& environment, bool lighting_enabled, bool sky_visible);
    void UseGpuScene(BufferSlice instances) noexcept;
    [[nodiscard]] Result<void> UseTemporalView(rhi::CommandEncoder& command, const RenderView& view, math::vec2f previous_jitter);
    [[nodiscard]] Result<void> UseViewProjection(rhi::CommandEncoder& command,
        const math::mat4f& view_projection,
        const math::mat4f& inverse_view_projection,
        math::vec3f position,
        f32 near_plane,
        f32 far_plane,
        f32 width,
        f32 height);
    [[nodiscard]] Result<BorrowedLayout> Bind(rhi::RenderPassEncoder& encoder, const PreparedMaterial& material, u32 object_index, SkinPaletteHandle palette);
    [[nodiscard]] Result<BorrowedLayout> PrepareLayout(const PipelineLayoutKey& layout, SkinPaletteHandle palette);
    [[nodiscard]] Result<void> PrepareDiagnostic(const PipelineTargetSignature& targets);
    [[nodiscard]] Result<void> PrepareFallback(const PipelineTargetSignature& targets, const VertexSchema& schema, SkinPaletteHandle palette);
    [[nodiscard]] Result<void> BindViewGroups(rhi::RenderPassEncoder& encoder, const PipelineLayoutKey& layout);
    [[nodiscard]] Result<void> DrawDiagnostic(rhi::RenderPassEncoder& encoder, const PipelineTargetSignature& targets, u32 object_index);
    [[nodiscard]] Result<void> DrawFallbackMesh(rhi::RenderPassEncoder& encoder,
        const PipelineTargetSignature& targets,
        const VertexSchema& schema,
        const MeshResident& resident,
        u32 lod,
        u32 object_index,
        SkinPaletteHandle palette,
        u32 first_index,
        u32 index_count,
        i32 vertex_offset);
    void MarkUsed(rhi::SubmissionTicket submission);

private:
    struct Generation final {
        ContentHash layout_hash;
        SkinPaletteHandle palette;
        AllocationId palette_allocation;
        AllocationId previous_palette_allocation;
        BorrowedLayout layout;
        ref<rhi::BindGroup> frame;
        ref<rhi::BindGroup> view;
        ref<rhi::BindGroup> material;
        ref<rhi::BindGroup> object;
        rhi::Buffer* lights{};
        rhi::Buffer* clusters{};
        rhi::Buffer* indices{};
        rhi::Buffer* gpu_instances{};
        u64 gpu_instances_offset{};
        rhi::TextureView* shadow_atlas{};
        std::array<rhi::TextureView*, 4> environment{};
        rhi::SubmissionTicket last_used;
        u64 binding_generation{};
    };

    PhysicalBindings(ref<rhi::Device> device, LayoutCache& layouts, SkinPaletteRegistry& palettes, BufferPool& skin_pool, UploadScheduler& uploads);
    [[nodiscard]] Result<Generation*> Resolve(const PipelineLayoutKey& key, SkinPaletteHandle palette);

    ref<rhi::Device> device_;
    LayoutCache& layouts_;
    SkinPaletteRegistry& palettes_;
    BufferPool& skin_pool_;
    UploadScheduler& uploads_;
    ref<rhi::Buffer> frame_buffer_;
    ref<rhi::Buffer> scene_buffer_;
    ref<rhi::Buffer> view_buffer_;
    ref<rhi::Buffer> object_buffer_;
    ref<rhi::Buffer> fallback_lighting_buffer_;
    ref<rhi::Buffer> cluster_params_buffer_;
    ref<rhi::Buffer> shadow_data_buffer_;
    ref<rhi::Buffer> environment_data_buffer_;
    ref<rhi::Texture> fallback_shadow_texture_;
    ref<rhi::TextureView> fallback_shadow_view_;
    std::array<ref<rhi::Texture>, 2> fallback_environment_textures_;
    std::array<ref<rhi::TextureView>, 2> fallback_environment_views_;
    ref<rhi::Sampler> shadow_sampler_;
    ref<rhi::Sampler> environment_sampler_;
    rhi::Buffer* active_lights_{};
    rhi::Buffer* active_clusters_{};
    rhi::Buffer* active_indices_{};
    BufferSlice active_gpu_instances_;
    rhi::TextureView* active_shadow_atlas_{};
    std::array<rhi::TextureView*, 4> active_environment_{};
    u64 binding_generation_{};
    BufferSlice fallback_palette_;
    u32 object_stride_{};
    u32 object_capacity_{65536};
    std::vector<Generation> generations_;
    std::vector<BorrowedLayout> used_layouts_;
    std::vector<SkinPaletteHandle> used_palettes_;
    PipelineLayoutKey diagnostic_layout_key_;
    ref<rhi::ShaderModule> diagnostic_shader_;

    struct DiagnosticPipeline final {
        PipelineTargetSignature targets;
        u64 vertex_schema_id{};
        bool skinned{};
        ref<rhi::RenderPipeline> pipeline;
        rhi::SubmissionTicket last_used;
    };

    std::vector<DiagnosticPipeline> diagnostic_pipelines_;
};

} // namespace woki::gfx
