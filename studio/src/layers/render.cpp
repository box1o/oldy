#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>

#include <woki/asset.hpp>
#include <woki/gfx.hpp>
#include <woki/gfx/advanced/pipeline_product.hpp>
#include <woki/math.hpp>

#include "render.hpp"

namespace woki {
namespace {

const asset::AssetId kClimbingId = *asset::AssetId::Parse("77ee2538-19d3-57ef-956f-890eac1ac253");

Result<std::filesystem::path> StudioAssetRoot() {
#ifdef __EMSCRIPTEN__
    return std::filesystem::path{WOKI_STUDIO_ASSET_ROOT};
#else
    auto executable = paths::ExecutablePath();
    if (!executable)
        return Err(executable.error());
    const auto installed = executable->parent_path() / std::filesystem::path{WOKI_STUDIO_INSTALLED_ASSET_DIR};
    std::error_code error;
    if (std::filesystem::is_directory(installed, error))
        return installed.lexically_normal();
    return std::filesystem::path{WOKI_STUDIO_DEVELOPMENT_ASSET_ROOT};
#endif
}

Result<asset::Product> ReadCompatibilityPipeline(const asset::Vfs& vfs, const asset::AssetManifest& manifest) {
    for (const auto& entry : manifest.Entries()) {
        if (entry.type != gfx::kPipelineProductType)
            continue;
        auto reader = asset::ProductReader::Open(
            vfs,
            entry.locator,
            {.max_payload_bytes = 1024U * 1024U * 1024U, .max_decompressed_chunk_bytes = 1024U * 1024U * 1024U}
        );
        if (!reader)
            return Err(std::move(reader).error());
        auto product = reader->ReadProduct();
        if (!product)
            return Err(std::move(product).error());
        auto pipeline = gfx::ParsePipelineProduct(product->payload);
        if (!pipeline)
            return Err(std::move(pipeline).error());
        if (pipeline->render_path == ToStringId("compatibility-forward"))
            return product;
    }
    return Err(ErrorCode::FileNotFound, "compatibility pipeline is absent from the shipping manifest");
}

math::vec3f TransformPoint(const math::mat4f& transform, const math::vec3f& point) noexcept {
    const auto value = transform * math::vec4f{point.x, point.y, point.z, 1.0F};
    return {value.x, value.y, value.z};
}

gfx::RenderBounds TransformBounds(const gfx::RenderBounds& bounds, const math::mat4f& transform) noexcept {
    const std::array corners{
        math::vec3f{bounds.minimum.x, bounds.minimum.y, bounds.minimum.z},
        math::vec3f{bounds.maximum.x, bounds.minimum.y, bounds.minimum.z},
        math::vec3f{bounds.minimum.x, bounds.maximum.y, bounds.minimum.z},
        math::vec3f{bounds.maximum.x, bounds.maximum.y, bounds.minimum.z},
        math::vec3f{bounds.minimum.x, bounds.minimum.y, bounds.maximum.z},
        math::vec3f{bounds.maximum.x, bounds.minimum.y, bounds.maximum.z},
        math::vec3f{bounds.minimum.x, bounds.maximum.y, bounds.maximum.z},
        math::vec3f{bounds.maximum.x, bounds.maximum.y, bounds.maximum.z},
    };

    gfx::RenderBounds result;
    result.minimum = TransformPoint(transform, corners.front());
    result.maximum = result.minimum;
    for (const auto& corner : corners) {
        const auto point = TransformPoint(transform, corner);
        result.minimum = math::min(result.minimum, point);
        result.maximum = math::max(result.maximum, point);
    }
    result.center = (result.minimum + result.maximum) * 0.5F;
    const auto extent = result.maximum - result.center;
    result.radius = math::length(extent);
    return result;
}

class StudioSurfaceSource final : public gfx::SurfaceSource {
public:
    explicit StudioSurfaceSource(Window& window)
        : window_(&window) {}

    gfx::SurfacePlatformSource Describe() const noexcept override {
        return {
            .platform = gfx::SurfacePlatform::WokiWindow,
            .display = nullptr,
            .window = window_,
            .selector = {},
        };
    }

