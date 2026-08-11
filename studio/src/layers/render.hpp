#pragma once

#include <woki/enums.hpp>
#include <woki/events/events.hpp>
#include <woki/window/window.hpp>
#include <woki/rhi/render_graph/resources.hpp>

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
    [[nodiscard]] Result<void> BuildRenderGraph();
    [[nodiscard]] Result<void> UpdateUniforms();
    [[nodiscard]] Result<void> Resize(u32 width, u32 height);
    [[nodiscard]] Result<void> RenderFrame();
    void Shutdown() noexcept;

    scope<rhi::Instance> instance_;
    scope<rhi::Adapter> adapter_;
    ref<rhi::Device> device_;
    ref<rhi::Surface> surface_;
    scope<rhi::Swapchain> swapchain_;
    ref<rhi::ShaderModule> shader_;
    ref<rhi::BindGroupLayout> bind_group_layout_;
    ref<rhi::BindGroup> bind_group_;
    ref<rhi::Buffer> vertex_buffer_;
    ref<rhi::Buffer> index_buffer_;
    ref<rhi::Buffer> uniform_buffer_;
    ref<rhi::PipelineLayout> pipeline_layout_;
    ref<rhi::RenderPipeline> pipeline_;
    ref<rhi::RenderGraph> render_graph_;
    rhi::PerFrameSlot backbuffer_{};
    rhi::Resource depth_{};
    Window* window_{nullptr};
    u32 width_{};
    u32 height_{};
    rhi::TextureFormat color_format_{rhi::TextureFormat::BGRA8Unorm};
    f32 cube_yaw_{0.65f};
    f32 cube_pitch_{0.45f};
    bool rotating_with_mouse_{};
    bool ready_{};
};

} // namespace woki
