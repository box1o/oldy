#pragma once

#include <array>

#include <woki/enums.hpp>
#include <woki/gfx/camera.hpp>
#include <woki/events/events.hpp>
#include <woki/window/window.hpp>
#include <woki/gfx/runtime.hpp>
#include <woki/gfx/canvas.hpp>

#include "core/layer.hpp"

namespace woki {

struct StudioRenderDemo;

class RenderLayer final : public Layer {
public:
    explicit RenderLayer(bool offscreen = false);
    ~RenderLayer() override;

    void OnAttach(Context& ctx) override;
    void OnDetach(Context& ctx) override;
    void OnUpdate(Context& ctx, f64 delta_ms) override;
    void OnEvent(Context& ctx, events::Event& event) override;

    void SetCanvas(gfx::CanvasFrame canvas) {
        canvas_ = std::move(canvas);
    }

    [[nodiscard]] gfx::SurfaceHandle Surface() const noexcept;
    [[nodiscard]] gfx::OffscreenTargetHandle SceneTarget() const noexcept;
    [[nodiscard]] gfx::MeshState AssetStatus() const noexcept;
    [[nodiscard]] Result<void> ResizeScene(u32 width, u32 height);
    [[nodiscard]] Result<void> SetSceneVisible(bool visible);

    void SetSceneOrigin(f32 x, f32 y) noexcept {
        scene_origin_x_ = x;
        scene_origin_y_ = y;
    }

private:
    [[nodiscard]] Result<void> Initialize(Window& window);
    [[nodiscard]] Result<void> CreateDemo();
    [[nodiscard]] Result<void> InitializeCameras();
    [[nodiscard]] Result<void> UpdateCameras(f32 delta_seconds);
    [[nodiscard]] Result<void> Resize(u32 width, u32 height);
    [[nodiscard]] Result<void> RenderFrame(f32 delta_seconds);
    void SelectViewport(f32 logical_x, f32 logical_y) noexcept;
    void FitSelectedView() noexcept;
    void ClearInput() noexcept;
    void Shutdown() noexcept;

    scope<StudioRenderDemo> demo_;
    Window* window_{nullptr};
    std::array<gfx::CameraPose, 4> camera_poses_{};
    std::array<gfx::CameraProjection, 4> camera_projections_{};
    std::array<gfx::CameraViewport, 4> camera_viewports_{};
    std::array<gfx::OrbitController, 4> orbits_{};
    std::array<gfx::PixelRect, 4> pixel_viewports_{};
    u32 width_{};
    u32 height_{};
    u32 selected_view_{};
    bool middle_drag_{};
    bool left_shift_down_{};
    bool right_shift_down_{};
    bool minimized_{};
    bool ready_{};
    bool scene_visible_{true};
    bool offscreen_{};
    std::optional<gfx::CanvasFrame> canvas_;
    f32 scene_origin_x_{}, scene_origin_y_{};
};

} // namespace woki