    std::string Label() const override {
        return "Studio window";
    }

private:
    Window* window_{};
};

} // namespace

struct StudioRenderDemo final {
    ref<asset::Vfs> vfs;
    scope<gfx::RenderRuntime> runtime;
    gfx::SurfaceHandle surface;
    gfx::OffscreenTargetHandle scene_target;
    gfx::SceneHandle scene;
    gfx::PipelineHandle pipeline;
    gfx::MeshHandle mesh;
    gfx::MaterialInstanceHandle material;
    gfx::RenderObjectId object;
    std::array<gfx::ViewId, 4> view_ids{};
    std::array<gfx::ViewDescriptor, 4> views{};
    gfx::AnimationPlaybackHandle animation;
    gfx::SkinPaletteHandle palette;
    gfx::RenderBounds bounds{{}, 1.0F, {-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}};
    math::mat4f model_transform{math::mat4f::identity()};
    bool animation_paused{};
    bool animation_initialized{};
    bool static_diagnostic_logged{};
    u64 last_visible_objects{~u64{0}};
    u64 last_draw_packets{~u64{0}};
    u64 last_draw_calls{~u64{0}};
    u64 last_skipped_resources{~u64{0}};
    u64 last_fallback_textures{~u64{0}};

    static Result<scope<StudioRenderDemo>> Create(
        ref<const gfx::SurfaceSource> source,
        const std::filesystem::path& root,
        const u32 width,
        const u32 height,
        const bool offscreen
    ) {
        auto result = createScope<StudioRenderDemo>();
        result->vfs = createRef<asset::Vfs>();
        ref<asset::DirectoryMount> mount;
        TRY_ASSIGN(mount, asset::DirectoryMount::Create(root));
        TRY_VOID(result->vfs->MountAt("studio-engine", asset::AssetScheme::Engine, {}, 0, mount));
        auto manifest_uri = asset::AssetUri::Parse("engine://cooked/manifest.wkam");
        if (!manifest_uri)
            return Err(manifest_uri.error());
        asset::AssetManifest manifest;
        TRY_ASSIGN(manifest, asset::AssetManifest::Load(*result->vfs, *manifest_uri));
        gfx::RenderRuntimeDescriptor descriptor;
        descriptor.assets.roots.push_back(root);
        TRY_ASSIGN(result->runtime, gfx::RenderRuntime::Create(std::move(descriptor)));
        TRY_ASSIGN(
            result->surface,
            result->runtime
                ->CreateSurface({.source = std::move(source), .width = width, .height = height, .label = "Studio"})
        );
        if (offscreen)
            TRY_ASSIGN(
                result->scene_target,
                result->runtime->CreateOffscreenTarget(
                    {.width = width,
                        .height = height,
                        .format = gfx::PixelFormat::RGBA8Unorm,
                        .sampled = true,
                        .label = "Scene viewport"}
                )
            );
        TRY_ASSIGN(result->scene, result->runtime->CreateScene({.label = "Studio demo"}));
        TRY_ASSIGN(result->pipeline, result->runtime->CreatePipeline());
        asset::Product pipeline_product;
        TRY_ASSIGN(pipeline_product, ReadCompatibilityPipeline(*result->vfs, manifest));
        TRY_VOID(result->runtime->PublishPipeline(result->pipeline, pipeline_product));
        TRY_ASSIGN(result->mesh, result->runtime->RequestMesh(kClimbingId));
        gfx::SceneMutation mutation;
        TRY_ASSIGN(mutation, result->runtime->MutateScene(result->scene));
        gfx::RenderObjectData object_data;
        object_data.mesh = result->mesh;
        // Leave the material unset so the mesh feature resolves each cooked
        // submesh's imported material (including its base-color texture).
        object_data.material = {};
        object_data.bounds = result->bounds;
        TRY_ASSIGN(result->object, mutation.CreateObject(object_data));

        gfx::RenderLightId light;
        TRY_ASSIGN(
            light,
            mutation.CreateLight(
                {
                    .type = gfx::LightType::Directional,
                    .direction = {-0.45F, -0.80F, -0.40F},
                    .color = {1.0F, 0.96F, 0.90F},
                    .intensity = 4.0F,
                }
            )
        );
        TRY_ASSIGN(
            light,
            mutation.CreateLight(
                {
                    .type = gfx::LightType::Directional,
                    .direction = {0.55F, -0.35F, 0.75F},
                    .color = {0.52F, 0.68F, 1.0F},
                    .intensity = 1.5F,
                }
            )
        );
        static_cast<void>(light);
        TRY_VOID(mutation.Commit());
        for (u32 index = 0; index < result->view_ids.size(); ++index) {
            gfx::ViewDescriptor view;
            view.scene = result->scene;
            view.family = 1;
            view.pipeline = result->pipeline;
            view.output = offscreen ? gfx::ViewOutput{gfx::OffscreenOutput{result->scene_target}}
                                    : gfx::ViewOutput{gfx::SurfaceOutput{result->surface}};
            view.active = index == 0;
            result->views[index] = view;
            TRY_ASSIGN(result->view_ids[index], result->runtime->CreateView(view));
        }
        slog::Info("Loading the packaged climbing mesh asynchronously");
        return Ok(std::move(result));
    }

