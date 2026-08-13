#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <limits>

#include <woki/gfx/canvas.hpp>
#include <woki/rhi/render_pass_encoder.hpp>

#include "internal/canvas_feature.hpp"

namespace woki::gfx {

bool CanvasFrame::Valid() const noexcept {
    if (!surface.IsValid() || width == 0 || height == 0 || !std::isfinite(content_scale) || content_scale <= 0)
        return false;
    for (const auto& vertex : vertices)
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.u) || !std::isfinite(vertex.v)
            || !std::isfinite(vertex.r) || !std::isfinite(vertex.g) || !std::isfinite(vertex.b)
            || !std::isfinite(vertex.a) || !std::isfinite(vertex.p0) || !std::isfinite(vertex.p1)
            || !std::isfinite(vertex.p2) || !std::isfinite(vertex.p3) || !std::isfinite(vertex.q0)
            || !std::isfinite(vertex.q1))
            return false;
    for (u32 index : indices)
        if (index >= vertices.size())
            return false;
    for (const auto& batch : batches)
        if (batch.first_index > indices.size() || batch.index_count > indices.size() - batch.first_index
            || batch.index_count % 3 != 0 || !std::isfinite(batch.scissor.x) || !std::isfinite(batch.scissor.y)
            || !std::isfinite(batch.scissor.width) || !std::isfinite(batch.scissor.height))
            return false;
    return true;
}

