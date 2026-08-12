#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <vector>

#include <woki/asset.hpp>
#include <woki/gfx.hpp>
#include <woki/rhi.hpp>

#include "render.hpp"

namespace woki {
namespace {

constexpr rhi::Color kBackgroundColor{14.0f / 255.0f, 17.0f / 255.0f, 23.0f / 255.0f, 1.0f};

struct CubeVertex {
    math::vec3f position;
    math::vec3f normal;
    math::vec4f tangent;
    math::vec2f uv;
    math::vec4f color;
};

struct alignas(16) ViewUniforms {
    math::mat4f view_projection;
    math::mat4f inverse_view_projection;
    math::vec3f camera_position;
    f32 near_plane;
    math::vec2f viewport_size;
    f32 far_plane;
    f32 padding{};
};

struct alignas(16) ObjectUniforms {
    math::mat4f model;
    math::mat4f normal_matrix;
};

constexpr std::array<math::vec4f, 6> kFaceColors = {
    math::vec4f{0.94f, 0.29f, 0.23f, 1.0f},
    math::vec4f{0.18f, 0.54f, 0.96f, 1.0f},
    math::vec4f{0.25f, 0.80f, 0.43f, 1.0f},
    math::vec4f{0.98f, 0.72f, 0.20f, 1.0f},
    math::vec4f{0.68f, 0.42f, 0.94f, 1.0f},
    math::vec4f{0.08f, 0.74f, 0.76f, 1.0f},
};

constexpr std::array<CubeVertex, 24> kCubeVertices = {
    CubeVertex{{-1, -1, 1}, {0, 0, 1}, {1, 0, 0, 1}, {0, 0}, kFaceColors[0]},
    CubeVertex{{1, -1, 1}, {0, 0, 1}, {1, 0, 0, 1}, {1, 0}, kFaceColors[0]},
    CubeVertex{{1, 1, 1}, {0, 0, 1}, {1, 0, 0, 1}, {1, 1}, kFaceColors[0]},
    CubeVertex{{-1, 1, 1}, {0, 0, 1}, {1, 0, 0, 1}, {0, 1}, kFaceColors[0]},
    CubeVertex{{1, -1, -1}, {0, 0, -1}, {-1, 0, 0, 1}, {0, 0}, kFaceColors[1]},
    CubeVertex{{-1, -1, -1}, {0, 0, -1}, {-1, 0, 0, 1}, {1, 0}, kFaceColors[1]},
    CubeVertex{{-1, 1, -1}, {0, 0, -1}, {-1, 0, 0, 1}, {1, 1}, kFaceColors[1]},
    CubeVertex{{1, 1, -1}, {0, 0, -1}, {-1, 0, 0, 1}, {0, 1}, kFaceColors[1]},
    CubeVertex{{-1, -1, -1}, {-1, 0, 0}, {0, 0, 1, 1}, {0, 0}, kFaceColors[2]},
    CubeVertex{{-1, -1, 1}, {-1, 0, 0}, {0, 0, 1, 1}, {1, 0}, kFaceColors[2]},
    CubeVertex{{-1, 1, 1}, {-1, 0, 0}, {0, 0, 1, 1}, {1, 1}, kFaceColors[2]},
    CubeVertex{{-1, 1, -1}, {-1, 0, 0}, {0, 0, 1, 1}, {0, 1}, kFaceColors[2]},
    CubeVertex{{1, -1, 1}, {1, 0, 0}, {0, 0, -1, 1}, {0, 0}, kFaceColors[3]},
    CubeVertex{{1, -1, -1}, {1, 0, 0}, {0, 0, -1, 1}, {1, 0}, kFaceColors[3]},
    CubeVertex{{1, 1, -1}, {1, 0, 0}, {0, 0, -1, 1}, {1, 1}, kFaceColors[3]},
    CubeVertex{{1, 1, 1}, {1, 0, 0}, {0, 0, -1, 1}, {0, 1}, kFaceColors[3]},
    CubeVertex{{-1, 1, 1}, {0, 1, 0}, {1, 0, 0, 1}, {0, 0}, kFaceColors[4]},
    CubeVertex{{1, 1, 1}, {0, 1, 0}, {1, 0, 0, 1}, {1, 0}, kFaceColors[4]},
    CubeVertex{{1, 1, -1}, {0, 1, 0}, {1, 0, 0, 1}, {1, 1}, kFaceColors[4]},
    CubeVertex{{-1, 1, -1}, {0, 1, 0}, {1, 0, 0, 1}, {0, 1}, kFaceColors[4]},
    CubeVertex{{-1, -1, -1}, {0, -1, 0}, {1, 0, 0, 1}, {0, 0}, kFaceColors[5]},
    CubeVertex{{1, -1, -1}, {0, -1, 0}, {1, 0, 0, 1}, {1, 0}, kFaceColors[5]},
    CubeVertex{{1, -1, 1}, {0, -1, 0}, {1, 0, 0, 1}, {1, 1}, kFaceColors[5]},
    CubeVertex{{-1, -1, 1}, {0, -1, 0}, {1, 0, 0, 1}, {0, 1}, kFaceColors[5]},
};

constexpr std::array<u16, 36> kCubeIndices = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23};

u32 AlignUp(u32 value, u32 alignment) {
    return alignment == 0 ? value : (value + alignment - 1) / alignment * alignment;
}

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

} // namespace

