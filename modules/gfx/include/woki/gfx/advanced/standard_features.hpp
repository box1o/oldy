#pragma once

#include <array>
#include <functional>

#include <woki/math.hpp>

#include "feature.hpp"
#include "library.hpp"
#include "material_prepare.hpp"
#include "mesh_library.hpp"
#include "gpu_scene.hpp"
#include "render_graph.hpp"
#include "render_world.hpp"
#include "visibility.hpp"
#include "physical_bindings.hpp"
#include "temporal.hpp"
#include "environment.hpp"
#include "exposure.hpp"
#include "indirect_draw.hpp"

namespace woki::task {
class Scheduler;
}

namespace woki::gfx {

class ReadbackManager;

// HDR10/PQ is reserved for a future surface capability path and is rejected today.
enum class SdrTargetEncoding : u8 { Auto, SrgbAttachment, LinearUnorm, Hdr10Pq };

struct FeatureDiagnostic final {
    StringId feature;
    ViewId view;
    DiagnosticSeverity severity{DiagnosticSeverity::Error};
    std::string code;
    std::string message;
    std::string stage;
    std::string pass;
    std::string resource;
};

struct PreparedMeshDraw final {
    DrawPacket packet;
    MeshHandle mesh;
    MaterialInstanceHandle material_instance;
    u32 object_index{};
    u32 lod{};
};

struct MeshFeatureOutput final {
    std::array<std::vector<PreparedMeshDraw>, static_cast<size_t>(RenderPhase::Count)> phases;
};

struct DepthFeatureOutput final {
    GraphTexture depth;
    GraphTextureRef version;
};

struct VelocityOutputs final {
    GraphTexture velocity;
    GraphTextureRef version;
};

struct ShadowFeatureOutput final {
    GraphTexture atlas;
    GraphTextureRef version;
    u32 cascades{};
    std::array<f32, 4> split_depths{};
    std::array<math::mat4f, 4> cascade_matrices{};
    std::array<std::array<f32, 4>, 4> atlas_regions{};
};

struct ClusteredLightingOutput final {
    GraphBuffer lights;
    GraphBufferRef lights_version;
    GraphBuffer clusters;
    GraphBufferRef clusters_version;
    GraphBuffer indices;
    GraphBufferRef indices_version;
    GraphBuffer diagnostics;
    GraphBufferRef diagnostics_version;
    abi::ClusterParams params;
    u32 directional_count{};
    u32 local_count{};
};

struct SceneColorOutput final {
    GraphTexture color;
    GraphTextureRef version;
    bool hdr{};
};

struct ExposureOutput final {
    GraphBuffer exposure;
    GraphBufferRef version;
    f32 cpu_value{1.0F};
};

struct RenderTargetOutput final {
    GraphTexture color;
    GraphTextureRef initial;
    rhi::TextureFormat format{rhi::TextureFormat::Undefined};
    u32 samples{1};
    bool direct{};
    ExternalState final_state{ExternalState::Present};
    SdrTargetEncoding encoding{SdrTargetEncoding::Auto};
};

struct StandardFrameFeatureState final {
    const RenderWorldSnapshot* world{};
    const RenderView* view{};
    PipelineTargetSignature targets;
    MeshFeatureOutput draws;
    std::vector<GpuLightRecord> local_lights;
    std::array<GpuLightRecord, 4> directional_lights{};
    u32 directional_count{};
    u64 visible_objects{};
    u64 packet_count{};
    u64 draw_calls{};
    u64 skipped{};
    u64 cluster_overflow{};
    u64 shadow_draws{};
    u32 shadow_cascades{};
    u32 bloom_passes{};
    u64 fallback_materials{};
    u64 fallback_textures{};
    GpuVisibilityStats gpu_visibility;
    f32 exposure{1.0F};
    f32 delta_time{};
    u64 frame_number{};
    bool clear_output{};
    bool direct_output{};
    std::vector<GraphicsPipelineKey> used_pipelines;
    std::vector<FeatureDiagnostic>* diagnostics{};
};

enum class FullscreenProgram : u8 { Sky, BloomThreshold, BloomDownsample, BloomBlur, BloomComposite, Taa, Fxaa, Copy, ToneMap };

struct FullscreenDraw final {
    FullscreenProgram program{FullscreenProgram::ToneMap};
    GraphTextureRef source;
    GraphTextureRef secondary;
    GraphTextureRef tertiary;
    GraphTextureRef quaternary;
    rhi::TextureView* external_source{};
    rhi::TextureView* grading_lut{};
    std::array<f32, 4> parameters{};
    rhi::TextureFormat target_format{rhi::TextureFormat::Undefined};
    u32 sample_count{1};
};

// Immutable feature GPU generations are prepared from cooked products before a
// pipeline instance can be used by a frame.
class StandardFeaturePrograms {
public:
    virtual ~StandardFeaturePrograms() = default;
    [[nodiscard]] virtual Result<ref<const StandardFeaturePrograms>> PrepareReplacement(ref<rhi::Device> device) const = 0;
    [[nodiscard]] virtual Result<void> DrawFullscreen(RenderGraphContext& graph, const FullscreenDraw& draw) const = 0;
    [[nodiscard]] virtual Result<void> DispatchClusters(RenderGraphContext& graph,
        rhi::Buffer& lights,
        rhi::Buffer& clusters,
        rhi::Buffer& indices,
        rhi::Buffer& diagnostics,
        const abi::ClusterParams& params,
        u32 cluster_count,
        u32 light_count) const = 0;
    [[nodiscard]] virtual Result<void> DispatchExposure(RenderGraphContext& graph,
        rhi::TextureView& source,
        rhi::Buffer& histogram,
        rhi::Buffer& exposure,
        const ExposureSettings& settings,
        f32 delta_time,
        u32 width,
        u32 height,
        bool reduce) const = 0;
    [[nodiscard]] virtual bool GpuDrivenReady() const noexcept = 0;
    [[nodiscard]] virtual Result<void> DispatchHiZ(RenderGraphContext& graph, rhi::TextureView& source, rhi::TextureView& destination, u32 width, u32 height, bool depth_source) const = 0;
    [[nodiscard]] virtual Result<void> DispatchVisibility(RenderGraphContext& graph,
        rhi::Buffer& candidates,
        rhi::Buffer& lods,
        rhi::Buffer& meshlet_bounds,
        rhi::TextureView& previous_hiz,
        rhi::Buffer& commands,
        rhi::Buffer& counts,
        rhi::Buffer& visible_instances,
        rhi::Buffer& visible_meshlets,
        rhi::Buffer& diagnostics,
        const GpuVisibilityParams& params) const = 0;
    virtual void OnSubmitted(rhi::SubmissionTicket submission) const = 0;
    [[nodiscard]] virtual Result<MaterialShaderIdentity> ResolveMaterialShader(asset::AssetId shader) const = 0;
    [[nodiscard]] virtual Result<BorrowedShader> BorrowShader(ContentHash product, u64 generation) const = 0;
    [[nodiscard]] virtual const asset::Product& DiagnosticShaderProduct() const noexcept = 0;
    [[nodiscard]] virtual const asset::Product* ProgramProduct(std::string_view name) const noexcept = 0;
};

// Loads one complete, immutable generation from cooked/programs.json. Product
// locations themselves are resolved through the generic AssetManifest. No
// authored shader source is consulted at runtime.
[[nodiscard]] Result<ref<const StandardFeaturePrograms>> LoadStandardFeaturePrograms(const asset::Vfs& vfs, ref<rhi::Device> device, const asset::AssetUri& manifest);

struct StandardFeatureServices final {
    MeshLibrary* meshes{};
    MaterialPreparation* materials{};
    MaterialLibrary* material_library{};
    PipelineCache* pipelines{};
    VisibilityService* visibility{};
    task::Scheduler* scheduler{};
    StandardFrameFeatureState* frame{};
    PhysicalBindings* bindings{};
    ReadbackManager* readbacks{};
    TextureLibrary* textures{};
    RenderHistoryRegistry* histories{};
    GpuScene* gpu_scene{};
    std::function<Result<ResolvedEnvironment>(EnvironmentHandle)> resolve_environment;
    TextureHandle grading_lut;
    const CapabilitySet* capabilities{};
    ref<const StandardFeaturePrograms> programs;
    ref<rhi::Device> device;
    ref<DeferredReleaseQueue> releases;
    std::function<Result<ref<rhi::RenderPipeline>>(const GraphicsPipelineKey&, rhi::PipelineLayout&, const VertexSchema&, const MaterialRenderState&)> create_graphics_pipeline;
};

[[nodiscard]] Result<void> RegisterConcreteStandardFeatures(FeatureRegistry& registry);

} // namespace woki::gfx
