#include <array>
#include <cmath>
#include <cstddef>

#include <woki/rhi.hpp>
#include <woki/math/math.hpp>

#include "render.hpp"

namespace woki {

constexpr f32 kMouseRotationSpeed = 0.008f;
constexpr rhi::Color kBackgroundColor{20.0f / 255.0f, 20.0f / 255.0f, 20.0f / 255.0f, 1.0f};

struct CubeVertex final {
    math::vec3f position;
    math::vec3f color;
};

struct alignas(16) CubeUniforms final {
    math::mat4f mvp;
};

struct CubePassData final {
    ref<rhi::RenderPipeline> pipeline;
    ref<rhi::BindGroup> bind_group;
    ref<rhi::Buffer> vertex_buffer;
    ref<rhi::Buffer> index_buffer;
    u32 index_count{};
};

constexpr std::array<CubeVertex, 24> kCubeVertices = {
    CubeVertex{{-1.0f, -1.0f, 1.0f}, {0.93f, 0.25f, 0.21f}},
    CubeVertex{{1.0f, -1.0f, 1.0f}, {0.93f, 0.25f, 0.21f}},
    CubeVertex{{1.0f, 1.0f, 1.0f}, {0.93f, 0.25f, 0.21f}},
    CubeVertex{{-1.0f, 1.0f, 1.0f}, {0.93f, 0.25f, 0.21f}},
    CubeVertex{{1.0f, -1.0f, -1.0f}, {0.16f, 0.55f, 0.96f}},
    CubeVertex{{-1.0f, -1.0f, -1.0f}, {0.16f, 0.55f, 0.96f}},
    CubeVertex{{-1.0f, 1.0f, -1.0f}, {0.16f, 0.55f, 0.96f}},
    CubeVertex{{1.0f, 1.0f, -1.0f}, {0.16f, 0.55f, 0.96f}},
    CubeVertex{{-1.0f, -1.0f, -1.0f}, {0.26f, 0.78f, 0.39f}},
    CubeVertex{{-1.0f, -1.0f, 1.0f}, {0.26f, 0.78f, 0.39f}},
    CubeVertex{{-1.0f, 1.0f, 1.0f}, {0.26f, 0.78f, 0.39f}},
    CubeVertex{{-1.0f, 1.0f, -1.0f}, {0.26f, 0.78f, 0.39f}},
    CubeVertex{{1.0f, -1.0f, 1.0f}, {0.96f, 0.70f, 0.18f}},
    CubeVertex{{1.0f, -1.0f, -1.0f}, {0.96f, 0.70f, 0.18f}},
    CubeVertex{{1.0f, 1.0f, -1.0f}, {0.96f, 0.70f, 0.18f}},
    CubeVertex{{1.0f, 1.0f, 1.0f}, {0.96f, 0.70f, 0.18f}},
    CubeVertex{{-1.0f, 1.0f, 1.0f}, {0.65f, 0.38f, 0.92f}},
    CubeVertex{{1.0f, 1.0f, 1.0f}, {0.65f, 0.38f, 0.92f}},
    CubeVertex{{1.0f, 1.0f, -1.0f}, {0.65f, 0.38f, 0.92f}},
    CubeVertex{{-1.0f, 1.0f, -1.0f}, {0.65f, 0.38f, 0.92f}},
    CubeVertex{{-1.0f, -1.0f, -1.0f}, {0.07f, 0.72f, 0.73f}},
    CubeVertex{{1.0f, -1.0f, -1.0f}, {0.07f, 0.72f, 0.73f}},
    CubeVertex{{1.0f, -1.0f, 1.0f}, {0.07f, 0.72f, 0.73f}},
    CubeVertex{{-1.0f, -1.0f, 1.0f}, {0.07f, 0.72f, 0.73f}},
};

constexpr std::array<u16, 36> kCubeIndices = {
    0,
    1,
    2,
    0,
    2,
    3,
    4,
    5,
    6,
    4,
    6,
    7,
    8,
    9,
    10,
    8,
    10,
    11,
    12,
    13,
    14,
    12,
    14,
    15,
    16,
    17,
    18,
    16,
    18,
    19,
    20,
    21,
    22,
    20,
    22,
    23,
};

constexpr const char* kCubeWgsl = R"(
struct Uniforms {
    mvp: mat4x4<f32>,
};

@group(0) @binding(0)
var<uniform> uniforms: Uniforms;

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f,
};

