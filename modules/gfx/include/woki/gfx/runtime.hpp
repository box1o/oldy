#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <woki/asset.hpp>
#include <woki/core.hpp>
#include <woki/math.hpp>

#include "compute.hpp"
#include "canvas.hpp"
#include "diagnostics.hpp"
#include "presentation.hpp"
#include "readback.hpp"
#include "scene.hpp"
#include "view.hpp"

namespace woki::gfx {

class RenderDeviceFactory;
struct RenderRuntimeAdvancedDescriptor;

enum class RenderBackend : u8 { Automatic, WebGPU, Vulkan, Metal, D3D12, OpenGL, Null };
enum class ValidationMode : u8 { Disabled, Standard, Full };
enum class RenderRecoveryState : u8 { Ready, DeviceLost, Recovering, Failed };

struct RuntimeAssetDescriptor final {
    std::vector<std::filesystem::path> roots;
    // Persistent cache/database roots and package selection share this runtime
    // target descriptor; callers must not derive them from the working directory.
    std::filesystem::path cache;
    std::filesystem::path manifest;
    std::string target{"native"};
    std::string platform{"generic"};
    ContentHash capability_fingerprint;
    asset::AssetPriority package_priority{asset::AssetPriority::High};
};

struct RenderRuntimeDescriptor final {
    RenderBackend backend{RenderBackend::Automatic};
    ValidationMode validation{ValidationMode::Standard};
    u32 frames_in_flight{3};
    u32 worker_count{};
    u32 io_worker_count{1};
    RuntimeAssetDescriptor assets;
    std::vector<SurfaceDescriptor> surfaces;
    ref<RenderDeviceFactory> device_factory;
    ref<RenderRuntimeAdvancedDescriptor> advanced;
};

struct SceneDescriptor final {
    std::string label;
};

struct AnimationPlaybackInfo final {
    AnimationPlaybackHandle playback;
    SkeletonHandle skeleton;
    AnimationClipHandle clip;
    SkinPaletteHandle palette;
    RenderBounds bounds;
    math::mat4f model_transform{math::mat4f::identity()};
    std::string clip_name;
    f32 duration{};
    u32 joint_count{};
    u32 mesh_count{};
    u32 lod_count{};
};

struct ViewDescriptor final {
    SceneHandle scene;
    u64 family{};
    CameraState camera;
    PixelRect viewport;
    u32 render_width{};
    u32 render_height{};
    u64 visibility_mask{~u64{0}};
    u64 layer_mask{~u64{0}};
    f32 exposure{1.0F};
    PipelineHandle pipeline;
    ViewOutput output;
    TemporalPolicy temporal;
    ViewFlags flags{ViewFlags::None};
    bool active{true};
};

struct RenderFrameRequest final {
    std::span<const SceneHandle> scenes;
    std::span<const ViewId> views;
    f32 time{};
    f32 delta_time{};
    std::optional<CanvasFrame> canvas;
};

struct ViewFrameDiagnostic final {
    ViewId view;
    bool submitted{};
    SurfaceState surface_state{SurfaceState::Ready};
    GraphStats graph;
    std::vector<std::string> messages;
};

struct FrameResult final {
    std::vector<GpuSubmissionId> submissions;
    std::vector<ViewFrameDiagnostic> views;
    RuntimeStats stats;
};

class SceneMutation final {
public:
    SceneMutation();
    SceneMutation(SceneMutation&&) noexcept;
    SceneMutation& operator=(SceneMutation&&) noexcept;
    ~SceneMutation();
    SceneMutation(const SceneMutation&) = delete;
    SceneMutation& operator=(const SceneMutation&) = delete;

