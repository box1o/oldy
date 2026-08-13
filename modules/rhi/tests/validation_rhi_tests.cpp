#include <algorithm>
#include <array>
#include <mutex>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "rhi_test_fixture.hpp"

using namespace woki;
using namespace woki::rhi;
using namespace woki::rhi::test;

namespace {

struct ValidationContext final {
    NullContext null;
    std::mutex mutex;
    std::vector<RhiDiagnostic> diagnostics;
    ref<Device> device;

    ValidationContext() {
        ValidationRhiDescriptor descriptor;
        descriptor.full = true;
        descriptor.diagnostic = [&](const RhiDiagnostic& diagnostic) {
            std::lock_guard lock(mutex);
            diagnostics.push_back(diagnostic);
        };
        device = CreateValidationDevice(null.device, std::move(descriptor));
        REQUIRE(device);
    }

    [[nodiscard]] bool Has(RhiDiagnosticCode code) const {
        return std::ranges::any_of(diagnostics, [code](const RhiDiagnostic& value) { return value.code == code; });
    }
};

} // namespace

TEST_CASE("ValidationRHI forwards capabilities, limits, and loss state") {
    ValidationContext context;
    CHECK(&context.device->Capabilities() == &context.null.device->Capabilities());
    CHECK(context.device->GetLimits().max_buffer_size == context.null.device->GetLimits().max_buffer_size);
    CHECK(context.device->HasFeature(FeatureName::TimestampQuery));
    CHECK(context.device->GetAdapterInfo().backend_type == BackendType::Null);
    CHECK_FALSE(context.device->IsLost());

    context.device->ForceLoss(DeviceLostReason::Destroyed, "validation loss");
    CHECK(context.device->IsLost());
    CHECK(context.device->LossReason() == DeviceLostReason::Destroyed);
    CHECK(context.device->GetLostFuture().completed);
}

TEST_CASE("ValidationRHI diagnoses invalid resource and pipeline descriptors") {
    ValidationContext context;
    auto zero_buffer = context.device->CreateBuffer({.size = 0, .usage = BufferUsage::CopyDst});
    REQUIRE_FALSE(zero_buffer);
    CHECK(zero_buffer.error().Code() == ErrorCode::ValidationInvalidState);
    CHECK(context.Has(RhiDiagnosticCode::DescriptorLimit));

    auto invalid_texture = context.device->CreateTexture({
        .size = {0, 1, 1},
        .format = TextureFormat::RGBA8Unorm,
        .usage = TextureUsage::TextureBinding,
    });
    REQUIRE_FALSE(invalid_texture);
    CHECK(context.Has(RhiDiagnosticCode::TextureDescriptor));

    auto shader = CreateShader(*context.device);
    const VertexStateDesc vertex{.module = shader.get(), .entry_point = "vs"};
    const ColorTargetStateDesc target{.format = TextureFormat::R8Unorm};
    const FragmentStateDesc fragment{.module = shader.get(), .entry_point = "fs", .targets = std::span(&target, 1)};
    auto invalid_pipeline = context.device->CreateRenderPipeline({
        .vertex = &vertex,
        .multisample = {.count = 4},
        .fragment = &fragment,
    });
    REQUIRE_FALSE(invalid_pipeline);
    CHECK(context.Has(RhiDiagnosticCode::PipelineTarget));
}

TEST_CASE("ValidationRHI reports retired buffer use with command breadcrumbs") {
    ValidationContext context;
    auto buffer_result = context.device->CreateBuffer({
        .size = 16,
        .usage = BufferUsage::MapRead | BufferUsage::CopyDst,
        .mapped_at_creation = true,
    });
    REQUIRE(buffer_result);
    auto buffer = std::move(*buffer_result);

    auto encoder_result = context.device->CreateCommandEncoder();
    REQUIRE(encoder_result);
    auto encoder = std::move(*encoder_result);
    auto pass_result = encoder->BeginComputePass();
    REQUIRE(pass_result);
    auto pass = std::move(*pass_result);
    pass->DispatchWorkgroups(1);
    pass->End();
    auto commands_result = encoder->Finish();
    REQUIRE(commands_result);

    buffer->Destroy();
    CHECK(buffer->GetMappedRange(0, 4) == nullptr);
    CHECK(context.Has(RhiDiagnosticCode::UseAfterRetire));
    REQUIRE_FALSE(context.diagnostics.empty());
    const auto& diagnostic = context.diagnostics.back();
    CHECK(diagnostic.message.find("RHI-VAL-005") != std::string::npos);
    CHECK(std::ranges::find(diagnostic.breadcrumbs, "compute.begin") != diagnostic.breadcrumbs.end());
    CHECK(std::ranges::find(diagnostic.breadcrumbs, "compute.dispatch") != diagnostic.breadcrumbs.end());
    CHECK(std::ranges::find(diagnostic.breadcrumbs, "encoder.finish") != diagnostic.breadcrumbs.end());
}

