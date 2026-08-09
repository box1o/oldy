#include <array>
#include <cmath>
#include <cstddef>
#include <algorithm>

#include <woki/rhi.hpp>
#include <woki/math/math.hpp>

#include "rhi_renderer.hpp"

namespace woki {

namespace {

constexpr f32 kMouseRotationSpeed = 0.008f;

struct CubeVertex final {
    math::vec3f position;
    math::vec3f normal;
    math::vec3f color;
};

struct alignas(16) CubeUniforms final {
    math::mat4f mvp;
};

struct GBufferPassData final {
    ref<rhi::RenderPipeline> pipeline{};
    ref<rhi::BindGroup> bind_group{};
    ref<rhi::Buffer> vertex_buffer{};
    ref<rhi::Buffer> index_buffer{};
    u32 index_count{0};
};

struct LightingPassData final {
    ref<rhi::RenderPipeline> pipeline{};
    ref<rhi::BindGroupLayout> bind_group_layout{};
    ref<rhi::Sampler> sampler{};
};

struct PresentPassData final {
    ref<rhi::RenderPipeline> pipeline{};
    ref<rhi::BindGroupLayout> bind_group_layout{};
    ref<rhi::RenderPipeline> debug_pipeline{};
    ref<rhi::BindGroupLayout> debug_bind_group_layout{};
    ref<rhi::Sampler> sampler{};
    ref<bool> show_texture_debug{};
};

constexpr const char* kGBufferWgsl = R"(
struct Uniforms {
    mvp: mat4x4<f32>,
};

@group(0) @binding(0)
var<uniform> uniforms: Uniforms;

struct VSOut {
    @builtin(position) clip: vec4f,
    @location(0) world_normal: vec3f,
    @location(1) albedo: vec3f,
    @location(2) roughness: f32,
};

@vertex
fn vs_main(
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) color: vec3f
) -> VSOut {
    var output: VSOut;
    output.clip = uniforms.mvp * vec4f(position, 1.0);
    output.world_normal = normalize(normal);
    output.albedo = color;
    output.roughness = 0.5;
    return output;
}

struct GBufferOut {
    @location(0) albedo: vec4f,
    @location(1) normal: vec4f,
    @location(2) material: vec4f,
};

@fragment
fn fs_main(input: VSOut) -> GBufferOut {
    var output: GBufferOut;
    let normal = normalize(input.world_normal);
    output.albedo = vec4f(input.albedo, 1.0);
    output.normal = vec4f(normal * 0.5 + 0.5, 1.0);
    output.material = vec4f(input.roughness, 0.0, 0.0, 1.0);
    return output;
}
)";