namespace {

constexpr u32 kAtlasWidth = 128;
constexpr u32 kAtlasHeight = 72;
constexpr u32 kAtlasStride = 256;
constexpr u64 kUniformStride = 256;

struct alignas(16) CanvasUniform final {
    std::array<f32, 2> viewport;
    u32 primitive{};
    u32 fit{};
    std::array<f32, 4> rect;
    std::array<f32, 2> image_size;
    u32 target_srgb{};
    u32 padding{};
};

std::array<u8, 7> GlyphRows(u32 glyph) {
    if (glyph >= 'a' && glyph <= 'z')
        glyph -= 'a' - 'A';
#define GLYPH(c, a, b, d, e, f, g, h)                                                                                  \
    case c:                                                                                                            \
        return {                                                                                                       \
            a, b, d, e, f, g, h                                                                                        \
        }
    switch (glyph) {
        GLYPH('A', 14, 17, 17, 31, 17, 17, 17);
        GLYPH('B', 30, 17, 17, 30, 17, 17, 30);
        GLYPH('C', 14, 17, 16, 16, 16, 17, 14);
        GLYPH('D', 28, 18, 17, 17, 17, 18, 28);
        GLYPH('E', 31, 16, 16, 30, 16, 16, 31);
        GLYPH('F', 31, 16, 16, 30, 16, 16, 16);
        GLYPH('G', 14, 17, 16, 23, 17, 17, 15);
        GLYPH('H', 17, 17, 17, 31, 17, 17, 17);
        GLYPH('I', 14, 4, 4, 4, 4, 4, 14);
        GLYPH('J', 7, 2, 2, 2, 18, 18, 12);
        GLYPH('K', 17, 18, 20, 24, 20, 18, 17);
        GLYPH('L', 16, 16, 16, 16, 16, 16, 31);
        GLYPH('M', 17, 27, 21, 21, 17, 17, 17);
        GLYPH('N', 17, 25, 21, 19, 17, 17, 17);
        GLYPH('O', 14, 17, 17, 17, 17, 17, 14);
        GLYPH('P', 30, 17, 17, 30, 16, 16, 16);
        GLYPH('Q', 14, 17, 17, 17, 21, 18, 13);
        GLYPH('R', 30, 17, 17, 30, 20, 18, 17);
        GLYPH('S', 15, 16, 16, 14, 1, 1, 30);
        GLYPH('T', 31, 4, 4, 4, 4, 4, 4);
        GLYPH('U', 17, 17, 17, 17, 17, 17, 14);
        GLYPH('V', 17, 17, 17, 17, 17, 10, 4);
        GLYPH('W', 17, 17, 17, 21, 21, 21, 10);
        GLYPH('X', 17, 17, 10, 4, 10, 17, 17);
        GLYPH('Y', 17, 17, 10, 4, 4, 4, 4);
        GLYPH('Z', 31, 1, 2, 4, 8, 16, 31);
        GLYPH('0', 14, 17, 19, 21, 25, 17, 14);
        GLYPH('1', 4, 12, 4, 4, 4, 4, 14);
        GLYPH('2', 14, 17, 1, 2, 4, 8, 31);
        GLYPH('3', 30, 1, 1, 14, 1, 1, 30);
        GLYPH('4', 2, 6, 10, 18, 31, 2, 2);
        GLYPH('5', 31, 16, 16, 30, 1, 1, 30);
        GLYPH('6', 14, 16, 16, 30, 17, 17, 14);
        GLYPH('7', 31, 1, 2, 4, 8, 8, 8);
        GLYPH('8', 14, 17, 17, 14, 17, 17, 14);
        GLYPH('9', 14, 17, 17, 15, 1, 1, 14);
        GLYPH('?', 14, 17, 1, 2, 4, 0, 4);
        GLYPH('!', 4, 4, 4, 4, 4, 0, 4);
        GLYPH('.', 0, 0, 0, 0, 0, 0, 4);
        GLYPH(':', 0, 4, 0, 0, 4, 0, 0);
        GLYPH('-', 0, 0, 0, 31, 0, 0, 0);
        GLYPH('_', 0, 0, 0, 0, 0, 0, 31);
        GLYPH('/', 1, 2, 2, 4, 8, 8, 16);
        GLYPH('(', 2, 4, 8, 8, 8, 4, 2);
        GLYPH(')', 8, 4, 2, 2, 2, 4, 8);
        GLYPH('[', 14, 8, 8, 8, 8, 8, 14);
        GLYPH(']', 14, 2, 2, 2, 2, 2, 14);
        GLYPH('+', 0, 4, 4, 31, 4, 4, 0);
        GLYPH('=', 0, 0, 31, 0, 31, 0, 0);
        GLYPH('#', 10, 10, 31, 10, 31, 10, 10);
        default:
            return GlyphRows('?');
    }
#undef GLYPH
}

std::vector<std::byte> MakeAtlas() {
    std::vector<std::byte> bytes(kAtlasStride * kAtlasHeight);
    for (u32 glyph = 0x20; glyph <= 0x7e; ++glyph) {
        const u32 cell = glyph - 0x20;
        const u32 left = (cell % 16) * 8 + 1;
        const u32 top = (cell / 16) * 12 + 2;
        const auto rows = GlyphRows(glyph);
        for (u32 y = 0; y < rows.size(); ++y)
            for (u32 x = 0; x < 5; ++x)
                if ((rows[y] & (1U << (4U - x))) != 0)
                    bytes[(top + y) * kAtlasStride + left + x] = std::byte{0xff};
    }
    return bytes;
}

bool IsSrgbFormat(const rhi::TextureFormat format) {
    return format == rhi::TextureFormat::RGBA8UnormSrgb || format == rhi::TextureFormat::BGRA8UnormSrgb;
}

} // namespace

struct CanvasFeature::Impl final {
    struct Pipeline final {
        rhi::TextureFormat format{rhi::TextureFormat::Undefined};
        CanvasBlend blend{CanvasBlend::PremultipliedAlpha};
        ref<rhi::RenderPipeline> pipeline;
    };

    ref<rhi::Device> device;
    UploadScheduler* uploads{};
    ref<DeferredReleaseQueue> releases;
    ShaderLibrary shaders;
    LayoutCache layouts;
    ShaderAssetHandle shader_handle;
    BorrowedShader shader;
    BorrowedLayout layout;
    scope<BufferPool> vertices;
    scope<BufferPool> indices;
    scope<BufferPool> uniforms;
    ref<rhi::Texture> atlas;
    ref<rhi::TextureView> atlas_view;
    ref<rhi::Texture> diagnostic;
    ref<rhi::TextureView> diagnostic_view;
    ref<rhi::Sampler> linear_sampler;
    ref<rhi::Sampler> nearest_sampler;
    std::vector<Pipeline> pipelines;
    bool lost{};

