#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <map>
#include <mutex>
#include <set>
#include <tuple>
#include <unordered_map>

#include <woki/gfx/runtime.hpp>
#include <woki/gfx/advanced/animation.hpp>
#include <woki/gfx/advanced/runtime.hpp>
#include <woki/gfx/advanced/render_scene.hpp>
#include <woki/gfx/advanced/graph_executor.hpp>
#include <woki/gfx/advanced/gpu_scene.hpp>
#include <woki/gfx/advanced/library.hpp>
#include <woki/gfx/advanced/pipeline_library.hpp>
#include <woki/rhi/surface.hpp>
#include <woki/rhi/instance.hpp>
#include <woki/rhi/swapchain.hpp>
#include <woki/rhi/render_pass_encoder.hpp>
#include <woki/rhi/validation.hpp>
#include <woki/window/window.hpp>

#include "runtime_facade.hpp"
#include "canvas_feature.hpp"
#include "readback_manager.hpp"
#include "../canonical_hash.hpp"
#include "../gfx_util.hpp"


namespace woki::gfx {

struct FrameOutputBinding final {
    ViewId view;
    ref<rhi::Texture> texture;
    ref<rhi::TextureView> view_texture;
    rhi::TextureFormat format{rhi::TextureFormat::Undefined};
    u32 width{};
    u32 height{};
    u32 sample_count{1};
    bool present{};
    bool clear{};
    SurfaceHandle surface;
    OffscreenTargetHandle offscreen;
    SdrTargetEncoding encoding{SdrTargetEncoding::Auto};
    ExternalState initial_state{ExternalState::Undefined};
    ExternalState final_state{ExternalState::ShaderRead};
};

struct CoreFrameRequest final {
    struct Scene final {
        SceneHandle handle;
        ref<RenderScene> value;
    };

    std::span<const Scene> scenes;
    std::span<const RenderViewFamily> view_families;
    std::span<const FrameOutputBinding> outputs;
    f32 time{};
    f32 delta_time{};
    const CanvasFrame* canvas{};
    const CanvasTarget* canvas_target{};
};

struct CoreFrameStats final {
    struct CpuStageTiming final {
        std::string name;
        f64 milliseconds{};
    };

    u64 extracted_objects{};
    u64 visible_objects{};
    u64 packets{};
    u64 draw_calls{};
    u64 skipped_pending_resources{};
    u64 cluster_overflow{};
    u64 shadow_draws{};
    u32 shadow_cascades{};
    u32 bloom_passes{};
    u64 fallback_materials{};
    u64 fallback_textures{};
    GpuVisibilityStats gpu_visibility;
    UploadStats uploads;
    u64 graph_hash{};
    u64 transient_bytes{};
    rhi::SubmissionEpoch submission_epoch;
    RenderRecoveryState recovery_state{RenderRecoveryState::Ready};
    std::vector<CpuStageTiming> cpu_stages;
};

struct CoreFrameResult final {
    rhi::SubmissionTicket submission;
    std::vector<ViewId> submitted_views;
    std::vector<FeatureDiagnostic> diagnostics;
    CoreFrameStats stats;
};

struct BootstrapDescriptor final {
    RenderDeviceBundle device;
    ref<RenderDeviceFactory> recovery_factory;
    ref<RenderRuntimeAdvancedDescriptor> advanced;
    ref<void> owned_assets;
};

struct SceneRuntimeState final {
    std::mutex mutex;
    SceneHandle handle;
    ref<RenderScene> scene;
    std::unordered_map<RenderObjectId, u64> object_versions;
    std::unordered_map<RenderLightId, u64> light_versions;
    bool live{true};
};


[[nodiscard]] rhi::PresentMode ToRhiPresentMode(PresentMode mode);
[[nodiscard]] Result<ref<rhi::Surface>> CreatePlatformSurface(rhi::Instance& instance, const SurfaceSource& source);

struct RenderRuntime::Impl final {
    struct SceneSlot final {
        u32 generation{1};
        ref<SceneRuntimeState> state;
        std::string label;
    };

    struct ViewSlot final {
        u32 generation{1};
        std::optional<ViewDescriptor> descriptor;
        ViewHistoryId history;
        u64 version{1};
    };

    struct SurfaceSlot final {
        u32 generation{1};
        SurfaceDescriptor descriptor;
        ref<rhi::Surface> surface;
        scope<rhi::Swapchain> swapchain;
        SurfaceState state{SurfaceState::Ready};
        std::optional<rhi::Frame> acquired;
        rhi::SubmissionTicket last_used;
    };

    struct OffscreenSlot final {
        u32 generation{1};
        std::optional<OffscreenTargetDescriptor> descriptor;
        ref<rhi::Texture> texture;
        ref<rhi::TextureView> view;
        u64 version{1};
        rhi::SubmissionTicket last_used;
    };