@vertex
fn vs_main(@location(0) position: vec3f, @location(1) color: vec3f) -> VertexOutput {
    var output: VertexOutput;
    output.position = uniforms.mvp * vec4f(position, 1.0);
    output.color = color;
    return output;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4f {
    return vec4f(input.color, 1.0);
}
)";

[[nodiscard]] static math::mat4f PerspectiveWebGpu(f32 fovy, f32 aspect, f32 z_near, f32 z_far) {
    const f32 tan_half = std::tan(fovy * 0.5f);
    const f32 depth = z_near - z_far;
    return math::mat4f(math::layout::rowm, 1.0f / (aspect * tan_half), 0.0f, 0.0f, 0.0f, 0.0f, 1.0f / tan_half, 0.0f, 0.0f, 0.0f, 0.0f, z_far / depth, (z_near * z_far) / depth, 0.0f, 0.0f, -1.0f, 0.0f);
}

RenderLayer::RenderLayer() = default;

RenderLayer::~RenderLayer() {
    Shutdown();
}

void RenderLayer::OnAttach(Context& ctx) {
    WOKI_ASSERT(ctx.window != nullptr);
    auto initialized = Initialize(*ctx.window);
    if (!initialized) {
        slog::Error("Render layer initialization failed: {}", initialized.error().Message());
        Shutdown();
    }
}

void RenderLayer::OnDetach(Context&) {
    Shutdown();
}

void RenderLayer::OnUpdate(Context&, f64) {
    if (!ready_)
        return;
    if (auto rendered = RenderFrame(); !rendered)
        slog::Warn("Render frame failed: {}", rendered.error().Message());
}