    void PatchObject(gfx::RenderObjectPatch patch) {
        auto mutation = runtime->MutateScene(scene);
        if (!mutation) {
            slog::Warn("Scene mutation failed: {}", mutation.error().Message());
            return;
        }
        if (auto updated = mutation->UpdateObject(object, std::move(patch)); !updated)
            slog::Warn("Object update failed: {}", updated.error().Message());
        else if (auto committed = mutation->Commit(); !committed)
            slog::Warn("Scene commit failed: {}", committed.error().Message());
    }

    [[nodiscard]] bool PumpAssets() {
        if (animation_initialized)
            return false;
        auto prepared = runtime->CreateAnimationPlayback(mesh);
        if (!prepared) {
            if (runtime->MeshStatus(mesh) == gfx::MeshState::Failed && !static_diagnostic_logged) {
                slog::Warn("climbing mesh has no usable animation: {}", prepared.error().Message());
                static_diagnostic_logged = true;
                animation_initialized = true;
            }
            return false;
        }
        animation = prepared->playback;
        palette = prepared->palette;
        const auto imported_bounds = TransformBounds(prepared->bounds, prepared->model_transform);
        const f32 normalization = 2.0F / std::max(imported_bounds.radius, 0.001F);
        model_transform = math::scale(normalization) * math::translate(-imported_bounds.center)
                          * prepared->model_transform;
        bounds = TransformBounds(prepared->bounds, model_transform);
        slog::Info(
            "climbing world bounds: center=({:.3f}, {:.3f}, {:.3f}) radius={:.3f} min=({:.3f}, {:.3f}, "
            "{:.3f}) max=({:.3f}, {:.3f}, {:.3f})",
            bounds.center.x,
            bounds.center.y,
            bounds.center.z,
            bounds.radius,
            bounds.minimum.x,
            bounds.minimum.y,
            bounds.minimum.z,
            bounds.maximum.x,
            bounds.maximum.y,
            bounds.maximum.z
        );
        gfx::RenderObjectPatch patch;
        patch.palette = palette;
        patch.transform = model_transform;
        patch.bounds = bounds;
        PatchObject(std::move(patch));
        animation_initialized = true;
        slog::Info(
            "climbing ready: meshes={} LODs={} joints={}; playing '{}' ({:.3f}s)",
            prepared->mesh_count,
            prepared->lod_count,
            prepared->joint_count,
            prepared->clip_name,
            prepared->duration
        );
        return true;
    }

    [[nodiscard]] bool Advance(const f32 delta_seconds) {
        const bool became_ready = PumpAssets();
        if (!animation.IsValid())
            return became_ready;
        if (auto advanced = runtime->AdvanceAnimation(animation, delta_seconds); !advanced) {
            slog::Warn("Animation evaluation failed: {}", advanced.error().Message());
            return became_ready;
        }
        gfx::RenderObjectPatch patch;
        patch.palette = palette;
        PatchObject(std::move(patch));
        return became_ready;
    }