constexpr const char* kLightingWgsl = R"(
struct VSOut {
    @builtin(position) clip: vec4f,
    @location(0) uv: vec2f,
};

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32) -> VSOut {
    var output: VSOut;
    let x = f32((vertex_index << 1u) & 2u);
    let y = f32(vertex_index & 2u);
    output.uv = vec2f(x, 1.0 - y);
    output.clip = vec4f(x * 2.0 - 1.0, y * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

@group(0) @binding(0) var g_albedo: texture_2d<f32>;
@group(0) @binding(1) var g_normal: texture_2d<f32>;
@group(0) @binding(2) var g_material: texture_2d<f32>;
@group(0) @binding(3) var g_depth: texture_depth_2d;
@group(0) @binding(4) var linear_sampler: sampler;

@fragment
fn fs_main(input: VSOut) -> @location(0) vec4f {
    let albedo = textureSample(g_albedo, linear_sampler, input.uv).rgb;
    let normal = normalize(textureSample(g_normal, linear_sampler, input.uv).xyz * 2.0 - 1.0);

    let light_dir = normalize(vec3f(0.35, 0.85, 0.4));
    let ambient = 0.22;
    let diffuse = max(dot(normal, light_dir), 0.0);

    let color = albedo * (ambient + diffuse * 0.85);
    return vec4f(color, 1.0);
}
)";

constexpr const char* kPresentWgsl = R"(
struct VSOut {
    @builtin(position) clip: vec4f,
    @location(0) uv: vec2f,
};

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32) -> VSOut {
    var output: VSOut;
    let x = f32((vertex_index << 1u) & 2u);
    let y = f32(vertex_index & 2u);
    output.uv = vec2f(x, 1.0 - y);
    output.clip = vec4f(x * 2.0 - 1.0, y * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

@group(0) @binding(4) var hdr_tex: texture_2d<f32>;
@group(0) @binding(5) var linear_sampler: sampler;

@fragment
fn fs_main(input: VSOut) -> @location(0) vec4f {
    let hdr = textureSample(hdr_tex, linear_sampler, input.uv).rgb;
    let mapped = hdr / (hdr + vec3f(1.0));
    let gamma_corrected = pow(mapped, vec3f(1.0 / 2.2));
    return vec4f(gamma_corrected, 1.0);
}
)";

constexpr const char* kTextureDebugWgsl = R"(
struct VSOut {
    @builtin(position) clip: vec4f,
    @location(0) uv: vec2f,
};

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32) -> VSOut {
    var output: VSOut;
    let x = f32((vertex_index << 1u) & 2u);
    let y = f32(vertex_index & 2u);
    output.uv = vec2f(x, 1.0 - y);
    output.clip = vec4f(x * 2.0 - 1.0, y * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

@group(0) @binding(0) var g_albedo: texture_2d<f32>;
@group(0) @binding(1) var g_normal: texture_2d<f32>;
@group(0) @binding(2) var g_material: texture_2d<f32>;
@group(0) @binding(3) var g_depth: texture_depth_2d;
@group(0) @binding(4) var g_hdr: texture_2d<f32>;
@group(0) @binding(5) var linear_sampler: sampler;

@fragment
fn fs_main(input: VSOut) -> @location(0) vec4f {
    let grid = vec2f(3.0, 2.0);
    let cell = input.uv * grid;
    let index = u32(cell.y) * 3u + u32(cell.x);
    let local_uv = fract(cell);

    let albedo = textureSample(g_albedo, linear_sampler, local_uv);
    let normal = textureSample(g_normal, linear_sampler, local_uv);
    let material = textureSample(g_material, linear_sampler, local_uv);

    let depth_size = vec2f(textureDimensions(g_depth));
    let depth_xy = vec2i(clamp(local_uv * depth_size, vec2f(0.0), depth_size - vec2f(1.0)));
    let depth = textureLoad(g_depth, depth_xy, 0);

    let hdr = textureSample(g_hdr, linear_sampler, local_uv).rgb;
    let tonemapped_hdr = hdr / (hdr + vec3f(1.0));
    let gamma_corrected_hdr = pow(tonemapped_hdr, vec3f(1.0 / 2.2));

    if (index == 0u) {
        return albedo;
    }
    if (index == 1u) {
        return normal;
    }
    if (index == 2u) {
        return material;
    }
    if (index == 3u) {
        return vec4f(vec3f(depth), 1.0);
    }
    if (index == 4u) {
        return vec4f(gamma_corrected_hdr, 1.0);
    }

    return vec4f(0.08, 0.08, 0.1, 1.0);
}
)";

constexpr std::array<CubeVertex, 24> kCubeVertices = {
    CubeVertex{{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.93f, 0.25f, 0.21f}},
    CubeVertex{{1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.93f, 0.25f, 0.21f}},
    CubeVertex{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.93f, 0.25f, 0.21f}},
    CubeVertex{{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.93f, 0.25f, 0.21f}},

    CubeVertex{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.16f, 0.55f, 0.96f}},
    CubeVertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.16f, 0.55f, 0.96f}},
    CubeVertex{{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.16f, 0.55f, 0.96f}},
    CubeVertex{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.16f, 0.55f, 0.96f}},

    CubeVertex{{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.26f, 0.78f, 0.39f}},
    CubeVertex{{-1.0f, -1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.26f, 0.78f, 0.39f}},
    CubeVertex{{-1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.26f, 0.78f, 0.39f}},
    CubeVertex{{-1.0f, 1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.26f, 0.78f, 0.39f}},

    CubeVertex{{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.96f, 0.70f, 0.18f}},
    CubeVertex{{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.96f, 0.70f, 0.18f}},
    CubeVertex{{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.96f, 0.70f, 0.18f}},
    CubeVertex{{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.96f, 0.70f, 0.18f}},

    CubeVertex{{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.65f, 0.38f, 0.92f}},
    CubeVertex{{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.65f, 0.38f, 0.92f}},
    CubeVertex{{1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.65f, 0.38f, 0.92f}},
    CubeVertex{{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.65f, 0.38f, 0.92f}},

    CubeVertex{{-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.07f, 0.72f, 0.73f}},
    CubeVertex{{1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.07f, 0.72f, 0.73f}},
    CubeVertex{{1.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.07f, 0.72f, 0.73f}},
    CubeVertex{{-1.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.07f, 0.72f, 0.73f}},
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

GBufferPassData g_gbuffer_pass_data{};
LightingPassData g_lighting_pass_data{};
PresentPassData g_present_pass_data{};

[[nodiscard]] math::mat4f PerspectiveWebGpu(const f32 fovy, const f32 aspect, const f32 z_near, const f32 z_far) {
    const f32 tan_half = std::tan(fovy * 0.5f);
    const f32 depth = z_near - z_far;

    return math::mat4f(math::layout::rowm, 1.0f / (aspect * tan_half), 0.0f, 0.0f, 0.0f, 0.0f, 1.0f / tan_half, 0.0f, 0.0f, 0.0f, 0.0f, z_far / depth, (z_near * z_far) / depth, 0.0f, 0.0f, -1.0f, 0.0f);
}

[[nodiscard]] bool CreateShader(rhi::Device& device, const char* label, const char* code, ref<rhi::ShaderModule>& out) {
    rhi::ShaderModuleDesc desc{};
    desc.label = label;
    desc.code = code;
    auto shader = device.CreateShaderModule(desc);
    if (!shader) {
        slog::Error("RHI shader '{}' creation failed: {}", label, shader.error().Message());
        return false;
    }
    out = std::move(*shader);
    return true;
}

} // namespace