void RenderLayer::OnEvent(Context&, events::Event& event) {
    if (!ready_)
        return;

    switch (event.GetEventType()) {
        case events::EventType::kWindowResized: {
            const auto& resized = static_cast<const events::WindowResizeEvent&>(event);
            if (auto result = Resize(resized.width, resized.height); !result)
                slog::Warn("Render resize failed: {}", result.error().Message());
            break;
        }
        case events::EventType::kMouseButtonPressed: {
            const auto& pressed = static_cast<const events::MouseButtonPressedEvent&>(event);
            if (pressed.button == events::MouseButton::kLeft) {
                rotating_with_mouse_ = true;
                event.handled = true;
            }
            break;
        }
        case events::EventType::kMouseButtonReleased: {
            const auto& released = static_cast<const events::MouseButtonReleasedEvent&>(event);
            if (released.button == events::MouseButton::kLeft) {
                rotating_with_mouse_ = false;
                event.handled = true;
            }
            break;
        }
        case events::EventType::kMouseLeft:
            rotating_with_mouse_ = false;
            break;
        case events::EventType::kMouseMoved: {
            if (!rotating_with_mouse_)
                break;
            const auto& moved = static_cast<const events::MouseMovedEvent&>(event);
            cube_yaw_ += moved.delta_x * kMouseRotationSpeed;
            cube_pitch_ += moved.delta_y * kMouseRotationSpeed;
            event.handled = true;
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

    TRY_ASSIGN(instance_, rhi::Instance::Create({}));
    TRY_ASSIGN(surface_, instance_->CreateSurface(window));

    rhi::RequestAdapterDesc adapter_desc{};
    adapter_desc.compatible_surface = surface_.get();
    TRY_ASSIGN(adapter_, instance_->RequestAdapter(adapter_desc));

    rhi::DeviceDesc device_desc{};
    device_desc.label = "StudioDevice";
    device_desc.uncaptured_error_callback = [](rhi::ErrorType type, std::string_view message) { slog::Error("RHI device error ({}): {}", static_cast<u32>(type), message); };
    TRY_ASSIGN(device_, adapter_->CreateDevice(device_desc));

    rhi::SurfaceCapabilities capabilities{};
    TRY_VOID(surface_->GetCapabilities(*adapter_, capabilities));
    if (capabilities.formats.empty())
        return Err(ErrorCode::InvalidState, "RHI surface has no supported color formats.");

    const auto preferred = std::ranges::find(capabilities.formats, rhi::TextureFormat::BGRA8Unorm);
    color_format_ = preferred != capabilities.formats.end() ? *preferred : capabilities.formats.front();
    TRY_ASSIGN(swapchain_, rhi::Swapchain::Builder(device_, surface_).Size(width_, height_).ColorFormat(color_format_).Label("StudioSwapchain").Build());
    TRY_VOID(CreateResources());
    TRY_VOID(BuildRenderGraph());
    TRY_VOID(UpdateUniforms());

    ready_ = true;
    slog::Info("Single-cube renderer initialized ({}x{})", width_, height_);
    return Ok();
}

Result<void> RenderLayer::CreateResources() {
    rhi::BufferDesc vertex_desc{};
    vertex_desc.label = "CubeVertices";
    vertex_desc.size = sizeof(kCubeVertices);
    vertex_desc.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst;
    TRY_ASSIGN(vertex_buffer_, device_->CreateBuffer(vertex_desc));
    TRY_VOID(device_->GetQueue().WriteBuffer(*vertex_buffer_, 0, kCubeVertices.data(), sizeof(kCubeVertices)));

    rhi::BufferDesc index_desc{};
    index_desc.label = "CubeIndices";
    index_desc.size = sizeof(kCubeIndices);
    index_desc.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst;
    TRY_ASSIGN(index_buffer_, device_->CreateBuffer(index_desc));
    TRY_VOID(device_->GetQueue().WriteBuffer(*index_buffer_, 0, kCubeIndices.data(), sizeof(kCubeIndices)));

    rhi::BufferDesc uniform_desc{};
    uniform_desc.label = "CubeUniforms";
    uniform_desc.size = sizeof(CubeUniforms);
    uniform_desc.usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst;
    TRY_ASSIGN(uniform_buffer_, device_->CreateBuffer(uniform_desc));

    TRY_ASSIGN(shader_, device_->CreateShaderModule({.code = kCubeWgsl, .label = "CubeShader"}));

    const rhi::BindGroupLayoutEntryDesc uniform_layout{
        .binding = 0,
        .visibility = static_cast<u32>(rhi::ShaderStage::Vertex),
        .buffer = {.type = rhi::BufferBindingType::Uniform, .min_binding_size = sizeof(CubeUniforms)},
    };
    TRY_ASSIGN(bind_group_layout_, device_->CreateBindGroupLayout({.entries = std::span<const rhi::BindGroupLayoutEntryDesc>(&uniform_layout, 1), .label = "CubeBindGroupLayout"}));

    const rhi::BindGroupEntryDesc uniform_binding{.binding = 0, .buffer = uniform_buffer_.get(), .offset = 0, .size = sizeof(CubeUniforms)};
    TRY_ASSIGN(bind_group_, device_->CreateBindGroup({.layout = bind_group_layout_.get(), .entries = std::span<const rhi::BindGroupEntryDesc>(&uniform_binding, 1), .label = "CubeBindGroup"}));

    rhi::BindGroupLayout* layouts[]{bind_group_layout_.get()};
    TRY_ASSIGN(pipeline_layout_, device_->CreatePipelineLayout({.bind_group_layouts = std::span<rhi::BindGroupLayout* const>(layouts, 1), .label = "CubePipelineLayout"}));

    const std::array vertex_attributes{
        rhi::VertexAttributeDesc{.format = rhi::VertexFormat::Float32x3, .offset = offsetof(CubeVertex, position), .shader_location = 0},
        rhi::VertexAttributeDesc{.format = rhi::VertexFormat::Float32x3, .offset = offsetof(CubeVertex, color), .shader_location = 1},
    };
    const rhi::VertexBufferLayoutDesc vertex_layout{.array_stride = sizeof(CubeVertex), .attributes = vertex_attributes};
    const rhi::VertexStateDesc vertex_state{.module = shader_.get(), .entry_point = "vs_main", .buffers = std::span<const rhi::VertexBufferLayoutDesc>(&vertex_layout, 1)};
    const rhi::ColorTargetStateDesc color_target{.format = color_format_};
    const rhi::FragmentStateDesc fragment_state{.module = shader_.get(), .entry_point = "fs_main", .targets = std::span<const rhi::ColorTargetStateDesc>(&color_target, 1)};
    const rhi::PrimitiveStateDesc primitive_state{.topology = rhi::PrimitiveTopology::TriangleList, .front_face = rhi::FrontFace::CCW, .cull_mode = rhi::CullMode::None};
    const rhi::DepthStencilStateDesc depth_state{.format = rhi::TextureFormat::Depth24PlusStencil8, .depth_write_enabled = true, .depth_compare = rhi::CompareFunction::Less};

    rhi::RenderPipelineDescTyped pipeline_desc{};
    pipeline_desc.label = "CubePipeline";
    pipeline_desc.layout = pipeline_layout_.get();
    pipeline_desc.vertex = &vertex_state;
    pipeline_desc.primitive = &primitive_state;
    pipeline_desc.depth_stencil = &depth_state;
    pipeline_desc.fragment = &fragment_state;
    TRY_ASSIGN(pipeline_, device_->CreateRenderPipeline(pipeline_desc));
    return Ok();
}

Result<void> RenderLayer::BuildRenderGraph() {
    rhi::RenderGraphBuilder builder(device_);
    backbuffer_ = builder.PerFrame();
    depth_ = builder.Transient({
        .label = "CubeDepth",
        .format = rhi::TextureFormat::Depth24PlusStencil8,
        .usage = rhi::TextureUsage::RenderAttachment,
        .extent = rhi::ExtentMode::Swapchain(),
    });

    builder.AddPass("Cube").Color(0, backbuffer_, {.load = rhi::LoadOp::Clear, .clear = kBackgroundColor}).Depth(depth_, {.load = rhi::LoadOp::Clear, .clear = 1.0f}).Execute([](rhi::RenderPassContext& ctx) {
        const auto& data = ctx.data<CubePassData>();
        auto& pass = ctx.encoder();
        pass.SetPipeline(*data.pipeline);
        pass.SetBindGroup(0, data.bind_group.get());
        pass.SetVertexBuffer(0, data.vertex_buffer.get());
        pass.SetIndexBuffer(*data.index_buffer, rhi::IndexFormat::Uint16);
        pass.DrawIndexed(data.index_count, 1);
    });
    builder.SetPassData("Cube", CubePassData{pipeline_, bind_group_, vertex_buffer_, index_buffer_, static_cast<u32>(kCubeIndices.size())});
    TRY_ASSIGN(render_graph_, builder.Compile(width_, height_));
    return Ok();
}

Result<void> RenderLayer::UpdateUniforms() {
    const f32 aspect = height_ == 0 ? 1.0f : static_cast<f32>(width_) / static_cast<f32>(height_);
    const math::mat4f projection = PerspectiveWebGpu(math::radians(55.0f), aspect, 0.1f, 100.0f);
    const math::mat4f view = math::lookAt(math::vec3f{0.0f, 0.0f, 6.0f}, math::vec3f{}, math::vec3f{0.0f, 1.0f, 0.0f});
    const math::mat4f model = math::rotate_y(cube_yaw_) * math::rotate_x(cube_pitch_);
    const CubeUniforms uniforms{.mvp = projection * view * model};
    TRY_VOID(device_->GetQueue().WriteBuffer(*uniform_buffer_, 0, &uniforms, sizeof(uniforms)));
    return Ok();
}

Result<void> RenderLayer::Resize(u32 width, u32 height) {
    if (!ready_ || width == 0 || height == 0 || (width == width_ && height == height_))
        return Ok();
    width_ = width;
    height_ = height;
    swapchain_->Resize(width_, height_);
    TRY_VOID(render_graph_->RebuildForResize(width_, height_));
    return UpdateUniforms();
}

Result<void> RenderLayer::RenderFrame() {
    TRY_VOID(UpdateUniforms());
    auto graph_frame = render_graph_->BeginFrame(width_, height_);
    if (!graph_frame)
        return Err(std::move(graph_frame).error());

    auto frame = swapchain_->AcquireNextFrame();
    if (!frame) {
        instance_->ProcessEvents();
        return Err(std::move(frame).error());
    }
    graph_frame->Bind(backbuffer_, frame->ColorViewRef());
    if (auto executed = graph_frame->Execute(); !executed) {
        swapchain_->Discard();
        return Err(std::move(executed).error());
    }
    TRY_VOID(swapchain_->Present());
    device_->Tick();
    instance_->ProcessEvents();
    return Ok();
}

void RenderLayer::Shutdown() noexcept {
    ready_ = false;
    render_graph_.reset();
    pipeline_.reset();
    pipeline_layout_.reset();
    bind_group_.reset();
    bind_group_layout_.reset();
    uniform_buffer_.reset();
    index_buffer_.reset();
    vertex_buffer_.reset();
    shader_.reset();
    swapchain_.reset();
    surface_.reset();
    device_.reset();
    adapter_.reset();
    instance_.reset();
    window_ = nullptr;
    width_ = 0;
    height_ = 0;
    cube_yaw_ = 0.65f;
    cube_pitch_ = 0.45f;
    rotating_with_mouse_ = false;
}

} // namespace woki
