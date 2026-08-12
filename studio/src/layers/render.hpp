#pragma once

#include <array>

#include <woki/enums.hpp>
#include <woki/events/events.hpp>
#include <woki/gfx/camera.hpp>
#include <woki/rhi/render_graph/resources.hpp>
#include <woki/window/window.hpp>

#include "core/layer.hpp"

namespace woki::rhi {
class Adapter;
class BindGroup;
class BindGroupLayout;
class Buffer;
class Device;
class Instance;
class PipelineLayout;
class RenderGraph;
class RenderPipeline;
class ShaderModule;
class Surface;
class Swapchain;
} // namespace woki::rhi

namespace woki {

struct CubePassState;

class RenderLayer final : public Layer {
public:
    RenderLayer();
    ~RenderLayer() override;

    void OnAttach(Context& ctx) override;
    void OnDetach(Context& ctx) override;
    void OnUpdate(Context& ctx, f64 delta_ms) override;
    void OnEvent(Context& ctx, events::Event& event) override;

private:
    [[nodiscard]] Result<void> Initialize(Window& window);
    [[nodiscard]] Result<void> CreateResources();
    [[nodiscard]] Result<void> InitializeCameras();
    [[nodiscard]] Result<void> BuildRenderGraph();
    [[nodiscard]] Result<void> UpdateCameras(f32 delta_seconds);
    [[nodiscard]] Result<void> Resize(u32 width, u32 height);
    [[nodiscard]] Result<void> RenderFrame(f32 delta_seconds);
    void SelectViewport(f32 logical_x, f32 logical_y) noexcept;
    void ClearInput() noexcept;
    void Shutdown() noexcept;

    scope<rhi::Instance> instance_;
    scope<rhi::Adapter> adapter_;
    ref<rhi::Device> device_;
    ref<rhi::Surface> surface_;
    scope<rhi::Swapchain> swapchain_;
    ref<rhi::ShaderModule> shader_;
    std::array<ref<rhi::BindGroupLayout>, 4> bind_group_layouts_;
    ref<rhi::BindGroup> view_bind_group_;
    ref<rhi::BindGroup> object_bind_group_;
    ref<rhi::Buffer> vertex_buffer_;
    ref<rhi::Buffer> index_buffer_;
    ref<rhi::Buffer> view_buffer_;
    ref<rhi::Buffer> object_buffer_;
    ref<rhi::Buffer> joints_buffer_;
    ref<rhi::PipelineLayout> pipeline_layout_;
    ref<rhi::RenderPipeline> pipeline_;
    ref<rhi::RenderGraph> render_graph_;
    ref<CubePassState> pass_state_;
    rhi::PerFrameSlot backbuffer_{};
    rhi::Resource depth_{};
    Window* window_{nullptr};
    std::array<gfx::CameraPose, 4> camera_poses_{};
    std::array<gfx::CameraProjection, 4> camera_projections_{};
    std::array<gfx::CameraViewport, 4> camera_viewports_{};
    gfx::OrbitController orbit_{};
    gfx::FlyController fly_{};
    gfx::FlyInput fly_input_{};
    u32 view_uniform_stride_{};
    u32 width_{};
    u32 height_{};
    u32 selected_view_{};
    rhi::TextureFormat color_format_{rhi::TextureFormat::BGRA8Unorm};
    bool left_drag_{};
    bool middle_drag_{};
    bool right_drag_{};
    bool forward_key_{};
    bool backward_key_{};
    bool left_key_{};
    bool right_key_{};
    bool up_key_{};
    bool down_key_{};
    bool left_boost_key_{};
    bool right_boost_key_{};
    bool minimized_{};
    bool ready_{};
};

} // namespace woki