bool RhiRenderer::UploadCubeResources() {
    const auto upload_buffer = [this](const rhi::Buffer& buffer, const void* data, const u64 size, const char* label) {
        if (auto write = device_->GetQueue().WriteBuffer(buffer, 0, data, size); !write) {
            slog::Error("RHI {} upload failed: {}", label, write.error().Message());
            return false;
        }
        return true;
    };

    rhi::BufferDesc vertex_buffer_desc{};
    vertex_buffer_desc.label = "StudioCubeVertices";
    vertex_buffer_desc.size = sizeof(kCubeVertices);
    vertex_buffer_desc.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst;
    auto vertex_buffer = device_->CreateBuffer(vertex_buffer_desc);
    if (!vertex_buffer) {
        slog::Error("RHI vertex buffer creation failed: {}", vertex_buffer.error().Message());
        return false;
    }
    vertex_buffer_ = std::move(*vertex_buffer);

    if (!upload_buffer(*vertex_buffer_, kCubeVertices.data(), sizeof(kCubeVertices), "vertex buffer")) {
        return false;
    }

    rhi::BufferDesc index_buffer_desc{};
    index_buffer_desc.label = "StudioCubeIndices";
    index_buffer_desc.size = sizeof(kCubeIndices);
    index_buffer_desc.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst;
    auto index_buffer = device_->CreateBuffer(index_buffer_desc);
    if (!index_buffer) {
        slog::Error("RHI index buffer creation failed: {}", index_buffer.error().Message());
        return false;
    }
    index_buffer_ = std::move(*index_buffer);

    if (!upload_buffer(*index_buffer_, kCubeIndices.data(), sizeof(kCubeIndices), "index buffer")) {
        return false;
    }

    rhi::BufferDesc uniform_buffer_desc{};
    uniform_buffer_desc.label = "StudioCubeUniforms";
    uniform_buffer_desc.size = sizeof(CubeUniforms);
    uniform_buffer_desc.usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst;
    auto uniform_buffer = device_->CreateBuffer(uniform_buffer_desc);
    if (!uniform_buffer) {
        slog::Error("RHI uniform buffer creation failed: {}", uniform_buffer.error().Message());
        return false;
    }
    uniform_buffer_ = std::move(*uniform_buffer);

    return UpdateUniforms(0.0);
}

