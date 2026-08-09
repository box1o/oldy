#pragma once

#include <woki/enums.hpp>
#include <woki/events/events.hpp>
#include <woki/window/window.hpp>
#include <woki/rhi/render_graph/resources.hpp>

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
class Sampler;
class ShaderModule;
class Surface;
class Swapchain;
class Texture;
class TextureView;
} // namespace woki::rhi

namespace woki {

class RhiRenderer {
public:
    RhiRenderer();
    ~RhiRenderer();

    RhiRenderer(const RhiRenderer&) = delete;
    RhiRenderer& operator=(const RhiRenderer&) = delete;

    [[nodiscard]] bool Initialize(Window& window);
    void Shutdown() noexcept;
    void Resize(u32 width, u32 height);
    void HandleEvent(events::Event& event);
    [[nodiscard]] bool RenderFrame(f64 delta_ms);

    [[nodiscard]] bool IsReady() const noexcept {
        return ready_;
    }

private:
    [[nodiscard]] bool CreateDeferredPipelines();
    [[nodiscard]] bool BuildRenderGraph();
    [[nodiscard]] bool UploadCubeResources();
    [[nodiscard]] bool UpdateUniforms(f64 delta_ms);

    scope<rhi::Instance> instance_;
    scope<rhi::Adapter> adapter_;
    ref<rhi::Device> device_;
    ref<rhi::Surface> surface_;
    scope<rhi::Swapchain> swapchain_;
    ref<rhi::ShaderModule> gbuffer_shader_;
    ref<rhi::ShaderModule> lighting_shader_;
    ref<rhi::ShaderModule> present_shader_;
    ref<rhi::ShaderModule> texture_debug_shader_;
    ref<rhi::BindGroupLayout> gbuffer_bind_group_layout_;
    ref<rhi::BindGroupLayout> lighting_bind_group_layout_;
    ref<rhi::BindGroupLayout> present_bind_group_layout_;
    ref<rhi::BindGroup> gbuffer_bind_group_;
    ref<rhi::Buffer> vertex_buffer_;
    ref<rhi::Buffer> index_buffer_;
    ref<rhi::Buffer> uniform_buffer_;
    ref<rhi::PipelineLayout> gbuffer_pipeline_layout_;
    ref<rhi::PipelineLayout> lighting_pipeline_layout_;
    ref<rhi::PipelineLayout> present_pipeline_layout_;
    ref<rhi::RenderPipeline> gbuffer_pipeline_;
    ref<rhi::RenderPipeline> lighting_pipeline_;
    ref<rhi::RenderPipeline> present_pipeline_;
    ref<rhi::RenderPipeline> texture_debug_pipeline_;
    ref<rhi::Sampler> linear_sampler_;
    scope<rhi::Texture> history_texture_;
    ref<rhi::RenderGraph> render_graph_;
    rhi::PerFrameSlot backbuffer_{};
    rhi::Resource gbuffer_albedo_{};
    rhi::Resource gbuffer_normal_{};
    rhi::Resource gbuffer_material_{};
    rhi::Resource gbuffer_depth_{};
    rhi::Resource hdr_color_{};
    rhi::Resource history_{};
    Window* window_{nullptr};
    u32 width_{0};
    u32 height_{0};
    rhi::TextureFormat color_format_{rhi::TextureFormat::BGRA8Unorm};
    f32 cube_yaw_{0.65f};
    f32 cube_pitch_{0.45f};
    bool rotating_with_mouse_{false};
    ref<bool> show_texture_debug_{createRef<bool>(false)};
    bool ready_{false};
};

} // namespace woki