struct CubePassState {
    ref<rhi::RenderPipeline> pipeline;
    ref<rhi::BindGroup> view_bind_group;
    ref<rhi::BindGroup> object_bind_group;
    ref<rhi::Buffer> vertex_buffer;
    ref<rhi::Buffer> index_buffer;
    std::array<gfx::PixelRect, 4> viewports{};
    u32 uniform_stride{};
};

RenderLayer::RenderLayer() = default;

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

void RenderLayer::OnUpdate(Context&, f64 delta_ms) {
    if (!ready_ || minimized_)
        return;
    if (auto rendered = RenderFrame(static_cast<f32>(std::clamp(delta_ms * 0.001, 0.0, 0.1))); !rendered)
        slog::Warn("Render frame failed: {}", rendered.error().Message());
}

void RenderLayer::SelectViewport(f32 logical_x, f32 logical_y) noexcept {
    for (u32 index = 0; index < camera_viewports_.size(); ++index) {
        const auto hit = camera_viewports_[index].HitTest(logical_x, logical_y, window_->GetContentScaleX(), window_->GetContentScaleY(), width_, height_);
        if (hit && *hit) {
            selected_view_ = index;
            return;
        }
    }
}

void RenderLayer::OnEvent(Context&, events::Event& event) {
    if (!ready_) {
        if (event.GetEventType() == events::EventType::kWindowResized && window_ != nullptr) {
            const auto& value = static_cast<const events::WindowResizeEvent&>(event);
            if (value.width != 0 && value.height != 0) {
                Window* window = window_;
                if (auto initialized = Initialize(*window); !initialized)
                    slog::Error("Render layer initialization failed: {}", initialized.error().Message());
            }
        }
        return;
    }
    switch (event.GetEventType()) {
        case events::EventType::kWindowResized: {
            const auto& value = static_cast<const events::WindowResizeEvent&>(event);
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
        case events::EventType::kMouseLeft:
            ClearInput();
            break;
        case events::EventType::kMouseButtonPressed: {
            const auto& value = static_cast<const events::MouseButtonPressedEvent&>(event);
            SelectViewport(value.x, value.y);
            left_drag_ |= value.button == events::MouseButton::kLeft;
            middle_drag_ |= value.button == events::MouseButton::kMiddle;
            right_drag_ |= value.button == events::MouseButton::kRight;
            if (right_drag_ && selected_view_ == 0) {
                fly_.yaw = orbit_.yaw;
                fly_.pitch = -orbit_.pitch;
            }
            event.handled = true;
            break;
        }
        case events::EventType::kMouseButtonReleased: {
            const auto& value = static_cast<const events::MouseButtonReleasedEvent&>(event);
            if (value.button == events::MouseButton::kLeft)
                left_drag_ = false;
            if (value.button == events::MouseButton::kMiddle)
                middle_drag_ = false;
            if (value.button == events::MouseButton::kRight) {
                right_drag_ = false;
                fly_.Stop();
            }
            event.handled = true;
            break;
        }
        case events::EventType::kMouseMoved: {
            const auto& value = static_cast<const events::MouseMovedEvent&>(event);
            if (selected_view_ == 0) {
                if (left_drag_)
                    orbit_.Rotate(value.delta_x, value.delta_y);
                if (middle_drag_)
                    orbit_.Pan(value.delta_x, value.delta_y, camera_poses_[0]);
                if (right_drag_) {
                    fly_input_.look_x += value.delta_x;
                    fly_input_.look_y += value.delta_y;
                }
            } else if (left_drag_ || middle_drag_) {
                auto& projection = std::get<gfx::OrthographicCamera>(camera_projections_[selected_view_]);
                const f32 scale = projection.height / static_cast<f32>(std::max(1u, pass_state_->viewports[selected_view_].height));
                const f32 framebuffer_delta_x = value.delta_x * window_->GetContentScaleX();
                const f32 framebuffer_delta_y = value.delta_y * window_->GetContentScaleY();
                camera_poses_[selected_view_].position += camera_poses_[selected_view_].Right() * (-framebuffer_delta_x * scale) + camera_poses_[selected_view_].Up() * (framebuffer_delta_y * scale);
            }
            event.handled = left_drag_ || middle_drag_ || right_drag_;
            break;
        }
        case events::EventType::kMouseScrolled: {
            const auto& value = static_cast<const events::MouseScrolledEvent&>(event);
            if (selected_view_ == 0)
                orbit_.Dolly(value.offset_y);
            else {
                auto& projection = std::get<gfx::OrthographicCamera>(camera_projections_[selected_view_]);
                projection.height = std::clamp(projection.height * std::exp(-value.offset_y * 0.12f), 0.05f, 10000.0f);
            }
            event.handled = true;
            break;
        }
        case events::EventType::kKeyPressed:
        case events::EventType::kKeyReleased: {
            const bool down = event.GetEventType() == events::EventType::kKeyPressed;
            const auto key = down ? static_cast<const events::KeyPressedEvent&>(event).key : static_cast<const events::KeyReleasedEvent&>(event).key;
            if (key == events::KeyCode::kW)
                forward_key_ = down;
            if (key == events::KeyCode::kS)
                backward_key_ = down;
            if (key == events::KeyCode::kD)
                right_key_ = down;
            if (key == events::KeyCode::kA)
                left_key_ = down;
            if (key == events::KeyCode::kE)
                up_key_ = down;
            if (key == events::KeyCode::kQ)
                down_key_ = down;
            if (key == events::KeyCode::kLeftShift)
                left_boost_key_ = down;
            if (key == events::KeyCode::kRightShift)
                right_boost_key_ = down;
            fly_input_.forward = static_cast<f32>(forward_key_) - static_cast<f32>(backward_key_);
            fly_input_.right = static_cast<f32>(right_key_) - static_cast<f32>(left_key_);
            fly_input_.up = static_cast<f32>(up_key_) - static_cast<f32>(down_key_);
            fly_input_.boost = left_boost_key_ || right_boost_key_;
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
    TRY_ASSIGN(instance_, rhi::Instance::Create({}));
    TRY_ASSIGN(surface_, instance_->CreateSurface(window));
    rhi::RequestAdapterDesc adapter_desc{.compatible_surface = surface_.get()};
    TRY_ASSIGN(adapter_, instance_->RequestAdapter(adapter_desc));
    rhi::DeviceDesc device_desc{};
    device_desc.label = "StudioDevice";
    device_desc.uncaptured_error_callback = [](rhi::ErrorType type, std::string_view message) { slog::Error("RHI device error ({}): {}", static_cast<u32>(type), message); };
    TRY_ASSIGN(device_, adapter_->CreateDevice(device_desc));
    rhi::SurfaceCapabilities capabilities{};
    TRY_VOID(surface_->GetCapabilities(*adapter_, capabilities));
    if (capabilities.formats.empty())
        return Err(ErrorCode::InvalidState, "RHI surface has no supported color formats");
    const auto preferred = std::ranges::find(capabilities.formats, rhi::TextureFormat::BGRA8Unorm);
    color_format_ = preferred == capabilities.formats.end() ? capabilities.formats.front() : *preferred;
    TRY_ASSIGN(swapchain_, rhi::Swapchain::Builder(device_, surface_).Size(width_, height_).ColorFormat(color_format_).Label("StudioSwapchain").Build());
    TRY_VOID(CreateResources());
    TRY_VOID(InitializeCameras());
    TRY_VOID(BuildRenderGraph());
    TRY_VOID(UpdateCameras(0.0f));
    ready_ = true;
    slog::Info("Four-view camera demo initialized ({}x{})", width_, height_);
    return Ok();
}

Result<void> RenderLayer::CreateResources() {
    rhi::BufferDesc vertex_desc{.size = sizeof(kCubeVertices), .usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst, .label = "CubeVertices"};
    TRY_ASSIGN(vertex_buffer_, device_->CreateBuffer(vertex_desc));
    TRY_VOID(device_->GetQueue().WriteBuffer(*vertex_buffer_, 0, kCubeVertices.data(), sizeof(kCubeVertices)));
    rhi::BufferDesc index_desc{.size = sizeof(kCubeIndices), .usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst, .label = "CubeIndices"};
    TRY_ASSIGN(index_buffer_, device_->CreateBuffer(index_desc));
    TRY_VOID(device_->GetQueue().WriteBuffer(*index_buffer_, 0, kCubeIndices.data(), sizeof(kCubeIndices)));

    const u32 alignment = std::max(1u, device_->GetLimits().min_uniform_buffer_offset_alignment);
    view_uniform_stride_ = AlignUp(sizeof(ViewUniforms), alignment);
    rhi::BufferDesc view_desc{.size = static_cast<u64>(view_uniform_stride_) * 4, .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst, .label = "CameraViews"};
    TRY_ASSIGN(view_buffer_, device_->CreateBuffer(view_desc));
    rhi::BufferDesc object_desc{.size = sizeof(ObjectUniforms), .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst, .label = "CubeObject"};
    TRY_ASSIGN(object_buffer_, device_->CreateBuffer(object_desc));
    rhi::BufferDesc joints_desc{.size = sizeof(math::mat4f), .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::CopyDst, .label = "IdentityJoint"};
    TRY_ASSIGN(joints_buffer_, device_->CreateBuffer(joints_desc));
    const ObjectUniforms object{math::mat4f::identity(), math::mat4f::identity()};
    const math::mat4f identity = math::mat4f::identity();
    TRY_VOID(device_->GetQueue().WriteBuffer(*object_buffer_, 0, &object, sizeof(object)));
    TRY_VOID(device_->GetQueue().WriteBuffer(*joints_buffer_, 0, &identity, sizeof(identity)));

    asset::Vfs vfs;
    ref<asset::DirectoryMount> mount;
    std::filesystem::path asset_root;
    TRY_ASSIGN(asset_root, StudioAssetRoot());
    TRY_ASSIGN(mount, asset::DirectoryMount::Create(asset_root));
    vfs.AddMount(mount);
    const auto descriptor_path = asset::AssetPath::Parse("shaders/descriptors/cube.woki-shader");
    if (!descriptor_path)
        return Err(descriptor_path.error());
    TRY_ASSIGN(shader_, gfx::CreateShaderModuleFromAsset(*device_, vfs, *descriptor_path));

    const rhi::BindGroupLayoutEntryDesc view_entry{.binding = 0,
        .visibility = static_cast<u32>(rhi::ShaderStage::Vertex),
        .buffer = {.type = rhi::BufferBindingType::Uniform, .has_dynamic_offset = true, .min_binding_size = sizeof(ViewUniforms)}};
    const std::array object_entries{
        rhi::BindGroupLayoutEntryDesc{.binding = 0, .visibility = static_cast<u32>(rhi::ShaderStage::Vertex), .buffer = {.type = rhi::BufferBindingType::Uniform, .min_binding_size = sizeof(ObjectUniforms)}},
        rhi::BindGroupLayoutEntryDesc{.binding = 1, .visibility = static_cast<u32>(rhi::ShaderStage::Vertex), .buffer = {.type = rhi::BufferBindingType::ReadOnlyStorage, .min_binding_size = sizeof(math::mat4f)}},
    };
    TRY_ASSIGN(bind_group_layouts_[0], device_->CreateBindGroupLayout({.label = "EmptyGroup0"}));
    TRY_ASSIGN(bind_group_layouts_[1], device_->CreateBindGroupLayout({.entries = std::span(&view_entry, 1), .label = "ViewGroup1"}));
    TRY_ASSIGN(bind_group_layouts_[2], device_->CreateBindGroupLayout({.label = "EmptyGroup2"}));
    TRY_ASSIGN(bind_group_layouts_[3], device_->CreateBindGroupLayout({.entries = object_entries, .label = "ObjectGroup3"}));
    const rhi::BindGroupEntryDesc view_binding{.binding = 0, .buffer = view_buffer_.get(), .size = sizeof(ViewUniforms)};
    TRY_ASSIGN(view_bind_group_, device_->CreateBindGroup({.layout = bind_group_layouts_[1].get(), .entries = std::span(&view_binding, 1), .label = "CameraViews"}));
    const std::array object_bindings{
        rhi::BindGroupEntryDesc{.binding = 0, .buffer = object_buffer_.get(), .size = sizeof(ObjectUniforms)},
        rhi::BindGroupEntryDesc{.binding = 1, .buffer = joints_buffer_.get(), .size = sizeof(math::mat4f)},
    };
    TRY_ASSIGN(object_bind_group_, device_->CreateBindGroup({.layout = bind_group_layouts_[3].get(), .entries = object_bindings, .label = "CubeObject"}));
    std::array<rhi::BindGroupLayout*, 4> layouts{};
    std::ranges::transform(bind_group_layouts_, layouts.begin(), [](const auto& layout) { return layout.get(); });
    TRY_ASSIGN(pipeline_layout_, device_->CreatePipelineLayout({.bind_group_layouts = layouts, .label = "CubePipelineLayout"}));

    const std::array attributes{
        rhi::VertexAttributeDesc{.format = rhi::VertexFormat::Float32x3, .offset = offsetof(CubeVertex, position), .shader_location = 0},
        rhi::VertexAttributeDesc{.format = rhi::VertexFormat::Float32x3, .offset = offsetof(CubeVertex, normal), .shader_location = 1},
        rhi::VertexAttributeDesc{.format = rhi::VertexFormat::Float32x4, .offset = offsetof(CubeVertex, tangent), .shader_location = 2},
        rhi::VertexAttributeDesc{.format = rhi::VertexFormat::Float32x2, .offset = offsetof(CubeVertex, uv), .shader_location = 3},
        rhi::VertexAttributeDesc{.format = rhi::VertexFormat::Float32x4, .offset = offsetof(CubeVertex, color), .shader_location = 4},
    };
    const rhi::VertexBufferLayoutDesc vertex_layout{.array_stride = sizeof(CubeVertex), .attributes = attributes};
    const rhi::VertexStateDesc vertex_state{.module = shader_.get(), .entry_point = "cube_vs", .buffers = std::span(&vertex_layout, 1)};
    const rhi::ColorTargetStateDesc color_target{.format = color_format_};
    const rhi::FragmentStateDesc fragment_state{.module = shader_.get(), .entry_point = "cube_fs", .targets = std::span(&color_target, 1)};
    const rhi::PrimitiveStateDesc primitive_state{.topology = rhi::PrimitiveTopology::TriangleList, .front_face = rhi::FrontFace::CCW, .cull_mode = rhi::CullMode::Back};
    const rhi::DepthStencilStateDesc depth_state{.format = rhi::TextureFormat::Depth24PlusStencil8, .depth_write_enabled = true, .depth_compare = rhi::CompareFunction::Less};
    rhi::RenderPipelineDescTyped pipeline_desc{.layout = pipeline_layout_.get(),
        .vertex = &vertex_state,
        .primitive = &primitive_state,
        .depth_stencil = &depth_state,
        .fragment = &fragment_state,
        .label = "StandardColoredCube"};
    TRY_ASSIGN(pipeline_, device_->CreateRenderPipeline(pipeline_desc));
    return Ok();
}

Result<void> RenderLayer::InitializeCameras() {
    camera_viewports_ = {{{0.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 0.5f, 0.5f}, {0.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f}}};
    camera_projections_[0] = gfx::PerspectiveCamera{.vertical_fov = math::radians(55.0f), .near_plane = 0.1f, .far_plane = 100.0f};
    for (u32 index = 1; index < 4; ++index)
        camera_projections_[index] = gfx::OrthographicCamera{.height = 5.0f, .near_plane = 0.1f, .far_plane = 100.0f};
    orbit_.yaw = 0.7f;
    orbit_.pitch = 0.45f;
    orbit_.distance = 6.0f;
    TRY_VOID(orbit_.Update(camera_poses_[0]));
    camera_poses_[1].position = {0.0f, 0.0f, 6.0f};
    camera_poses_[2].position = {6.0f, 0.0f, 0.0f};
    camera_poses_[3].position = {0.0f, 6.0f, 0.001f};
    TRY_VOID(camera_poses_[1].LookAt({}));
    TRY_VOID(camera_poses_[2].LookAt({}));
    TRY_VOID(camera_poses_[3].LookAt({}, {0.0f, 0.0f, -1.0f}));
    return Ok();
}

Result<void> RenderLayer::BuildRenderGraph() {
    rhi::RenderGraphBuilder builder(device_);
    backbuffer_ = builder.PerFrame();
    depth_ = builder.Transient({.label = "FourViewDepth", .format = rhi::TextureFormat::Depth24PlusStencil8, .usage = rhi::TextureUsage::RenderAttachment, .extent = rhi::ExtentMode::Swapchain()});
    pass_state_ = createRef<CubePassState>();
    pass_state_->pipeline = pipeline_;
    pass_state_->view_bind_group = view_bind_group_;
    pass_state_->object_bind_group = object_bind_group_;
    pass_state_->vertex_buffer = vertex_buffer_;
    pass_state_->index_buffer = index_buffer_;
    pass_state_->uniform_stride = view_uniform_stride_;
    builder.AddPass("FourViewCube").Color(0, backbuffer_, {.load = rhi::LoadOp::Clear, .clear = kBackgroundColor}).Depth(depth_, {.load = rhi::LoadOp::Clear, .clear = 1.0f}).Execute([](rhi::RenderPassContext& ctx) {
        const auto& state = *ctx.data<ref<CubePassState>>();
        auto& pass = ctx.encoder();
        pass.SetPipeline(*state.pipeline);
        pass.SetVertexBuffer(0, state.vertex_buffer.get());
        pass.SetIndexBuffer(*state.index_buffer, rhi::IndexFormat::Uint16);
        pass.SetBindGroup(3, state.object_bind_group.get());
        for (u32 index = 0; index < state.viewports.size(); ++index) {
            const auto& viewport = state.viewports[index];
            if (viewport.width == 0 || viewport.height == 0)
                continue;
            pass.SetViewport(static_cast<f32>(viewport.x), static_cast<f32>(viewport.y), static_cast<f32>(viewport.width), static_cast<f32>(viewport.height), 0.0f, 1.0f);
            pass.SetScissorRect(viewport.x, viewport.y, viewport.width, viewport.height);
            const u32 offset = index * state.uniform_stride;
            pass.SetBindGroup(1, state.view_bind_group.get(), std::span(&offset, 1));
            pass.DrawIndexed(static_cast<u32>(kCubeIndices.size()), 1);
        }
    });
    builder.SetPassData("FourViewCube", pass_state_);
    TRY_ASSIGN(render_graph_, builder.Compile(width_, height_));
    return Ok();
}

Result<void> RenderLayer::UpdateCameras(f32 delta_seconds) {
    if (right_drag_ && selected_view_ == 0) {
        TRY_VOID(fly_.Update(camera_poses_[0], fly_input_, delta_seconds));
        orbit_.yaw = fly_.yaw;
        orbit_.pitch = -fly_.pitch;
        orbit_.target = camera_poses_[0].position + camera_poses_[0].Forward() * orbit_.distance;
    } else {
        TRY_VOID(orbit_.Update(camera_poses_[0]));
    }
    fly_input_.look_x = 0.0f;
    fly_input_.look_y = 0.0f;
    for (u32 index = 0; index < camera_poses_.size(); ++index) {
        auto view = gfx::CameraView::Build(camera_poses_[index], camera_projections_[index], camera_viewports_[index], width_, height_);
        if (!view)
            return Err(view.error());
        pass_state_->viewports[index] = view->pixel_rect;
        const f32 near_plane = std::visit([](const auto& projection) { return projection.near_plane; }, camera_projections_[index]);
        const f32 far_plane = std::visit([](const auto& projection) { return projection.far_plane; }, camera_projections_[index]);
        const ViewUniforms uniform{view->view_projection, view->inverse_view_projection, camera_poses_[index].position, near_plane, {static_cast<f32>(view->pixel_rect.width), static_cast<f32>(view->pixel_rect.height)},
            far_plane};
        TRY_VOID(device_->GetQueue().WriteBuffer(*view_buffer_, static_cast<u64>(index) * view_uniform_stride_, &uniform, sizeof(uniform)));
    }
    return Ok();
}

Result<void> RenderLayer::Resize(u32 width, u32 height) {
    if (width == 0 || height == 0) {
        minimized_ = true;
        return Ok();
    }
    minimized_ = false;
    if (width == width_ && height == height_)
        return Ok();
    width_ = width;
    height_ = height;
    swapchain_->Resize(width_, height_);
    TRY_VOID(render_graph_->RebuildForResize(width_, height_));
    return UpdateCameras(0.0f);
}

Result<void> RenderLayer::RenderFrame(f32 delta_seconds) {
    TRY_VOID(UpdateCameras(delta_seconds));
    auto graph_frame = render_graph_->BeginFrame(width_, height_);
    if (!graph_frame)
        return Err(graph_frame.error());
    auto frame = swapchain_->AcquireNextFrame();
    if (!frame) {
        instance_->ProcessEvents();
        return Err(frame.error());
    }
    graph_frame->Bind(backbuffer_, frame->ColorViewRef());
    if (auto executed = graph_frame->Execute(); !executed) {
        swapchain_->Discard();
        return Err(executed.error());
    }
    TRY_VOID(swapchain_->Present());
    device_->Tick();
    instance_->ProcessEvents();
    return Ok();
}

void RenderLayer::ClearInput() noexcept {
    left_drag_ = false;
    middle_drag_ = false;
    right_drag_ = false;
    fly_input_ = {};
    forward_key_ = false;
    backward_key_ = false;
    left_key_ = false;
    right_key_ = false;
    up_key_ = false;
    down_key_ = false;
    left_boost_key_ = false;
    right_boost_key_ = false;
    fly_.Stop();
}

void RenderLayer::Shutdown() noexcept {
    ready_ = false;
    ClearInput();
    pass_state_.reset();
    render_graph_.reset();
    pipeline_.reset();
    pipeline_layout_.reset();
    object_bind_group_.reset();
    view_bind_group_.reset();
    for (auto& layout : bind_group_layouts_)
        layout.reset();
    joints_buffer_.reset();
    object_buffer_.reset();
    view_buffer_.reset();
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
    minimized_ = false;
}

} // namespace woki
