#include <array>
#include <atomic>
#include <cstring>

#include <woki/rhi/conformance.hpp>
#include <woki/rhi/command_encoder.hpp>
#include <woki/rhi/compute_pass_encoder.hpp>
#include <woki/rhi/objects.hpp>
#include <woki/rhi/render_pass_encoder.hpp>

namespace woki::rhi {

Result<ConformanceReport> RunConformance(Device& device, const std::function<bool()>& pump) {
    ConformanceReport report;
    const std::array<u32, 4> source_data{0x10203040U, 2, 3, 4};
    scope<Buffer> source;
    scope<Buffer> readback;
    TRY_ASSIGN(
        source,
        device.CreateBuffer(
            {.size = sizeof(source_data),
                .usage = BufferUsage::CopySrc | BufferUsage::CopyDst,
                .label = "RHI conformance source"}
        )
    );
    TRY_ASSIGN(
        readback,
        device.CreateBuffer(
            {.size = sizeof(source_data),
                .usage = BufferUsage::CopyDst | BufferUsage::MapRead,
                .label = "RHI conformance readback"}
        )
    );
    TRY_VOID(device.GetQueue().WriteBuffer(*source, 0, source_data.data(), sizeof(source_data)));

    scope<Texture> texture_a;
    scope<Texture> texture_b;
    const TextureDesc texture_desc{.size = {4, 4, 1},
        .format = TextureFormat::RGBA8Unorm,
        .usage = TextureUsage::CopySrc | TextureUsage::CopyDst | TextureUsage::RenderAttachment,
        .label = "RHI conformance texture"};
    TRY_ASSIGN(texture_a, device.CreateTexture(texture_desc));
    TRY_ASSIGN(texture_b, device.CreateTexture(texture_desc));
    std::array<u8, 64> texels{};
    texels[0] = 0x7f;
    TRY_VOID(device.GetQueue().WriteTexture(
        {.texture = texture_a.get()},
        texels.data(),
        texels.size(),
        {.bytes_per_row = 16, .rows_per_image = 4},
        {4, 4, 1}
    ));

    constexpr std::string_view shader_source = R"(
        @vertex fn vs(@builtin(vertex_index) i:u32)->@builtin(position) vec4f {
            var p=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3)); return vec4f(p[i],0,1);
        }
        @fragment fn fs()->@location(0) vec4f { return vec4f(0,0,0,1); }
        @compute @workgroup_size(1) fn cs() {}
    )";
    scope<ShaderModule> shader;
    TRY_ASSIGN(
        shader,
        device.CreateShaderModule({.code = std::string(shader_source), .label = "RHI conformance shader"})
    );
    const VertexStateDesc vertex{.module = shader.get(), .entry_point = "vs"};
    const ColorTargetStateDesc target{.format = TextureFormat::RGBA8Unorm};
    const FragmentStateDesc fragment{.module = shader.get(), .entry_point = "fs", .targets = std::span(&target, 1)};
    scope<RenderPipeline> render_pipeline;
    TRY_ASSIGN(
        render_pipeline,
        device.CreateRenderPipeline(
            RenderPipelineDescTyped{.vertex = &vertex,
                .fragment = &fragment,
                .label = "RHI conformance render pipeline"}
        )
    );
    scope<ComputePipeline> compute_pipeline;
    TRY_ASSIGN(
        compute_pipeline,
        device.CreateComputePipeline(
            {.compute = {.module = shader.get(), .entry_point = "cs"}, .label = "RHI conformance compute pipeline"}
        )
    );

    scope<CommandEncoder> encoder;
    TRY_ASSIGN(encoder, device.CreateCommandEncoder({.label = "RHI conformance encoder"}));
    TRY_VOID(encoder->CopyBufferToBuffer(*source, 0, *readback, 0, sizeof(source_data)));
    TRY_VOID(encoder->CopyTextureToTexture({.texture = texture_a.get()}, {.texture = texture_b.get()}, {4, 4, 1}));
    auto view = texture_b->CreateView();
    const RenderPassColorAttachmentDesc attachment{.view = view.get(),
        .load_op = LoadOp::Clear,
        .store_op = StoreOp::Store};
    scope<RenderPassEncoder> render_pass;
    TRY_ASSIGN(
        render_pass,
        encoder->BeginRenderPass(RenderPassDescTyped{.color_attachments = std::span(&attachment, 1)})
    );
    render_pass->SetPipeline(*render_pipeline);
    render_pass->Draw(3);
    render_pass->End();
    scope<ComputePassEncoder> compute_pass;
    TRY_ASSIGN(compute_pass, encoder->BeginComputePass());
    compute_pass->SetPipeline(*compute_pipeline);
    compute_pass->DispatchWorkgroups(1);
    compute_pass->End();
    scope<CommandBuffer> commands;
    TRY_ASSIGN(commands, encoder->Finish());
    CommandBuffer* submitted[] = {commands.get()};
    TRY_ASSIGN(report.submission, device.GetQueue().Submit(submitted));
    report.buffer_copy_write_map = true;
    report.texture_upload_copy = true;
    report.render_commands = true;
    report.compute_commands = true;

    for (u32 attempts = 0; !device.GetQueue().CompletedSubmission().HasReached(report.submission) && attempts != 10000;
        ++attempts) {
        device.Tick();
        if (pump && !pump())
            return Err(ErrorCode::Cancelled, "RHI conformance callback pump cancelled");
    }
    report.completed = device.GetQueue().CompletedSubmission();
    if (!report.completed.HasReached(report.submission))
        return Err(ErrorCode::InvalidState, "RHI conformance submission watermark did not complete");

    std::atomic_bool mapped{};
    std::atomic_bool map_success{};
    static_cast<void>(readback->MapAsync(
        MapMode::Read,
        0,
        sizeof(source_data),
        CallbackMode::AllowProcessEvents,
        [&](MapAsyncStatus status, std::string_view) {
            map_success.store(status == MapAsyncStatus::Success);
            mapped.store(true);
        }
    ));
    for (u32 attempts = 0; !mapped.load() && attempts != 10000; ++attempts) {
        device.Tick();
        if (pump && !pump())
            return Err(ErrorCode::Cancelled, "RHI conformance callback pump cancelled");
    }
    std::array<u32, 4> copied{};
    if (!mapped.load() || !map_success.load())
        return Err(ErrorCode::InvalidState, "RHI conformance readback map did not complete");
    TRY_VOID(readback->ReadMappedRange(0, copied.data(), sizeof(copied)));
    readback->Unmap();
    report.readback = copied == source_data;
    if (!report.readback)
        return Err(ErrorCode::ValidationInvalidState, "RHI conformance readback did not match source data");
    return Ok(std::move(report));
}

} // namespace woki::rhi