bool RhiRenderer::CreateDeferredPipelines() {
    if (!CreateShader(*device_, "GBufferShader", kGBufferWgsl, gbuffer_shader_)) {
        return false;
    }
    if (!CreateShader(*device_, "LightingShader", kLightingWgsl, lighting_shader_)) {
        return false;
    }
    if (!CreateShader(*device_, "PresentShader", kPresentWgsl, present_shader_)) {
        return false;
    }
    if (!CreateShader(*device_, "TextureDebugShader", kTextureDebugWgsl, texture_debug_shader_)) {
        return false;
    }

    if (!UploadCubeResources()) {
        return false;
    }

    const rhi::BindGroupLayoutEntryDesc gbuffer_uniform_entry{
        .binding = 0,
        .visibility = static_cast<u32>(rhi::ShaderStage::Vertex),
        .buffer =
            rhi::BufferBindingLayoutDesc{
                .type = rhi::BufferBindingType::Uniform,
                .min_binding_size = sizeof(CubeUniforms),
            },
    };
    rhi::BindGroupLayoutDesc gbuffer_layout_desc{};
    gbuffer_layout_desc.label = "GBufferBindGroupLayout";
    gbuffer_layout_desc.entries = std::span<const rhi::BindGroupLayoutEntryDesc>(&gbuffer_uniform_entry, 1);
    auto gbuffer_layout = device_->CreateBindGroupLayout(gbuffer_layout_desc);
    if (!gbuffer_layout) {
        slog::Error("RHI gbuffer bind group layout failed: {}", gbuffer_layout.error().Message());
        return false;
    }
    gbuffer_bind_group_layout_ = std::move(*gbuffer_layout);

    const rhi::BindGroupEntryDesc gbuffer_binding{
        .binding = 0,
        .buffer = uniform_buffer_.get(),
        .offset = 0,
        .size = sizeof(CubeUniforms),
    };
    rhi::BindGroupDesc gbuffer_bind_group_desc{};
    gbuffer_bind_group_desc.label = "GBufferBindGroup";
    gbuffer_bind_group_desc.layout = gbuffer_bind_group_layout_.get();
    gbuffer_bind_group_desc.entries = std::span<const rhi::BindGroupEntryDesc>(&gbuffer_binding, 1);
    auto gbuffer_bind_group = device_->CreateBindGroup(gbuffer_bind_group_desc);
    if (!gbuffer_bind_group) {
        slog::Error("RHI gbuffer bind group failed: {}", gbuffer_bind_group.error().Message());
        return false;
    }
    gbuffer_bind_group_ = std::move(*gbuffer_bind_group);

    const std::array lighting_layout_entries{
        rhi::BindGroupLayoutEntryDesc{
            .binding = 0,
            .visibility = static_cast<u32>(rhi::ShaderStage::Fragment),
            .texture =
                rhi::TextureBindingLayoutDesc{
                    .sample_type = rhi::TextureSampleType::Float,
                    .view_dimension = rhi::TextureViewDimension::e2D,
                },
        },
        rhi::BindGroupLayoutEntryDesc{
            .binding = 1,
            .visibility = static_cast<u32>(rhi::ShaderStage::Fragment),
            .texture =
                rhi::TextureBindingLayoutDesc{
                    .sample_type = rhi::TextureSampleType::Float,
                    .view_dimension = rhi::TextureViewDimension::e2D,
                },
        },
        rhi::BindGroupLayoutEntryDesc{
            .binding = 2,
            .visibility = static_cast<u32>(rhi::ShaderStage::Fragment),
            .texture =
                rhi::TextureBindingLayoutDesc{
                    .sample_type = rhi::TextureSampleType::Float,
                    .view_dimension = rhi::TextureViewDimension::e2D,
                },
        },
        rhi::BindGroupLayoutEntryDesc{
            .binding = 3,
            .visibility = static_cast<u32>(rhi::ShaderStage::Fragment),
            .texture =
                rhi::TextureBindingLayoutDesc{
                    .sample_type = rhi::TextureSampleType::Depth,
                    .view_dimension = rhi::TextureViewDimension::e2D,
                },
        },
        rhi::BindGroupLayoutEntryDesc{
            .binding = 4,
            .visibility = static_cast<u32>(rhi::ShaderStage::Fragment),
            .sampler =
                rhi::SamplerBindingLayoutDesc{
                    .type = rhi::SamplerBindingType::Filtering,
                },
        },
    };
    rhi::BindGroupLayoutDesc lighting_layout_desc{};
    lighting_layout_desc.label = "LightingBindGroupLayout";
    lighting_layout_desc.entries = lighting_layout_entries;
    auto lighting_layout = device_->CreateBindGroupLayout(lighting_layout_desc);
    if (!lighting_layout) {
        slog::Error("RHI lighting bind group layout failed: {}", lighting_layout.error().Message());
        return false;
    }
    lighting_bind_group_layout_ = std::move(*lighting_layout);

    const std::array present_layout_entries{
        rhi::BindGroupLayoutEntryDesc{
            .binding = 0,
            .visibility = static_cast<u32>(rhi::ShaderStage::Fragment),
            .texture =
                rhi::TextureBindingLayoutDesc{
                    .sample_type = rhi::TextureSampleType::Float,
                    .view_dimension = rhi::TextureViewDimension::e2D,
                },
        },
        rhi::BindGroupLayoutEntryDesc{
            .binding = 1,
            .visibility = static_cast<u32>(rhi::ShaderStage::Fragment),
            .texture =
                rhi::TextureBindingLayoutDesc{
                    .sample_type = rhi::TextureSampleType::Float,
                    .view_dimension = rhi::TextureViewDimension::e2D,
                },
        },
        rhi::BindGroupLayoutEntryDesc{
            .binding = 2,
            .visibility = static_cast<u32>(rhi::ShaderStage::Fragment),
            .texture =
                rhi::TextureBindingLayoutDesc{
                    .sample_type = rhi::TextureSampleType::Float,
                    .view_dimension = rhi::TextureViewDimension::e2D,
                },
        },
        rhi::BindGroupLayoutEntryDesc{
            .binding = 3,
            .visibility = static_cast<u32>(rhi::ShaderStage::Fragment),
            .texture =
                rhi::TextureBindingLayoutDesc{
                    .sample_type = rhi::TextureSampleType::Depth,
                    .view_dimension = rhi::TextureViewDimension::e2D,
                },
        },
        rhi::BindGroupLayoutEntryDesc{
            .binding = 4,
            .visibility = static_cast<u32>(rhi::ShaderStage::Fragment),
            .texture =
                rhi::TextureBindingLayoutDesc{
                    .sample_type = rhi::TextureSampleType::Float,
                    .view_dimension = rhi::TextureViewDimension::e2D,
                },
        },
        rhi::BindGroupLayoutEntryDesc{
            .binding = 5,
            .visibility = static_cast<u32>(rhi::ShaderStage::Fragment),
            .sampler =
                rhi::SamplerBindingLayoutDesc{
                    .type = rhi::SamplerBindingType::Filtering,
                },
        },
    };
    rhi::BindGroupLayoutDesc present_layout_desc{};
    present_layout_desc.label = "PresentBindGroupLayout";
    present_layout_desc.entries = present_layout_entries;
    auto present_layout = device_->CreateBindGroupLayout(present_layout_desc);
    if (!present_layout) {
        slog::Error("RHI present bind group layout failed: {}", present_layout.error().Message());
        return false;
    }
    present_bind_group_layout_ = std::move(*present_layout);

    auto sampler = device_->CreateSampler({.label = "LinearSampler"});
    if (!sampler) {
        slog::Error("RHI sampler creation failed: {}", sampler.error().Message());
        return false;
    }
    linear_sampler_ = std::move(*sampler);

    rhi::BindGroupLayout* gbuffer_layouts[] = {gbuffer_bind_group_layout_.get()};
    rhi::PipelineLayoutDesc gbuffer_pipeline_layout_desc{};
    gbuffer_pipeline_layout_desc.label = "GBufferPipelineLayout";
    gbuffer_pipeline_layout_desc.bind_group_layouts = std::span<rhi::BindGroupLayout* const>(gbuffer_layouts, 1);
    auto gbuffer_pl = device_->CreatePipelineLayout(gbuffer_pipeline_layout_desc);
    if (!gbuffer_pl) {
        slog::Error("RHI gbuffer pipeline layout failed: {}", gbuffer_pl.error().Message());
        return false;
    }
    gbuffer_pipeline_layout_ = std::move(*gbuffer_pl);

    rhi::BindGroupLayout* lighting_layouts[] = {lighting_bind_group_layout_.get()};
    rhi::PipelineLayoutDesc lighting_pipeline_layout_desc{};
    lighting_pipeline_layout_desc.label = "LightingPipelineLayout";
    lighting_pipeline_layout_desc.bind_group_layouts = std::span<rhi::BindGroupLayout* const>(lighting_layouts, 1);
    auto lighting_pl = device_->CreatePipelineLayout(lighting_pipeline_layout_desc);
    if (!lighting_pl) {
        slog::Error("RHI lighting pipeline layout failed: {}", lighting_pl.error().Message());
        return false;
    }
    lighting_pipeline_layout_ = std::move(*lighting_pl);

    rhi::BindGroupLayout* present_layouts[] = {present_bind_group_layout_.get()};
    rhi::PipelineLayoutDesc present_pipeline_layout_desc{};
    present_pipeline_layout_desc.label = "PresentPipelineLayout";
    present_pipeline_layout_desc.bind_group_layouts = std::span<rhi::BindGroupLayout* const>(present_layouts, 1);
    auto present_pl = device_->CreatePipelineLayout(present_pipeline_layout_desc);
    if (!present_pl) {
        slog::Error("RHI present pipeline layout failed: {}", present_pl.error().Message());
        return false;
    }
    present_pipeline_layout_ = std::move(*present_pl);

    const std::array vertex_attributes{
        rhi::VertexAttributeDesc{
            .format = rhi::VertexFormat::Float32x3,
            .offset = offsetof(CubeVertex, position),
            .shader_location = 0,
        },
        rhi::VertexAttributeDesc{
            .format = rhi::VertexFormat::Float32x3,
            .offset = offsetof(CubeVertex, normal),
            .shader_location = 1,
        },
        rhi::VertexAttributeDesc{
            .format = rhi::VertexFormat::Float32x3,
            .offset = offsetof(CubeVertex, color),
            .shader_location = 2,
        },
    };
    const rhi::VertexBufferLayoutDesc vertex_buffer_layout{
        .array_stride = sizeof(CubeVertex),
        .attributes = std::span<const rhi::VertexAttributeDesc>(vertex_attributes.data(), vertex_attributes.size()),
    };
    const rhi::VertexStateDesc gbuffer_vertex_state{
        .module = gbuffer_shader_.get(),
        .entry_point = "vs_main",
        .buffers = std::span<const rhi::VertexBufferLayoutDesc>(&vertex_buffer_layout, 1),
    };

    const std::array gbuffer_color_targets{
        rhi::ColorTargetStateDesc{.format = rhi::TextureFormat::BGRA8Unorm},
        rhi::ColorTargetStateDesc{.format = rhi::TextureFormat::RGBA16Float},
        rhi::ColorTargetStateDesc{.format = rhi::TextureFormat::RGBA8Unorm},
    };
    const rhi::FragmentStateDesc gbuffer_fragment_state{
        .module = gbuffer_shader_.get(),
        .entry_point = "fs_main",
        .targets = gbuffer_color_targets,
    };
    const rhi::PrimitiveStateDesc gbuffer_primitive_state{
        .topology = rhi::PrimitiveTopology::TriangleList,
        .front_face = rhi::FrontFace::CCW,
        .cull_mode = rhi::CullMode::None,
    };
    const rhi::DepthStencilStateDesc gbuffer_depth_state{
        .format = rhi::TextureFormat::Depth24PlusStencil8,
        .depth_write_enabled = true,
        .depth_compare = rhi::CompareFunction::Less,
    };
    rhi::RenderPipelineDescTyped gbuffer_pipeline_desc{};
    gbuffer_pipeline_desc.label = "GBufferPipeline";
    gbuffer_pipeline_desc.layout = gbuffer_pipeline_layout_.get();
    gbuffer_pipeline_desc.vertex = &gbuffer_vertex_state;
    gbuffer_pipeline_desc.primitive = &gbuffer_primitive_state;
    gbuffer_pipeline_desc.depth_stencil = &gbuffer_depth_state;
    gbuffer_pipeline_desc.fragment = &gbuffer_fragment_state;
    auto gbuffer_pipeline = device_->CreateRenderPipeline(gbuffer_pipeline_desc);
    if (!gbuffer_pipeline) {
        slog::Error("RHI gbuffer pipeline failed: {}", gbuffer_pipeline.error().Message());
        return false;
    }
    gbuffer_pipeline_ = std::move(*gbuffer_pipeline);

    const rhi::VertexStateDesc fullscreen_vertex_state{
        .module = lighting_shader_.get(),
        .entry_point = "vs_main",
    };
    const rhi::ColorTargetStateDesc lighting_color_target{
        .format = rhi::TextureFormat::RGBA16Float,
    };
    const rhi::FragmentStateDesc lighting_fragment_state{
        .module = lighting_shader_.get(),
        .entry_point = "fs_main",
        .targets = std::span<const rhi::ColorTargetStateDesc>(&lighting_color_target, 1),
    };
    rhi::RenderPipelineDescTyped lighting_pipeline_desc{};
    lighting_pipeline_desc.label = "LightingPipeline";
    lighting_pipeline_desc.layout = lighting_pipeline_layout_.get();
    lighting_pipeline_desc.vertex = &fullscreen_vertex_state;
    lighting_pipeline_desc.fragment = &lighting_fragment_state;
    auto lighting_pipeline = device_->CreateRenderPipeline(lighting_pipeline_desc);
    if (!lighting_pipeline) {
        slog::Error("RHI lighting pipeline failed: {}", lighting_pipeline.error().Message());
        return false;
    }
    lighting_pipeline_ = std::move(*lighting_pipeline);

    const rhi::VertexStateDesc present_vertex_state{
        .module = present_shader_.get(),
        .entry_point = "vs_main",
    };
    const rhi::ColorTargetStateDesc present_color_target{
        .format = color_format_,
    };
    const rhi::FragmentStateDesc present_fragment_state{
        .module = present_shader_.get(),
        .entry_point = "fs_main",
        .targets = std::span<const rhi::ColorTargetStateDesc>(&present_color_target, 1),
    };
    rhi::RenderPipelineDescTyped present_pipeline_desc{};
    present_pipeline_desc.label = "PresentPipeline";
    present_pipeline_desc.layout = present_pipeline_layout_.get();
    present_pipeline_desc.vertex = &present_vertex_state;
    present_pipeline_desc.fragment = &present_fragment_state;
    auto present_pipeline = device_->CreateRenderPipeline(present_pipeline_desc);
    if (!present_pipeline) {
        slog::Error("RHI present pipeline failed: {}", present_pipeline.error().Message());
        return false;
    }
    present_pipeline_ = std::move(*present_pipeline);

    const rhi::VertexStateDesc debug_vertex_state{
        .module = texture_debug_shader_.get(),
        .entry_point = "vs_main",
    };
    const rhi::FragmentStateDesc debug_fragment_state{
        .module = texture_debug_shader_.get(),
        .entry_point = "fs_main",
        .targets = std::span<const rhi::ColorTargetStateDesc>(&present_color_target, 1),
    };
    rhi::RenderPipelineDescTyped debug_pipeline_desc{};
    debug_pipeline_desc.label = "TextureDebugPipeline";
    debug_pipeline_desc.layout = present_pipeline_layout_.get();
    debug_pipeline_desc.vertex = &debug_vertex_state;
    debug_pipeline_desc.fragment = &debug_fragment_state;
    auto debug_pipeline = device_->CreateRenderPipeline(debug_pipeline_desc);
    if (!debug_pipeline) {
        slog::Error("RHI texture debug pipeline failed: {}", debug_pipeline.error().Message());
        return false;
    }
    texture_debug_pipeline_ = std::move(*debug_pipeline);

    g_gbuffer_pass_data = GBufferPassData{
        .pipeline = gbuffer_pipeline_,
        .bind_group = gbuffer_bind_group_,
        .vertex_buffer = vertex_buffer_,
        .index_buffer = index_buffer_,
        .index_count = static_cast<u32>(kCubeIndices.size()),
    };
    g_lighting_pass_data = LightingPassData{
        .pipeline = lighting_pipeline_,
        .bind_group_layout = lighting_bind_group_layout_,
        .sampler = linear_sampler_,
    };
    g_present_pass_data = PresentPassData{
        .pipeline = present_pipeline_,
        .bind_group_layout = present_bind_group_layout_,
        .debug_pipeline = texture_debug_pipeline_,
        .debug_bind_group_layout = present_bind_group_layout_,
        .sampler = linear_sampler_,
        .show_texture_debug = show_texture_debug_,
    };

    return true;
}