    Impl(ref<rhi::Device> value, UploadScheduler& scheduler, ref<DeferredReleaseQueue> release)
        : device(std::move(value)),
          uploads(&scheduler),
          releases(std::move(release)),
          shaders(*device),
          layouts(*device) {}

    Result<void> Prepare(const asset::Product& product) {
        shader_handle = shaders.Create();
        TRY_VOID(shaders.Publish(shader_handle, product));
        TRY_ASSIGN(shader, shaders.Borrow(shader_handle));
        PipelineLayoutKey key;
        TRY_ASSIGN(key, MakePipelineLayoutKey(shader.Interface()));
        TRY_ASSIGN(layout, layouts.GetOrCreate(key));
        TRY_ASSIGN(
            vertices,
            BufferPool::Create(
                device,
                {.pool_class = BufferPoolClass::Dynamic,
                    .size = 8U * 1024U * 1024U,
                    .alignment = 16,
                    .usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst,
                    .label = "Canvas vertex ring"},
                releases
            )
        );
        TRY_ASSIGN(
            indices,
            BufferPool::Create(
                device,
                {.pool_class = BufferPoolClass::Dynamic,
                    .size = 4U * 1024U * 1024U,
                    .alignment = 4,
                    .usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst,
                    .label = "Canvas index ring"},
                releases
            )
        );
        TRY_ASSIGN(
            uniforms,
            BufferPool::Create(
                device,
                {.pool_class = BufferPoolClass::Dynamic,
                    .size = 2U * 1024U * 1024U,
                    .alignment = 256,
                    .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst,
                    .label = "Canvas uniform ring"},
                releases
            )
        );
        TRY_ASSIGN(
            atlas,
            device->CreateTexture(
                {.size = {kAtlasWidth, kAtlasHeight, 1},
                    .format = rhi::TextureFormat::R8Unorm,
                    .usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::CopyDst,
                    .label = "Built-in canvas glyph atlas"}
            )
        );
        atlas_view = ref<rhi::TextureView>(atlas
                ->CreateView({.format = rhi::TextureFormat::R8Unorm, .label = "Built-in canvas glyph atlas view"})
                .release());
        if (atlas_view == nullptr)
            return Err(ErrorCode::GraphicsResourceCreationFailed, "failed to create built-in glyph atlas view");
        TRY_ASSIGN(
            diagnostic,
            device->CreateTexture(
                {.size = {1, 1, 1},
                    .format = rhi::TextureFormat::RGBA8Unorm,
                    .usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::CopyDst,
                    .label = "Canvas diagnostic image"}
            )
        );
        diagnostic_view = ref<rhi::TextureView>(diagnostic
                ->CreateView({.format = rhi::TextureFormat::RGBA8Unorm, .label = "Canvas diagnostic image view"})
                .release());
        if (diagnostic_view == nullptr)
            return Err(ErrorCode::GraphicsResourceCreationFailed, "failed to create canvas diagnostic image view");
        TRY_ASSIGN(
            linear_sampler,
            device->CreateSampler(
                {.address_mode_u = rhi::AddressMode::ClampToEdge,
                    .address_mode_v = rhi::AddressMode::ClampToEdge,
                    .mag_filter = rhi::FilterMode::Linear,
                    .min_filter = rhi::FilterMode::Linear,
                    .label = "Canvas linear clamp"}
            )
        );
        TRY_ASSIGN(
            nearest_sampler,
            device->CreateSampler(
                {.address_mode_u = rhi::AddressMode::ClampToEdge,
                    .address_mode_v = rhi::AddressMode::ClampToEdge,
                    .mag_filter = rhi::FilterMode::Nearest,
                    .min_filter = rhi::FilterMode::Nearest,
                    .label = "Canvas nearest clamp"}
            )
        );
        TRY_VOID(uploads->Enqueue(
            TextureUploadRequest{
                .target = atlas,
                .mip_level = 0,
                .origin = {},
                .aspect = rhi::TextureAspect::All,
                .layout = {.bytes_per_row = kAtlasStride, .rows_per_image = kAtlasHeight},
                .extent = {kAtlasWidth, kAtlasHeight, 1},
                .bytes = MakeAtlas(),
                .publication = {},
            }
        ));
        std::vector<std::byte> diagnostic_bytes(256);
        diagnostic_bytes[0] = std::byte{0xff};
        diagnostic_bytes[2] = std::byte{0xff};
        diagnostic_bytes[3] = std::byte{0xff};
        TRY_VOID(uploads->Enqueue(
            TextureUploadRequest{
                .target = diagnostic,
                .mip_level = 0,
                .origin = {},
                .aspect = rhi::TextureAspect::All,
                .layout = {.bytes_per_row = 256, .rows_per_image = 1},
                .extent = {1, 1, 1},
                .bytes = std::move(diagnostic_bytes),
                .publication = {},
            }
        ));
        lost = false;
        return Ok();
    }