TEST_CASE("ValidationRHI diagnoses pass state and encoder scope misuse") {
    ValidationContext context;
    auto target_result = context.device->CreateTexture({.size = {1, 1, 1}, .format = TextureFormat::RGBA8Unorm, .usage = TextureUsage::RenderAttachment});
    REQUIRE(target_result);
    auto target = std::move(*target_result);
    auto view = target->CreateView();
    auto encoder_result = context.device->CreateCommandEncoder();
    REQUIRE(encoder_result);
    auto encoder = std::move(*encoder_result);
    const RenderPassColorAttachmentDesc color{.view = view.get()};
    auto pass_result = encoder->BeginRenderPass(RenderPassDescTyped{.color_attachments = std::span(&color, 1)});
    REQUIRE(pass_result);
    auto pass = std::move(*pass_result);

    pass->Draw(3);
    CHECK(context.Has(RhiDiagnosticCode::PassState));
    REQUIRE_FALSE(context.diagnostics.empty());
    CHECK(std::ranges::find(context.diagnostics.back().breadcrumbs, "render.begin") != context.diagnostics.back().breadcrumbs.end());
    pass->End();
    pass->End();
    CHECK(context.Has(RhiDiagnosticCode::EncoderScope));

    auto commands_result = encoder->Finish();
    REQUIRE(commands_result);
    auto second_finish = encoder->Finish();
    REQUIRE_FALSE(second_finish);
    CHECK(second_finish.error().Code() == ErrorCode::ValidationInvalidState);
}

TEST_CASE("ValidationRHI reports a pass destroyed before End") {
    ValidationContext context;
    auto encoder_result = context.device->CreateCommandEncoder();
    REQUIRE(encoder_result);
    auto encoder = std::move(*encoder_result);
    {
        auto pass_result = encoder->BeginComputePass();
        REQUIRE(pass_result);
        auto pass = std::move(*pass_result);
        pass->InsertDebugMarker("abandoned pass");
    }
    CHECK(context.Has(RhiDiagnosticCode::EncoderScope));
    REQUIRE(encoder->Finish());
}

TEST_CASE("ValidationRHI accepts aligned dynamic offsets in render and compute passes") {
    ValidationContext context;
    auto uniform_result = context.device->CreateBuffer({.size = 512, .usage = BufferUsage::Uniform | BufferUsage::CopyDst});
    REQUIRE(uniform_result);
    auto uniform = std::move(*uniform_result);
    const BindGroupLayoutEntryDesc layout_entry{
        .binding = 0,
        .visibility = static_cast<u32>(ShaderStage::Vertex | ShaderStage::Compute),
        .buffer = {.type = BufferBindingType::Uniform, .has_dynamic_offset = true, .min_binding_size = 16},
    };
    auto layout_result = context.device->CreateBindGroupLayout({.entries = std::span(&layout_entry, 1)});
    REQUIRE(layout_result);
    auto layout = std::move(*layout_result);
    const BindGroupEntryDesc entry{.binding = 0, .buffer = uniform.get(), .size = 16};
    auto group_result = context.device->CreateBindGroup({.layout = layout.get(), .entries = std::span(&entry, 1)});
    REQUIRE(group_result);
    auto group = std::move(*group_result);
    const u32 dynamic_offset = context.device->GetLimits().min_uniform_buffer_offset_alignment;

    auto encoder_result = context.device->CreateCommandEncoder();
    REQUIRE(encoder_result);
    auto encoder = std::move(*encoder_result);
    auto compute_result = encoder->BeginComputePass();
    REQUIRE(compute_result);
    auto compute = std::move(*compute_result);
    compute->SetBindGroup(0, group.get(), std::span(&dynamic_offset, 1));
    compute->End();

    auto target_result = context.device->CreateTexture({.size = {1, 1, 1}, .format = TextureFormat::RGBA8Unorm, .usage = TextureUsage::RenderAttachment});
    REQUIRE(target_result);
    auto target = std::move(*target_result);
    auto view = target->CreateView();
    const RenderPassColorAttachmentDesc color{.view = view.get()};
    auto render_result = encoder->BeginRenderPass(RenderPassDescTyped{.color_attachments = std::span(&color, 1)});
    REQUIRE(render_result);
    auto render = std::move(*render_result);
    render->SetBindGroup(0, group.get(), std::span(&dynamic_offset, 1));
    render->End();
    REQUIRE(encoder->Finish());
    CHECK_FALSE(context.Has(RhiDiagnosticCode::DynamicOffset));
}