bool RhiRenderer::BuildRenderGraph() {
    if (device_ == nullptr || gbuffer_pipeline_ == nullptr) {
        return false;
    }

    rhi::RenderGraphBuilder builder(device_);
    backbuffer_ = builder.PerFrame();

    const rhi::ExtentMode swap_extent = rhi::ExtentMode::Swapchain();
    const auto transient_usage = rhi::TextureUsage::RenderAttachment | rhi::TextureUsage::TextureBinding;

    gbuffer_albedo_ = builder.Transient({
        .label = "GBuffer.Albedo",
        .format = rhi::TextureFormat::BGRA8Unorm,
        .usage = transient_usage,
        .extent = swap_extent,
    });
    gbuffer_normal_ = builder.Transient({
        .label = "GBuffer.Normal",
        .format = rhi::TextureFormat::RGBA16Float,
        .usage = transient_usage,
        .extent = swap_extent,
    });
    gbuffer_material_ = builder.Transient({
        .label = "GBuffer.Material",
        .format = rhi::TextureFormat::RGBA8Unorm,
        .usage = transient_usage,
        .extent = swap_extent,
    });
    gbuffer_depth_ = builder.Transient({
        .label = "GBuffer.Depth",
        .format = rhi::TextureFormat::Depth24PlusStencil8,
        .usage = transient_usage,
        .extent = swap_extent,
    });
    hdr_color_ = builder.Transient({
        .label = "HDR.Color",
        .format = rhi::TextureFormat::RGBA16Float,
        .usage = transient_usage,
        .extent = swap_extent,
    });

    const rhi::Framebuffer gbuffer = builder.Framebuffer().Color(0, gbuffer_albedo_).Color(1, gbuffer_normal_).Color(2, gbuffer_material_).Depth(gbuffer_depth_).Build();

    builder.AddPass("GBuffer")
        .Target(gbuffer,
            {
                .clear_color =
                    {
                        rhi::Color{0.02f, 0.02f, 0.03f, 1.f},
                        rhi::Color{0.5f, 0.5f, 1.f, 1.f},
                        rhi::Color{0.f, 0.f, 0.f, 1.f},
                    },
                .clear_depth = 1.f,
            })
        .Execute([](rhi::RenderPassContext& ctx) {
            auto& data = ctx.data<GBufferPassData>();
            rhi::RenderPassEncoder& pass = ctx.encoder();
            pass.SetPipeline(*data.pipeline);
            pass.SetBindGroup(0, data.bind_group.get());
            pass.SetVertexBuffer(0, data.vertex_buffer.get());
            pass.SetIndexBuffer(*data.index_buffer, rhi::IndexFormat::Uint16);
            pass.DrawIndexed(data.index_count);
        });

    builder.AddPass("DeferredLighting")
        .Sample(gbuffer_albedo_)
        .Sample(gbuffer_normal_)
        .Sample(gbuffer_material_)
        .Sample(gbuffer_depth_, rhi::SampleMode::DepthTexture)
        .Color(0, hdr_color_,
            {
                .load = rhi::LoadOp::Clear,
                .clear = rhi::Color{0.f, 0.f, 0.f, 1.f},
            })
        .Execute([](rhi::RenderPassContext& ctx) {
            auto& data = ctx.data<LightingPassData>();
            rhi::BindGroup* bind_group = ctx.GetOrCreateBindGroup("lighting", [&]() -> scope<rhi::BindGroup> {
                auto built = rhi::BindGroupBuilder(ctx.device_ref(), data.bind_group_layout, "LightingBindGroup")
                                 .BindTexture(0, ctx.sample(0))
                                 .BindTexture(1, ctx.sample(1))
                                 .BindTexture(2, ctx.sample(2))
                                 .BindTexture(3, ctx.sample(3))
                                 .BindSampler(4, *data.sampler)
                                 .Build();
                if (!built) {
                    return nullptr;
                }
                return std::move(*built);
            });
            if (bind_group == nullptr) {
                return;
            }

            rhi::RenderPassEncoder& pass = ctx.encoder();
            pass.SetPipeline(*data.pipeline);
            pass.SetBindGroup(0, bind_group);
            pass.Draw(3);
        });

    builder.AddPass("Present")
        .Sample(gbuffer_albedo_)
        .Sample(gbuffer_normal_)
        .Sample(gbuffer_material_)
        .Sample(gbuffer_depth_, rhi::SampleMode::DepthTexture)
        .Sample(hdr_color_)
        .Color(0, backbuffer_,
            {
                .load = rhi::LoadOp::Clear,
                .clear = rhi::Color{0.f, 0.f, 0.f, 1.f},
            })
        .Execute([](rhi::RenderPassContext& ctx) {
            auto& data = ctx.data<PresentPassData>();
            const bool debug_view = data.show_texture_debug != nullptr && *data.show_texture_debug;

            rhi::BindGroup* bind_group = ctx.GetOrCreateBindGroup(debug_view ? "present_debug" : "present", [&]() -> scope<rhi::BindGroup> {
                auto built = rhi::BindGroupBuilder(ctx.device_ref(), debug_view ? data.debug_bind_group_layout : data.bind_group_layout, debug_view ? "TextureDebugBindGroup" : "PresentBindGroup")
                                 .BindTexture(0, ctx.sample(0))
                                 .BindTexture(1, ctx.sample(1))
                                 .BindTexture(2, ctx.sample(2))
                                 .BindTexture(3, ctx.sample(3))
                                 .BindTexture(4, ctx.sample(4))
                                 .BindSampler(5, *data.sampler)
                                 .Build();
                if (!built) {
                    return nullptr;
                }
                return std::move(*built);
            });
            if (bind_group == nullptr) {
                return;
            }

            rhi::RenderPassEncoder& pass = ctx.encoder();
            pass.SetPipeline(debug_view ? *data.debug_pipeline : *data.pipeline);
            pass.SetBindGroup(0, bind_group);
            pass.Draw(3);
        });

    builder.SetPassData("GBuffer", g_gbuffer_pass_data);
    builder.SetPassData("DeferredLighting", g_lighting_pass_data);
    builder.SetPassData("Present", g_present_pass_data);

    auto graph = builder.Compile(width_, height_);
    if (!graph) {
        slog::Error("RenderGraph compile failed: {}", graph.error().Message());
        return false;
    }

    render_graph_ = std::move(*graph);
    return true;
}