    Result<ref<rhi::RenderPipeline>> PipelineFor(const rhi::TextureFormat format, const CanvasBlend blend) {
        const auto found = std::ranges::find_if(pipelines, [&](const Pipeline& value) {
            return value.format == format && value.blend == blend;
        });
        if (found != pipelines.end())
            return Ok(found->pipeline);
        const std::array attributes{
            rhi::VertexAttributeDesc{.format = rhi::VertexFormat::Float32x2,
                .offset = offsetof(CanvasVertex, x),
                .shader_location = 0},
            rhi::VertexAttributeDesc{.format = rhi::VertexFormat::Float32x2,
                .offset = offsetof(CanvasVertex, u),
                .shader_location = 1},
            rhi::VertexAttributeDesc{.format = rhi::VertexFormat::Float32x4,
                .offset = offsetof(CanvasVertex, r),
                .shader_location = 2},
            rhi::VertexAttributeDesc{.format = rhi::VertexFormat::Float32x4,
                .offset = offsetof(CanvasVertex, p0),
                .shader_location = 3},
            rhi::VertexAttributeDesc{.format = rhi::VertexFormat::Float32x2,
                .offset = offsetof(CanvasVertex, q0),
                .shader_location = 4},
        };
        const std::array streams{
            rhi::VertexBufferLayoutDesc{.array_stride = sizeof(CanvasVertex), .attributes = attributes}};
        const rhi::VertexStateDesc vertex{.module = &shader.Module(), .entry_point = "canvas_vs", .buffers = streams};
        const rhi::BlendStateDesc alpha{
            {rhi::BlendOperation::Add, rhi::BlendFactor::One, rhi::BlendFactor::OneMinusSrcAlpha},
            {rhi::BlendOperation::Add, rhi::BlendFactor::One, rhi::BlendFactor::OneMinusSrcAlpha}};
        const std::array targets{rhi::ColorTargetStateDesc{.format = format,
            .blend = blend == CanvasBlend::PremultipliedAlpha ? &alpha : nullptr}};
        const rhi::FragmentStateDesc fragment{.module = &shader.Module(),
            .entry_point = "canvas_fs",
            .targets = targets};
        const rhi::PrimitiveStateDesc primitive{.topology = rhi::PrimitiveTopology::TriangleList,
            .front_face = rhi::FrontFace::CCW,
            .cull_mode = rhi::CullMode::None};
        scope<rhi::RenderPipeline> created;
        TRY_ASSIGN(
            created,
            device->CreateRenderPipeline(
                {.layout = &layout.Pipeline(),
                    .vertex = &vertex,
                    .primitive = &primitive,
                    .fragment = &fragment,
                    .label = "Canvas pipeline"}
            )
        );
        auto shared = ref<rhi::RenderPipeline>(std::move(created));
        pipelines.push_back({format, blend, shared});
        return Ok(std::move(shared));
    }
};

CanvasFeature::CanvasFeature(scope<Impl> impl)
    : impl_(std::move(impl)) {}

CanvasFeature::~CanvasFeature() = default;

Result<scope<CanvasFeature>> CanvasFeature::Create(
    ref<rhi::Device> device,
    UploadScheduler& uploads,
    ref<DeferredReleaseQueue> releases,
    const asset::Product& shader_product
) {
    if (device == nullptr)
        return Err(ErrorCode::InvalidArgument, "canvas feature requires a device");
    auto impl = createScope<Impl>(std::move(device), uploads, std::move(releases));
    TRY_VOID(impl->Prepare(shader_product));
    return Ok(scope<CanvasFeature>(new CanvasFeature(std::move(impl))));
}