    [[nodiscard]] Result<RenderObjectId> CreateObject(RenderObjectData data);
    [[nodiscard]] Result<void> UpdateObject(RenderObjectId id, RenderObjectPatch patch);
    [[nodiscard]] Result<void> DestroyObject(RenderObjectId id);
    [[nodiscard]] Result<RenderLightId> CreateLight(RenderLightData data);
    [[nodiscard]] Result<void> UpdateLight(RenderLightId id, RenderLightData data);
    [[nodiscard]] Result<void> DestroyLight(RenderLightId id);
    [[nodiscard]] Result<void> Commit();
    void Cancel() noexcept;

private:
    friend class RenderRuntime;
    struct Impl;
    explicit SceneMutation(scope<Impl> impl);
    scope<Impl> impl_;
};

class RenderRuntime final {
public:
    [[nodiscard]] static Result<scope<RenderRuntime>> Create(RenderRuntimeDescriptor descriptor);
    ~RenderRuntime();
    RenderRuntime(const RenderRuntime&) = delete;
    RenderRuntime& operator=(const RenderRuntime&) = delete;

    [[nodiscard]] Result<void> Shutdown();
    [[nodiscard]] Result<FrameResult> RenderFrame(const RenderFrameRequest& request = {});

    [[nodiscard]] Result<SceneHandle> CreateScene(SceneDescriptor descriptor = {});
    [[nodiscard]] Result<void> DestroyScene(SceneHandle scene);
    [[nodiscard]] Result<SceneMutation> MutateScene(SceneHandle scene);

    [[nodiscard]] Result<ViewId> CreateView(ViewDescriptor descriptor);
    [[nodiscard]] Result<void> UpdateView(ViewId view, ViewDescriptor descriptor);
    [[nodiscard]] Result<void> DestroyView(ViewId view);

    [[nodiscard]] Result<SurfaceHandle> CreateSurface(SurfaceDescriptor descriptor);
    [[nodiscard]] Result<void> ReconfigureSurface(SurfaceHandle surface, u32 width, u32 height);
    [[nodiscard]] Result<void> DestroySurface(SurfaceHandle surface);
    [[nodiscard]] SurfaceState GetSurfaceState(SurfaceHandle surface) const noexcept;
    [[nodiscard]] std::vector<SurfaceHandle> Surfaces() const;

    [[nodiscard]] Result<OffscreenTargetHandle> CreateOffscreenTarget(OffscreenTargetDescriptor descriptor);
    [[nodiscard]] Result<void> ResizeOffscreenTarget(OffscreenTargetHandle target, u32 width, u32 height);
    [[nodiscard]] Result<void> DestroyOffscreenTarget(OffscreenTargetHandle target);

    [[nodiscard]] Result<PipelineHandle> CreatePipeline();
    [[nodiscard]] Result<void> PublishPipeline(PipelineHandle pipeline, const asset::Product& product);
    [[nodiscard]] Result<MeshHandle> RequestMesh(asset::AssetId id);
    [[nodiscard]] MeshState MeshStatus(MeshHandle handle) const noexcept;
    [[nodiscard]] Result<MaterialInstanceHandle> RequestMaterial(asset::AssetId id);
    [[nodiscard]] SkinPaletteHandle CreateSkinPalette(u32 joint_count);
    [[nodiscard]] Result<void> UpdateSkinPalette(SkinPaletteHandle handle, std::span<const math::mat4f> matrices, u64 source_generation);
    [[nodiscard]] Result<AnimationPlaybackInfo> CreateAnimationPlayback(MeshHandle mesh);
    [[nodiscard]] Result<void> AdvanceAnimation(AnimationPlaybackHandle playback, f32 delta_seconds);
    [[nodiscard]] Result<void> SetAnimationPaused(AnimationPlaybackHandle playback, bool paused);
    [[nodiscard]] Result<void> RestartAnimation(AnimationPlaybackHandle playback);
    [[nodiscard]] Result<void> ChangeAnimationSpeed(AnimationPlaybackHandle playback, f32 amount);

    [[nodiscard]] ComputeService& Compute() noexcept;
    [[nodiscard]] ReadbackService& Readbacks() noexcept;
    [[nodiscard]] const Capabilities& GetCapabilities() const noexcept;
    [[nodiscard]] const Diagnostics& GetDiagnostics() const noexcept;
    [[nodiscard]] RenderRecoveryState RecoveryState() const noexcept;

private:
    friend class SceneMutation;
    struct Impl;
    explicit RenderRuntime(scope<Impl> impl);
    scope<Impl> impl_;
};

} // namespace woki::gfx