bool RhiRenderer::UpdateUniforms(const f64 delta_ms) {
    if (uniform_buffer_ == nullptr || device_ == nullptr) {
        return false;
    }

    (void)delta_ms;

    const f32 aspect = height_ != 0 ? static_cast<f32>(width_) / static_cast<f32>(height_) : 1.0f;
    const math::mat4f projection = PerspectiveWebGpu(math::radians(55.0f), aspect, 0.1f, 100.0f);
    const math::mat4f view = math::lookAt(math::vec3f{0.0f, 0.0f, 6.0f}, math::vec3f{0.0f, 0.0f, 0.0f}, math::vec3f{0.0f, 1.0f, 0.0f});
    const math::mat4f model = math::rotate_y(cube_yaw_) * math::rotate_x(cube_pitch_);

    const CubeUniforms uniforms{
        .mvp = projection * view * model,
    };

    if (auto write = device_->GetQueue().WriteBuffer(*uniform_buffer_, 0, &uniforms, sizeof(uniforms)); !write) {
        slog::Warn("RHI uniform update failed: {}", write.error().Message());
        return false;
    }

    return true;
}

void RhiRenderer::HandleEvent(events::Event& event) {
    switch (event.GetEventType()) {
        case events::EventType::kKeyPressed: {
            const auto& key_event = static_cast<const events::KeyPressedEvent&>(event);
            if (key_event.key == events::KeyCode::kM && key_event.repeat_count == 0) {
                *show_texture_debug_ = !*show_texture_debug_;
                slog::Info("Texture debug view {}", *show_texture_debug_ ? "enabled" : "disabled");
                event.handled = true;
            }
            break;
        }
        case events::EventType::kMouseButtonPressed: {
            const auto& mouse_event = static_cast<const events::MouseButtonPressedEvent&>(event);
            if (mouse_event.button == events::MouseButton::kLeft) {
                rotating_with_mouse_ = true;
                event.handled = true;
            }
            break;
        }
        case events::EventType::kMouseButtonReleased: {
            const auto& mouse_event = static_cast<const events::MouseButtonReleasedEvent&>(event);
            if (mouse_event.button == events::MouseButton::kLeft) {
                rotating_with_mouse_ = false;
                event.handled = true;
            }
            break;
        }
        case events::EventType::kMouseLeft:
            rotating_with_mouse_ = false;
            break;
        case events::EventType::kMouseMoved: {
            if (!rotating_with_mouse_) {
                break;
            }

            const auto& mouse_event = static_cast<const events::MouseMovedEvent&>(event);
            cube_yaw_ += mouse_event.delta_x * kMouseRotationSpeed;
            cube_pitch_ += mouse_event.delta_y * kMouseRotationSpeed;
            event.handled = true;
            break;
        }
        default:
            break;
    }
}