    void TogglePause() {
        if (animation.IsValid()) {
            animation_paused = !animation_paused;
            static_cast<void>(runtime->SetAnimationPaused(animation, animation_paused));
        }
    }

    void Restart() {
        if (animation.IsValid())
            static_cast<void>(runtime->RestartAnimation(animation));
    }

    void ChangeSpeed(const f32 amount) {
        if (animation.IsValid())
            static_cast<void>(runtime->ChangeAnimationSpeed(animation, amount));
    }
};

RenderLayer::RenderLayer(const bool offscreen)
    : offscreen_(offscreen) {}

RenderLayer::~RenderLayer() {
    Shutdown();
}

void RenderLayer::OnAttach(Context& ctx) {
    WOKI_ASSERT(ctx.window != nullptr);
    if (auto initialized = Initialize(*ctx.window); !initialized) {
        slog::Error("Render layer initialization failed: {}", initialized.error().Message());
        Shutdown();
    }
}

void RenderLayer::OnDetach(Context&) {
    Shutdown();
}

void RenderLayer::OnUpdate(Context&, const f64 delta_ms) {
    if (!ready_ || minimized_)
        return;
    if (auto rendered = RenderFrame(static_cast<f32>(std::clamp(delta_ms * 0.001, 0.0, 0.1))); !rendered)
        slog::Warn("Render frame failed: {}", rendered.error().Message());
}

void RenderLayer::SelectViewport(const f32 logical_x, const f32 logical_y) noexcept {
    for (u32 index = 0; index < camera_viewports_.size(); ++index) {
        const auto hit = camera_viewports_[index].HitTest(
            logical_x - scene_origin_x_,
            logical_y - scene_origin_y_,
            window_->GetContentScaleX(),
            window_->GetContentScaleY(),
            width_,
            height_
        );
        if (hit && *hit) {
            selected_view_ = index;
            return;
        }
    }
}

void RenderLayer::OnEvent(Context&, events::Event& event) {
    if (!ready_)
        return;
    switch (event.GetEventType()) {
        case events::EventType::kFramebufferResized: {
            const auto& value = static_cast<const events::FramebufferResizeEvent&>(event);
            if (auto resized = Resize(value.width, value.height); !resized)
                slog::Warn("Render resize failed: {}", resized.error().Message());
            break;
        }
        case events::EventType::kWindowMinimized:
            minimized_ = true;
            ClearInput();
            break;
        case events::EventType::kWindowRestored:
            minimized_ = false;
            break;
        case events::EventType::kWindowLostFocus:
        case events::EventType::kPointerLeft:
            ClearInput();
            break;
        case events::EventType::kPointerDown: {
            const auto& value = static_cast<const events::PointerDownEvent&>(event).pointer_data;
            if (value.button == events::PointerButton::kMiddle) {
                SelectViewport(value.x, value.y);
                middle_drag_ = true;
                event.handled = true;
            }
            break;
        }
        case events::EventType::kPointerUp: {
            const auto& value = static_cast<const events::PointerUpEvent&>(event).pointer_data;
            if (value.button == events::PointerButton::kMiddle) {
                middle_drag_ = false;
                event.handled = true;
            }
            break;
        }
        case events::EventType::kPointerMoved: {
            const auto& value = static_cast<const events::PointerMoveEvent&>(event).pointer_data;
            if (!middle_drag_)
                SelectViewport(value.x, value.y);
            if (middle_drag_) {
                auto& orbit = orbits_[selected_view_];
                if (left_shift_down_ || right_shift_down_)
                    orbit.Rotate(value.delta_x, value.delta_y);
                else if (const auto* projection = std::get_if<gfx::OrthographicCamera>(
                             &camera_projections_[selected_view_]
                         )) {
                    const f32 scale = projection->height
                                      / static_cast<f32>(std::max(1U, pixel_viewports_[selected_view_].height));
                    orbit.target += camera_poses_[selected_view_].Right()
                                        * (-value.delta_x * window_->GetContentScaleX() * scale)
                                    + camera_poses_[selected_view_].Up()
                                          * (value.delta_y * window_->GetContentScaleY() * scale);
                } else
                    orbit.Pan(value.delta_x, value.delta_y, camera_poses_[selected_view_]);
                event.handled = true;
            }
            break;
        }
        case events::EventType::kScrolled: {
            const auto& value = static_cast<const events::ScrollEvent&>(event);
            if (auto* projection = std::get_if<gfx::OrthographicCamera>(&camera_projections_[selected_view_]))
                projection->height = std::clamp(projection->height * std::exp(-value.delta_y * 0.12F), 0.05F, 10000.0F);
            else
                orbits_[selected_view_].Dolly(value.delta_y);
            event.handled = true;
            break;
        }
        case events::EventType::kPinch: {
            const auto& value = static_cast<const events::PinchEvent&>(event);
            if (value.scale_delta <= 0.0F)
                break;
            SelectViewport(value.gesture.center_x, value.gesture.center_y);
            if (auto* projection = std::get_if<gfx::OrthographicCamera>(&camera_projections_[selected_view_]))
                projection->height = std::clamp(projection->height / value.scale_delta, 0.05F, 10000.0F);
            else
                orbits_[selected_view_].Dolly(std::log(value.scale_delta) * 8.0F);
            event.handled = true;
            break;
        }
        case events::EventType::kPan: {
            const auto& value = static_cast<const events::PanEvent&>(event).gesture;
            if (value.phase == events::GesturePhase::kBegin)
                SelectViewport(value.center_x, value.center_y);
            if (value.phase == events::GesturePhase::kBegin || value.phase == events::GesturePhase::kUpdate) {
                auto& orbit = orbits_[selected_view_];
                if (const auto* projection = std::get_if<gfx::OrthographicCamera>(
                        &camera_projections_[selected_view_]
                    )) {
                    const f32 scale = projection->height
                                      / static_cast<f32>(std::max(1U, pixel_viewports_[selected_view_].height));
                    orbit.target += camera_poses_[selected_view_].Right() * (-value.delta_x * scale)
                                    + camera_poses_[selected_view_].Up() * (value.delta_y * scale);
                } else
                    orbit.Pan(value.delta_x, value.delta_y, camera_poses_[selected_view_]);
            }
            event.handled = true;
            break;
        }
        case events::EventType::kKeyPressed:
        case events::EventType::kKeyReleased: {
            const bool down = event.GetEventType() == events::EventType::kKeyPressed;
            const auto key = down ? static_cast<const events::KeyPressedEvent&>(event).key
                                  : static_cast<const events::KeyReleasedEvent&>(event).key;
            if (key == events::KeyCode::kLeftShift) {
                left_shift_down_ = down;
                event.handled = middle_drag_;
            }
            if (key == events::KeyCode::kRightShift) {
                right_shift_down_ = down;
                event.handled = middle_drag_;
            }
            if (down && key == events::KeyCode::kF) {
                FitSelectedView();
                event.handled = true;
            }
            if (down && key == events::KeyCode::kSpace) {
                demo_->TogglePause();
                event.handled = true;
            }
            if (down && key == events::KeyCode::kR) {
                demo_->Restart();
                event.handled = true;
            }
            if (down && key == events::KeyCode::kMinus) {
                demo_->ChangeSpeed(-0.25F);
                event.handled = true;
            }
            if (down && key == events::KeyCode::kEqual) {
                demo_->ChangeSpeed(0.25F);
                event.handled = true;
            }
            break;
        }
        default:
            break;
    }
}

Result<void> RenderLayer::Initialize(Window& window) {
    Shutdown();
    window_ = &window;
    width_ = window.GetWidth();
    height_ = window.GetHeight();
    if (width_ == 0 || height_ == 0) {
        minimized_ = true;
        return Ok();
    }
    TRY_VOID(CreateDemo());
    TRY_VOID(InitializeCameras());
    TRY_VOID(UpdateCameras(0.0F));
    ready_ = true;
    slog::Info(
        "Animated climbing demo initialized; middle pan, Shift+middle orbit, wheel zoom, F fit, space pause, R "
        "restart, +/- speed"
    );
    return Ok();
}

Result<void> RenderLayer::CreateDemo() {
    std::filesystem::path root;
    TRY_ASSIGN(root, StudioAssetRoot());
    ref<const gfx::SurfaceSource> source = createRef<StudioSurfaceSource>(*window_);
    return StudioRenderDemo::Create(std::move(source), root, width_, height_, offscreen_)
        .transform([this](scope<StudioRenderDemo> value) { demo_ = std::move(value); });
}

Result<void> RenderLayer::InitializeCameras() {
    camera_viewports_ = {{{0.0F, 0.0F, 1.0F, 1.0F}, {0, 0, 1, 1}, {0, 0, 1, 1}, {0, 0, 1, 1}}};
    camera_projections_[0] = gfx::PerspectiveCamera{.vertical_fov = math::radians(55.0F),
        .near_plane = 0.1F,
        .far_plane = 1000.0F};
    for (u32 index = 1; index < 4; ++index)
        camera_projections_[index] = gfx::OrthographicCamera{.height = 5.0F, .near_plane = 0.1F, .far_plane = 1000.0F};
    orbits_[0].yaw = 0.7F;
    orbits_[0].pitch = 0.45F;
    orbits_[2].yaw = math::pi<f32> * 0.5F;
    orbits_[3].pitch = orbits_[3].max_pitch;
    for (u32 index = 0; index < 4; ++index) {
        orbits_[index].distance = 6.0F;
        TRY_VOID(orbits_[index].Update(camera_poses_[index]));
    }
    return Ok();
}

Result<void> RenderLayer::UpdateCameras(f32) {
    for (u32 index = 0; index < 4; ++index) {
        TRY_VOID(orbits_[index].Update(camera_poses_[index]));
        gfx::CameraView camera;
        TRY_ASSIGN(
            camera,
            gfx::CameraView::Build(
                camera_poses_[index],
                camera_projections_[index],
                camera_viewports_[index],
                width_,
                height_
            )
        );
        pixel_viewports_[index] = camera.pixel_rect;
        auto& view = demo_->views[index];
        view.camera = gfx::CameraState::FromCameraView(camera);
        view.viewport = camera.pixel_rect;
        TRY_VOID(demo_->runtime->UpdateView(demo_->view_ids[index], view));
    }
    return Ok();
}

void RenderLayer::FitSelectedView() noexcept {
    const auto center = demo_ ? demo_->bounds.center : math::vec3f{};
    const f32 radius = demo_ ? std::max(demo_->bounds.radius, 0.01F) : 1.0F;
    const auto viewport = camera_viewports_[selected_view_];
    const f32 aspect = height_ == 0 ? 1.0F
                                    : (static_cast<f32>(width_) * viewport.width)
                                          / (static_cast<f32>(height_) * viewport.height);
    if (const auto* perspective = std::get_if<gfx::PerspectiveCamera>(&camera_projections_[selected_view_])) {
        const f32 horizontal = 2.0F * std::atan(std::tan(perspective->vertical_fov * 0.5F) * aspect);
        static_cast<void>(orbits_[selected_view_]
                .Focus(center, radius, std::min(perspective->vertical_fov, horizontal)));
    } else {
        orbits_[selected_view_].target = center;
        std::get<gfx::OrthographicCamera>(camera_projections_[selected_view_])
            .height = radius * 2.5F / std::min(1.0F, aspect);
    }
}

Result<void> RenderLayer::Resize(const u32 width, const u32 height) {
    if (width == 0 || height == 0) {
        minimized_ = true;
        if (demo_)
            TRY_VOID(demo_->runtime->ReconfigureSurface(demo_->surface, 0, 0));
        return Ok();
    }
    if (demo_)
        TRY_VOID(demo_->runtime->ReconfigureSurface(demo_->surface, width, height));
    minimized_ = false;
    width_ = width;
    height_ = height;
    return UpdateCameras(0.0F);
}

Result<void> RenderLayer::RenderFrame(const f32 delta_seconds) {
    if (demo_->Advance(delta_seconds))
        FitSelectedView();
    TRY_VOID(UpdateCameras(delta_seconds));
    auto rendered = demo_->runtime->RenderFrame(
        {
            .scenes = {},
            .views = {},
            .time = 0.0F,
            .delta_time = delta_seconds,
            .canvas = canvas_,
        }
    );
    if (!rendered)
        return Err(rendered.error());
    if (demo_->animation_initialized
        && (demo_->last_visible_objects != rendered->stats.visible_objects
            || demo_->last_draw_packets != rendered->stats.draw_packets
            || demo_->last_draw_calls != rendered->stats.draw_calls
            || demo_->last_skipped_resources != rendered->stats.skipped_resources
            || demo_->last_fallback_textures != rendered->stats.fallback_textures)) {
        const auto& camera = demo_->views[0].camera;
        const auto& view = rendered->views[0];
        slog::Info(
            "render diagnostic: submitted={} passes={} submissions={} visible={} packets={} draws={} skipped={} "
            "fallback_textures={} "
            "camera=({:.3f}, {:.3f}, {:.3f})",
            view.submitted,
            view.graph.pass_count,
            rendered->submissions.size(),
            rendered->stats.visible_objects,
            rendered->stats.draw_packets,
            rendered->stats.draw_calls,
            rendered->stats.skipped_resources,
            rendered->stats.fallback_textures,
            camera.position.x,
            camera.position.y,
            camera.position.z
        );
        demo_->last_visible_objects = rendered->stats.visible_objects;
        demo_->last_draw_packets = rendered->stats.draw_packets;
        demo_->last_draw_calls = rendered->stats.draw_calls;
        demo_->last_skipped_resources = rendered->stats.skipped_resources;
        demo_->last_fallback_textures = rendered->stats.fallback_textures;
    }
    for (const auto& view : rendered->views)
        for (const auto& message : view.messages)
            slog::Warn("Render view {} diagnostic: {}", view.view.Index(), message);
    return Ok();
}

gfx::SurfaceHandle RenderLayer::Surface() const noexcept {
    return demo_ ? demo_->surface : gfx::SurfaceHandle{};
}

gfx::OffscreenTargetHandle RenderLayer::SceneTarget() const noexcept {
    return demo_ ? demo_->scene_target : gfx::OffscreenTargetHandle{};
}

gfx::MeshState RenderLayer::AssetStatus() const noexcept {
    return demo_ ? demo_->runtime->MeshStatus(demo_->mesh) : gfx::MeshState::Unloaded;
}

Result<void> RenderLayer::ResizeScene(u32 width, u32 height) {
    if (!demo_ || width == 0 || height == 0)
        return Ok();
    width_ = width;
    height_ = height;
    if (demo_->scene_target.IsValid())
        TRY_VOID(demo_->runtime->ResizeOffscreenTarget(demo_->scene_target, width, height));
    return UpdateCameras(0.0f);
}

Result<void> RenderLayer::SetSceneVisible(bool visible) {
    if (scene_visible_ == visible)
        return Ok();
    scene_visible_ = visible;
    if (!demo_)
        return Ok();
    for (u32 index = 0; index < demo_->views.size(); ++index) {
        demo_->views[index].active = visible && index == 0;
        TRY_VOID(demo_->runtime->UpdateView(demo_->view_ids[index], demo_->views[index]));
    }
    if (!visible)
        ClearInput();
    return Ok();
}

void RenderLayer::ClearInput() noexcept {
    middle_drag_ = false;
    left_shift_down_ = false;
    right_shift_down_ = false;
}

void RenderLayer::Shutdown() noexcept {
    ready_ = false;
    ClearInput();
    demo_.reset();
    window_ = nullptr;
    width_ = 0;
    height_ = 0;
    minimized_ = false;
    canvas_.reset();
}

} // namespace woki