TEST_CASE("ValidationRHI unwraps copy resources and rejects foreign command buffers") {
    ValidationContext first;
    auto source_result = first.device->CreateBuffer({.size = 8, .usage = BufferUsage::CopySrc | BufferUsage::CopyDst});
    auto destination_result = first.device->CreateBuffer({.size = 8, .usage = BufferUsage::CopyDst | BufferUsage::MapRead});
    REQUIRE(source_result);
    REQUIRE(destination_result);
    auto source = std::move(*source_result);
    auto destination = std::move(*destination_result);
    const std::array<u32, 2> values{42, 84};
    REQUIRE(first.device->GetQueue().WriteBuffer(*source, 0, values.data(), sizeof(values)));
    auto encoder_result = first.device->CreateCommandEncoder();
    REQUIRE(encoder_result);
    auto encoder = std::move(*encoder_result);
    REQUIRE(encoder->CopyBufferToBuffer(*source, 0, *destination, 0, sizeof(values)));
    auto commands_result = encoder->Finish();
    REQUIRE(commands_result);
    auto commands = std::move(*commands_result);

    auto foreign_encoder = first.null.device->CreateCommandEncoder();
    REQUIRE(foreign_encoder);
    auto foreign_commands = (*foreign_encoder)->Finish();
    REQUIRE(foreign_commands);
    CommandBuffer* foreign[] = {foreign_commands->get()};
    auto rejected = first.device->GetQueue().Submit(foreign);
    REQUIRE_FALSE(rejected);
    CHECK(first.Has(RhiDiagnosticCode::DeviceOwnership));

    CommandBuffer* owned[] = {commands.get()};
    auto ticket = first.device->GetQueue().Submit(owned);
    REQUIRE(ticket);
    auto duplicate = first.device->GetQueue().Submit(owned);
    REQUIRE_FALSE(duplicate);
    CHECK(duplicate.error().Code() == ErrorCode::ValidationInvalidState);
    CHECK(first.device->GetQueue().CompletedSubmission().HasReached(*ticket));

    bool mapped = false;
    const auto map_future = destination->MapAsync(MapMode::Read, 0, sizeof(values), CallbackMode::AllowProcessEvents, [&](MapAsyncStatus status, std::string_view) { mapped = status == MapAsyncStatus::Success; });
    CHECK(map_future.success);
    REQUIRE(mapped);
    std::array<u32, 2> copied{};
    REQUIRE(destination->ReadMappedRange(0, copied.data(), sizeof(copied)));
    CHECK(copied == values);
}

TEST_CASE("ValidationRHI enforces owning-thread submission") {
    ValidationContext context;
    auto encoder_result = context.device->CreateCommandEncoder();
    REQUIRE(encoder_result);
    auto commands_result = (*encoder_result)->Finish();
    REQUIRE(commands_result);
    auto commands = std::move(*commands_result);
    CommandBuffer* submitted[] = {commands.get()};
    bool rejected = false;

    std::thread worker([&] { rejected = !context.device->GetQueue().Submit(submitted); });
    worker.join();

    CHECK(rejected);
    CHECK(context.Has(RhiDiagnosticCode::ThreadOwnership));
}