RhiRenderer::RhiRenderer() = default;

RhiRenderer::~RhiRenderer() {
    Shutdown();
}

bool RhiRenderer::Initialize(Window& window) {
    Shutdown();

    window_ = &window;
    width_ = window.GetWidth();
    height_ = window.GetHeight();

    auto instance = rhi::Instance::Create({});
    if (!instance) {
        slog::Error("RHI init failed: {}", instance.error().Message());
        return false;
    }
    instance_ = std::move(*instance);

    auto surface = instance_->CreateSurface(window);
    if (!surface) {
        slog::Error("RHI surface creation failed: {}", surface.error().Message());
        Shutdown();
        return false;
    }
    surface_ = std::move(*surface);

    rhi::RequestAdapterDesc adapter_desc{};
    adapter_desc.compatible_surface = surface_.get();
    auto adapter = instance_->RequestAdapter(adapter_desc);
    if (!adapter) {
        slog::Error("RHI adapter request failed: {}", adapter.error().Message());
        Shutdown();
        return false;
    }
    adapter_ = std::move(*adapter);

    rhi::DeviceDesc device_desc{};
    device_desc.label = "StudioDevice";
    device_desc.uncaptured_error_callback = [](const rhi::ErrorType type, const std::string_view message) { slog::Error("RHI uncaptured device error ({}): {}", static_cast<u32>(type), message); };
    auto device = adapter_->CreateDevice(device_desc);
    if (!device) {
        slog::Error("RHI device creation failed: {}", device.error().Message());
        Shutdown();
        return false;
    }
    device_ = std::move(*device);

    rhi::SurfaceCapabilities capabilities{};
    if (auto surface_capabilities = surface_->GetCapabilities(*adapter_, capabilities); !surface_capabilities) {
        slog::Error("RHI surface capability query failed: {}", surface_capabilities.error().Message());
        Shutdown();
        return false;
    }

    if (capabilities.formats.empty()) {
        slog::Error("RHI surface has no supported color formats");
        Shutdown();
        return false;
    }

    const auto preferred_format = std::ranges::find(capabilities.formats, rhi::TextureFormat::BGRA8Unorm);
    color_format_ = preferred_format != capabilities.formats.end() ? rhi::TextureFormat::BGRA8Unorm : capabilities.formats.front();

    auto swapchain = rhi::Swapchain::Builder(device_, surface_).Size(width_, height_).ColorFormat(color_format_).Label("StudioSwapchain").Build();
    if (!swapchain) {
        slog::Error("RHI swapchain creation failed: {}", swapchain.error().Message());
        Shutdown();
        return false;
    }
    swapchain_ = std::move(*swapchain);

    if (!CreateDeferredPipelines()) {
        Shutdown();
        return false;
    }

    if (!BuildRenderGraph()) {
        Shutdown();
        return false;
    }

    ready_ = true;
    slog::Info("RHI deferred renderer initialized ({}x{})", width_, height_);
    return true;
}