    struct AnimationSlot final {
        u32 generation{1};
        scope<Animator> animator;
        SkinPaletteHandle palette;
        u64 pose_generation{1};
    };

    struct FeaturePoolKey final {
        ContentHash config;
        FeatureScope scope{FeatureScope::Pipeline};
        ContentHash identity;
        u64 version{};
        [[nodiscard]] friend auto operator<=>(const FeaturePoolKey&, const FeaturePoolKey&) = default;
    };

    struct FeaturePoolEntry final {
        ref<const RenderFeature> feature;
        u64 last_frame{};
    };

    struct Executable final {
        u64 signature{};
        PipelineInstance pipeline;
        GraphTexture output;
        GraphTextureDesc output_descriptor;
        scope<GraphExecutor> executor;
        bool direct{};
    };

    scope<rhi::Instance> instance;
    scope<rhi::Adapter> adapter;
    ref<rhi::Device> device;
    ref<RenderDeviceFactory> recovery_factory;
    ref<void> owned_assets;
    asset::AssetServices assets;
    task::Scheduler* scheduler{};
    task::IoExecutor* io{};
    task::CompletionQueue* completions{};
    CapabilitySet capabilities;
    PreparationFailurePolicy upload_failure_policy{PreparationFailurePolicy::Required};
    PreparationFailurePolicy material_failure_policy{PreparationFailurePolicy::Required};
    RenderRuntimeDesc resource_descriptor;
    scope<RenderRuntimeServices> resources;
    ShaderLibrary shaders;
    ShaderAssetHandle diagnostic_shader;
    asset::Product diagnostic_shader_product;
    LayoutCache layouts;
    PipelineCache pipeline_cache;
    scope<TextureLibrary> textures;
    scope<MaterialLibrary> materials;
    MaterialBindingCache material_bindings;
    MaterialPreparation::ResolveShader resolve_material_shader;
    scope<MaterialPreparation> material_preparation;
    scope<MeshLibrary> meshes;
    scope<SkinPaletteRegistry> palettes;
    scope<PhysicalBindings> bindings;
    scope<RenderHistoryRegistry> histories;
    scope<ReadbackManager> feature_readbacks;
    scope<RenderWorldServices> world;
    std::map<SceneHandle, RenderWorldBuilder> scene_worlds;
    FeatureRegistry features;
    RenderPipelineLibrary pipelines;
    StandardFrameFeatureState frame_state;
    StandardFeatureServices feature_services;
    std::map<ViewId, scope<Executable>> executables;
    std::map<FeaturePoolKey, FeaturePoolEntry> feature_pool;
    u64 frame_number{};
    RenderRecoveryState recovery_state{RenderRecoveryState::Ready};
    u64 reported_fallback_textures{};
    rhi::SubmissionTicket last_submission;
    std::function<void()> stop_requests;
    std::function<void()> join_workers;
    std::function<void()> drain_publications;
    std::function<Result<void>()> flush_asset_manifest;
    std::function<Result<void>(rhi::Device&, rhi::SubmissionTicket)> wait_for_shutdown_submission;
    bool shutdown{};

    std::vector<SceneSlot> scene_slots;
    std::vector<u32> free_scenes;
    std::vector<CoreFrameRequest::Scene> retired_scenes;
    std::vector<ViewSlot> view_slots;
    std::vector<u32> free_views;
    std::vector<SurfaceSlot> surface_slots;
    std::vector<u32> free_surfaces;
    std::vector<OffscreenSlot> offscreen_slots;
    std::vector<u32> free_offscreens;
    std::vector<AnimationSlot> animation_slots;
    Capabilities public_capabilities;
    Diagnostics diagnostics;
    scope<ComputeService> compute;
    scope<ReadbackService> readback_service;
    scope<CanvasFeature> canvas;

    Impl(BootstrapDescriptor descriptor, scope<RenderRuntimeServices> runtime, scope<TextureLibrary> texture_library);

    [[nodiscard]] Result<void> RecoverDevice();
    void MarkDeviceLost() noexcept;
    [[nodiscard]] Result<void> RestoreOffscreen(OffscreenSlot& target);
    [[nodiscard]] Result<CoreFrameResult> Frame(const CoreFrameRequest& request);
    [[nodiscard]] Result<rhi::SubmissionTicket> PumpFacade();
    [[nodiscard]] Result<ref<rhi::RenderPipeline>> CreateGraphicsPipeline(
        const GraphicsPipelineKey& key,
        rhi::PipelineLayout& layout,
        const VertexSchema& schema,
        const MaterialRenderState& state);
    [[nodiscard]] Result<Executable*> GetExecutable(
        const RenderWorldSnapshot& world_snapshot,
        const RenderView& view,
        const FrameOutputBinding& output,
        u64 family_id);
};

} // namespace woki::gfx