Result<CanvasSubmission> CanvasFeature::Execute(
    const CanvasFrame& canvas,
    const CanvasTarget& target,
    const ImageResolver& resolve
) {
    if (impl_->lost || target.view == nullptr || target.width == 0 || target.height == 0)
        return Err(ErrorCode::GraphicsDeviceLost, "canvas target is unavailable");
    if (canvas.vertices.empty() || canvas.indices.empty() || canvas.batches.empty()) {
        RenderGraphBuilder graph;
        GraphTextureDesc output_desc;
        output_desc.label = "Empty canvas surface";
        output_desc.extent = GraphExtent::Fixed(target.width, target.height);
        output_desc.format = target.format;
        output_desc.usage = rhi::TextureUsage::RenderAttachment;
        ExternalTextureContract output_contract;
        output_contract.descriptor = output_desc;
        output_contract.initial_state = ExternalState::Present;
        output_contract.final_state = ExternalState::Present;
        output_contract.frame_bound = true;
        const auto output = graph.ImportTexture(std::move(output_contract));
        auto pass = graph.AddPass("Empty canvas clear", PassKind::Render);
        ColorAttachment attachment;
        attachment.load = rhi::LoadOp::Clear;
        attachment.store = rhi::StoreOp::Store;
        attachment.clear = {0.035, 0.04, 0.055, 1.0};
        const auto written = pass.Color(output, std::move(attachment));
        pass.Execute([](RenderGraphContext&) { return Ok(); });
        TRY_VOID(graph.Export(written, ExternalState::Present));
        CompiledRenderGraph compiled;
        TRY_ASSIGN(compiled, graph.Compile(target.width, target.height));
        GraphExecutor executor(impl_->device, std::move(compiled), impl_->releases);
        auto begun = executor.Begin(target.width, target.height);
        if (!begun)
            return Err(std::move(begun).error());
        auto graph_frame = std::move(*begun);
        GraphTextureBinding output_binding;
        output_binding.resource = output;
        output_binding.texture = target.texture;
        output_binding.view = target.view;
        output_binding.signature = output_desc;
        TRY_VOID(graph_frame.Bind(std::move(output_binding)));
        CanvasSubmission result;
        TRY_ASSIGN(result.submission, graph_frame.Execute());
        return Ok(std::move(result));
    }

    impl_->vertices->Collect(impl_->device->GetQueue().CompletedSubmission());
    impl_->indices->Collect(impl_->device->GetQueue().CompletedSubmission());
    impl_->uniforms->Collect(impl_->device->GetQueue().CompletedSubmission());
    BufferSlice vertex_slice, index_slice, uniform_slice;
    TRY_ASSIGN(
        vertex_slice,
        impl_->vertices->Allocate(canvas.vertices.size() * sizeof(CanvasVertex), alignof(CanvasVertex))
    );
    auto index_result = impl_->indices->Allocate(canvas.indices.size() * sizeof(u32), alignof(u32));
    if (!index_result) {
        static_cast<void>(impl_->vertices->Free(vertex_slice.allocation));
        return Err(std::move(index_result).error());
    }
    index_slice = *index_result;
    auto uniform_result = impl_->uniforms->Allocate(canvas.batches.size() * kUniformStride, kUniformStride);
    if (!uniform_result) {
        static_cast<void>(impl_->vertices->Free(vertex_slice.allocation));
        static_cast<void>(impl_->indices->Free(index_slice.allocation));
        return Err(std::move(uniform_result).error());
    }
    uniform_slice = *uniform_result;

    struct Draw final {
        CanvasBatch batch;
        ref<rhi::RenderPipeline> pipeline;
        ref<rhi::BindGroup> frame_group;
        ref<rhi::Sampler> sampler;
        ref<rhi::Texture> image;
        ref<rhi::TextureView> image_view;
        GraphTextureRef graph_image;
    };

    std::vector<Draw> draws;
    std::vector<std::byte> uniforms(canvas.batches.size() * kUniformStride);
    CanvasSubmission result;
    draws.reserve(canvas.batches.size());
    for (u32 batch_index = 0; batch_index < canvas.batches.size(); ++batch_index) {
        const auto& batch = canvas.batches[batch_index];
        ref<rhi::Texture> image = impl_->atlas;
        ref<rhi::TextureView> image_view = impl_->atlas_view;
        TextureHandle logical;
        if (batch.primitive == CanvasPrimitive::Image
            || (batch.primitive == CanvasPrimitive::Glyph && !std::holds_alternative<std::monostate>(batch.image))) {
            auto resolved = resolve(batch.image);
            if (resolved && resolved->texture != nullptr && resolved->view != nullptr) {
                image = resolved->texture;
                image_view = resolved->view;
                logical = resolved->logical_texture;
                if (logical.IsValid() && std::ranges::find(result.textures, logical) == result.textures.end())
                    result.textures.push_back(logical);
            } else if (batch.primitive == CanvasPrimitive::Image) {
                image = impl_->diagnostic;
                image_view = impl_->diagnostic_view;
            }
        }
        f32 left = std::numeric_limits<f32>::max(), top = left, right = std::numeric_limits<f32>::lowest(),
            bottom = right;
        for (u32 index = 0; index < batch.index_count; ++index) {
            const auto& vertex = canvas.vertices[canvas.indices[batch.first_index + index]];
            left = std::min(left, vertex.x);
            top = std::min(top, vertex.y);
            right = std::max(right, vertex.x);
            bottom = std::max(bottom, vertex.y);
        }
        CanvasUniform value{{static_cast<f32>(canvas.width) / canvas.content_scale,
                                static_cast<f32>(canvas.height) / canvas.content_scale},
            static_cast<u32>(batch.primitive),
            batch.primitive == CanvasPrimitive::Image
                ? static_cast<u32>(canvas.vertices[canvas.indices[batch.first_index]].p0)
                : 0U,
            {left, top, right - left, bottom - top},
            {static_cast<f32>(image->GetWidth()) / canvas.content_scale,
                static_cast<f32>(image->GetHeight()) / canvas.content_scale},
            IsSrgbFormat(target.format) ? 0U : 1U};
        std::memcpy(uniforms.data() + batch_index * kUniformStride, &value, sizeof(value));
        ref<rhi::RenderPipeline> pipeline;
        TRY_ASSIGN(pipeline, impl_->PipelineFor(target.format, batch.blend));
        const std::array frame_entries{rhi::BindGroupEntryDesc{.binding = 0,
            .buffer = uniform_slice.buffer,
            .offset = uniform_slice.offset + batch_index * kUniformStride,
            .size = sizeof(CanvasUniform)}};
        ref<rhi::BindGroup> frame_group;
        TRY_ASSIGN(
            frame_group,
            impl_->device->CreateBindGroup(
                {.layout = impl_->layout.BindGroupLayouts()[0], .entries = frame_entries, .label = "Canvas frame/batch"}
            )
        );
        auto sampler = batch.sampler == CanvasSampler::NearestClamp ? impl_->nearest_sampler : impl_->linear_sampler;
        draws.push_back(
            {batch,
                std::move(pipeline),
                std::move(frame_group),
                std::move(sampler),
                std::move(image),
                std::move(image_view),
                {}}
        );
    }

    std::vector<std::byte> vertex_bytes(canvas.vertices.size() * sizeof(CanvasVertex));
    std::memcpy(vertex_bytes.data(), canvas.vertices.data(), vertex_bytes.size());
    std::vector<std::byte> index_bytes(canvas.indices.size() * sizeof(u32));
    std::memcpy(index_bytes.data(), canvas.indices.data(), index_bytes.size());
    TRY_VOID(impl_->uploads->EnqueueBatch(
        {
            BufferUploadRequest{.target = impl_->vertices->SharedBuffer(),
                .offset = vertex_slice.offset,
                .bytes = std::move(vertex_bytes),
                .publication = {}},
            BufferUploadRequest{.target = impl_->indices->SharedBuffer(),
                .offset = index_slice.offset,
                .bytes = std::move(index_bytes),
                .publication = {}},
            BufferUploadRequest{.target = impl_->uniforms->SharedBuffer(),
                .offset = uniform_slice.offset,
                .bytes = std::move(uniforms),
                .publication = {}},
        }
    ));
    auto uploads = impl_->uploads->PrepareAndSubmit();
    if (!uploads)
        return Err(std::move(uploads).error());

    RenderGraphBuilder graph;
    GraphTextureDesc output_desc;
    output_desc.label = "Canvas surface";
    output_desc.extent = GraphExtent::Fixed(target.width, target.height);
    output_desc.format = target.format;
    output_desc.usage = rhi::TextureUsage::RenderAttachment;
    ExternalTextureContract output_contract;
    output_contract.descriptor = output_desc;
    output_contract.initial_state = ExternalState::Present;
    output_contract.final_state = ExternalState::Present;
    output_contract.frame_bound = true;
    const auto output = graph.ImportTexture(std::move(output_contract));
    const GraphBufferDesc vertex_desc{.label = "Canvas vertices",
        .size = impl_->vertices->Buffer().GetSize(),
        .usage = impl_->vertices->Buffer().GetUsage()};
    const GraphBufferDesc index_desc{.label = "Canvas indices",
        .size = impl_->indices->Buffer().GetSize(),
        .usage = impl_->indices->Buffer().GetUsage()};
    const GraphBufferDesc uniform_desc{.label = "Canvas uniforms",
        .size = impl_->uniforms->Buffer().GetSize(),
        .usage = impl_->uniforms->Buffer().GetUsage()};
    const auto vertex_buffer = graph.ImportBuffer(
        {.descriptor = vertex_desc,
            .initial_state = ExternalState::ShaderRead,
            .final_state = ExternalState::ShaderRead},
        impl_->vertices->SharedBuffer()
    );
    const auto index_buffer = graph.ImportBuffer(
        {.descriptor = index_desc,
            .initial_state = ExternalState::ShaderRead,
            .final_state = ExternalState::ShaderRead},
        impl_->indices->SharedBuffer()
    );
    const auto uniform_buffer = graph.ImportBuffer(
        {.descriptor = uniform_desc,
            .initial_state = ExternalState::ShaderRead,
            .final_state = ExternalState::ShaderRead},
        impl_->uniforms->SharedBuffer()
    );
    auto pass = graph.AddPass("Canvas composite", PassKind::Render);
    pass.Read(graph.Initial(vertex_buffer), GraphAccess::Vertex)
        .Read(graph.Initial(index_buffer), GraphAccess::Index)
        .Read(graph.Initial(uniform_buffer), GraphAccess::Uniform);
    for (auto& draw : draws) {
        GraphTextureDesc descriptor;
        descriptor.label = "Canvas sampled image";
        descriptor.extent = GraphExtent::Fixed(draw.image->GetWidth(), draw.image->GetHeight());
        descriptor.depth_or_layers = draw.image->GetDepthOrArrayLayers();
        descriptor.mip_levels = draw.image->GetMipLevelCount();
        descriptor.sample_count = draw.image->GetSampleCount();
        descriptor.dimension = draw.image->GetDimension();
        descriptor.format = draw.image->GetFormat();
        descriptor.usage = draw.image->GetUsage();
        ExternalTextureContract image_contract;
        image_contract.descriptor = descriptor;
        image_contract.initial_state = ExternalState::ShaderRead;
        image_contract.final_state = ExternalState::ShaderRead;
        const auto image = graph.ImportTexture(std::move(image_contract), draw.image);
        draw.graph_image = graph.Initial(image);
        pass.Read(draw.graph_image, GraphAccess::Sampled);
    }
    ColorAttachment attachment;
    attachment.load = rhi::LoadOp::Clear;
    attachment.store = rhi::StoreOp::Store;
    attachment.clear = {0.035, 0.04, 0.055, 1.0};
    const auto written = pass.Color(output, std::move(attachment));
    const f32 scale = canvas.content_scale;
    pass.Execute(
        [draws = std::move(draws),
            vertex = impl_->vertices->SharedBuffer(),
            index = impl_->indices->SharedBuffer(),
            vertex_slice,
            index_slice,
            scale,
            target,
            image_layout = impl_->layout.BindGroupLayouts()[1]](RenderGraphContext& context) -> Result<void> {
            auto* encoder = context.RenderEncoder();
            if (encoder == nullptr)
                return Err(ErrorCode::ValidationInvalidState, "canvas requires a render encoder");
            encoder->SetVertexBuffer(0, vertex.get(), vertex_slice.offset, vertex_slice.size);
            encoder->SetIndexBuffer(*index, rhi::IndexFormat::Uint32, index_slice.offset, index_slice.size);
            for (const auto& draw : draws) {
                const f32 left = std::clamp(draw.batch.scissor.x * scale, 0.0f, static_cast<f32>(target.width));
                const f32 top = std::clamp(draw.batch.scissor.y * scale, 0.0f, static_cast<f32>(target.height));
                const f32 right = std::clamp(
                    (draw.batch.scissor.x + draw.batch.scissor.width) * scale,
                    left,
                    static_cast<f32>(target.width)
                );
                const f32 bottom = std::clamp(
                    (draw.batch.scissor.y + draw.batch.scissor.height) * scale,
                    top,
                    static_cast<f32>(target.height)
                );
                if (right <= left || bottom <= top)
                    continue;
                encoder->SetPipeline(*draw.pipeline);
                encoder->SetBindGroup(0, draw.frame_group.get());
                std::reference_wrapper<rhi::TextureView> image_view = std::ref(*draw.image_view);
                TRY_ASSIGN(image_view, context.TextureView(draw.graph_image));
                const std::array image_entries{rhi::BindGroupEntryDesc{.binding = 0, .texture_view = &image_view.get()},
                    rhi::BindGroupEntryDesc{.binding = 1, .sampler = draw.sampler.get()}};
                ref<rhi::BindGroup> image_group;
                TRY_ASSIGN(
                    image_group,
                    context.Device()
                        .CreateBindGroup({.layout = image_layout, .entries = image_entries, .label = "Canvas image"})
                );
                encoder->SetBindGroup(1, image_group.get());
                encoder->SetScissorRect(
                    static_cast<u32>(left),
                    static_cast<u32>(top),
                    static_cast<u32>(std::ceil(right - left)),
                    static_cast<u32>(std::ceil(bottom - top))
                );
                encoder->DrawIndexed(draw.batch.index_count, 1, draw.batch.first_index);
            }
            return Ok();
        }
    );
    TRY_VOID(graph.Export(written, ExternalState::Present));
    CompiledRenderGraph compiled;
    TRY_ASSIGN(compiled, graph.Compile(target.width, target.height));
    GraphExecutor executor(impl_->device, std::move(compiled), impl_->releases);
    auto begun = executor.Begin(target.width, target.height);
    if (!begun)
        return Err(std::move(begun).error());
    GraphFrame graph_frame = std::move(*begun);
    GraphTextureBinding output_binding;
    output_binding.resource = output;
    output_binding.texture = target.texture;
    output_binding.view = target.view;
    output_binding.signature = output_desc;
    TRY_VOID(graph_frame.Bind(std::move(output_binding)));
    TRY_ASSIGN(result.submission, graph_frame.Execute());
    impl_->vertices->MarkUsed(result.submission);
    impl_->indices->MarkUsed(result.submission);
    impl_->uniforms->MarkUsed(result.submission);
    impl_->layouts.MarkUsed(impl_->layout, result.submission);
    impl_->shaders.MarkUsed(impl_->shader, result.submission);
    static_cast<void>(impl_->vertices->Free(vertex_slice.allocation, result.submission));
    static_cast<void>(impl_->indices->Free(index_slice.allocation, result.submission));
    static_cast<void>(impl_->uniforms->Free(uniform_slice.allocation, result.submission));
    return Ok(std::move(result));
}

void CanvasFeature::MarkDeviceLost() noexcept {
    impl_->lost = true;
    impl_->vertices->MarkResidencyLost();
    impl_->indices->MarkResidencyLost();
    impl_->uniforms->MarkResidencyLost();
}

} // namespace woki::gfx