void RhiRenderer::Shutdown() noexcept {
    render_graph_.reset();
    history_texture_.reset();
    texture_debug_pipeline_.reset();
    present_pipeline_.reset();
    lighting_pipeline_.reset();
    gbuffer_pipeline_.reset();
    linear_sampler_.reset();
    present_bind_group_layout_.reset();
    lighting_bind_group_layout_.reset();
    gbuffer_bind_group_.reset();
    gbuffer_bind_group_layout_.reset();
    uniform_buffer_.reset();
    index_buffer_.reset();
    vertex_buffer_.reset();
    present_pipeline_layout_.reset();
    lighting_pipeline_layout_.reset();
    gbuffer_pipeline_layout_.reset();
    present_shader_.reset();
    texture_debug_shader_.reset();
    lighting_shader_.reset();
    gbuffer_shader_.reset();
    swapchain_.reset();
    surface_.reset();
    device_.reset();
    adapter_.reset();
    instance_.reset();
    window_ = nullptr;
    width_ = 0;
    height_ = 0;
    color_format_ = rhi::TextureFormat::BGRA8Unorm;
    cube_yaw_ = 0.65f;
    cube_pitch_ = 0.45f;
    rotating_with_mouse_ = false;
    *show_texture_debug_ = false;
    ready_ = false;
}

void RhiRenderer::Resize(const u32 width, const u32 height) {
    if (!ready_ || swapchain_ == nullptr) {
        return;
    }

    if (width == 0 || height == 0 || (width == width_ && height == height_)) {
        return;
    }

    width_ = width;
    height_ = height;

    swapchain_->Resize(width, height);

    render_graph_.reset();

    if (!BuildRenderGraph()) {
        slog::Error("RHI resize failed: could not rebuild render graph for {}x{}", width_, height_);
        ready_ = false;
        return;
    }

    slog::Info("RHI renderer resized to {}x{}", width_, height_);
}

bool RhiRenderer::RenderFrame(const f64 delta_ms) {
    if (!ready_ || device_ == nullptr || swapchain_ == nullptr || instance_ == nullptr) {
        return false;
    }

    if (window_ != nullptr) {
        const u32 window_width = window_->GetWidth();
        const u32 window_height = window_->GetHeight();
        if (window_width != width_ || window_height != height_) {
            Resize(window_width, window_height);
            if (!ready_) {
                return false;
            }
        }
    }

    if (!UpdateUniforms(delta_ms)) {
        return false;
    }

    if (render_graph_ == nullptr) {
        return false;
    }

    auto graph_frame = render_graph_->BeginFrame(width_, height_);
    if (!graph_frame) {
        slog::Warn("RenderGraph frame creation failed: {}", graph_frame.error().Message());
        return false;
    }

    auto frame = swapchain_->AcquireNextFrame();
    if (!frame) {
        slog::Warn("Failed to acquire swapchain frame: {}", frame.error().Message());
        instance_->ProcessEvents();
        return false;
    }

    graph_frame->Bind(backbuffer_, frame->ColorViewRef());

    if (auto execute = graph_frame->Execute(); !execute) {
        slog::Warn("RenderGraph execute failed: {}", execute.error().Message());
        swapchain_->Discard();
        return false;
    }

    if (auto present = swapchain_->Present(); !present) {
        slog::Warn("Swapchain present failed: {}", present.error().Message());
        return false;
    }

    device_->Tick();
    instance_->ProcessEvents();
    return true;
}

} // namespace woki
