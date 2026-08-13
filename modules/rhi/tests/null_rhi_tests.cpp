#include <array>
#include <atomic>

#include <catch2/catch_test_macros.hpp>

#include <woki/rhi/advanced.hpp>

#include "rhi_test_fixture.hpp"

using namespace woki;
using namespace woki::rhi;
using namespace woki::rhi::test;

TEST_CASE("NullRHI exposes normalized deterministic capabilities") {
    NullContext context;
    const auto info = context.device->GetAdapterInfo();
    const auto features = context.device->GetFeatures();
    const auto& capabilities = context.device->Capabilities();

    CHECK(info.backend_type == BackendType::Null);
    CHECK(info.adapter_type == AdapterType::CPU);
    CHECK(info.feature_level == FeatureLevel::Core);
    CHECK(features.Has(FeatureName::TimestampQuery));
    CHECK(context.device->HasFeature(FeatureName::MultiDrawIndirect));
    CHECK(capabilities.Has(CapabilityFeature::Compute));
    CHECK(capabilities.Has(CapabilityFeature::IndirectDraw));
    CHECK(capabilities.Has(CapabilityFeature::IndirectCount));
    CHECK(capabilities.Has(CapabilityFeature::StorageBuffers));
    CHECK(capabilities.Has(CapabilityFeature::StorageTextures));
    CHECK(capabilities.Has(CapabilityFeature::Readback));
    CHECK(capabilities.Has(CapabilityFeature::Presentation));
    CHECK(capabilities.Has(CapabilityFeature::HdrPresentation));
    CHECK(capabilities.Supports(TextureFormat::RGBA8Unorm, TextureUsage::CopyDst | TextureUsage::RenderAttachment, 4));
    CHECK_FALSE(capabilities.Supports(TextureFormat::R8Unorm, TextureUsage::RenderAttachment, 4));
    REQUIRE(capabilities.Queues().size() == 4);
    CHECK(capabilities.Queues()[0] == QueueClass::Graphics);
    CHECK(context.device->GetLimits().max_buffer_size == capabilities.GetLimits().max_buffer_size);

    const auto device_handles = advanced::Native(*context.device);
    const auto queue_handles = advanced::Native(context.device->GetQueue());
    CHECK(device_handles.device == nullptr);
    CHECK(queue_handles.queue == nullptr);
}

TEST_CASE("NullRHI preserves buffer descriptors and mapped writes") {
    NullContext context;
    auto created = context.device->CreateBuffer({
        .size = 32,
        .usage = BufferUsage::MapRead | BufferUsage::MapWrite | BufferUsage::CopySrc | BufferUsage::CopyDst,
        .mapped_at_creation = true,
        .label = "mapped",
    });
    REQUIRE(created);
    auto buffer = std::move(*created);

    CHECK(buffer->GetSize() == 32);
    CHECK(HasFlag(buffer->GetUsage(), BufferUsage::MapRead));
    CHECK(HasFlag(buffer->GetUsage(), BufferUsage::CopyDst));
    CHECK(buffer->GetMapState() == BufferMapState::Mapped);

    const std::array<u32, 2> values{0x12345678, 0xabcdef01};
    REQUIRE(buffer->WriteMappedRange(8, values.data(), sizeof(values)));
    std::array<u32, 2> read{};
    REQUIRE(buffer->ReadMappedRange(8, read.data(), sizeof(read)));
    CHECK(read == values);
    REQUIRE(buffer->GetMappedRange(8, sizeof(values)) != nullptr);
    REQUIRE(buffer->GetConstMappedRange(8, sizeof(values)) != nullptr);

    buffer->Unmap();
    CHECK(buffer->GetMapState() == BufferMapState::Unmapped);
    bool callback_called = false;
    const auto mapped = buffer->MapAsync(MapMode::Read, 8, sizeof(values), CallbackMode::AllowProcessEvents, [&](MapAsyncStatus status, std::string_view message) {
        callback_called = true;
        CHECK(status == MapAsyncStatus::Success);
        CHECK(message.empty());
    });
    CHECK(mapped.completed);
    CHECK(mapped.success);
    CHECK(callback_called);
    buffer->Unmap();

    CHECK_FALSE(context.device->CreateBuffer({.size = 4, .usage = BufferUsage::None}));
    CHECK_FALSE(context.device->CreateBuffer({.size = context.device->GetLimits().max_buffer_size + 1, .usage = BufferUsage::CopyDst}));
}

TEST_CASE("NullRHI executes queue writes, encoder writes, copies, clears, and readback") {
    NullContext context;
    auto source_result = context.device->CreateBuffer({.size = 32, .usage = BufferUsage::CopySrc | BufferUsage::CopyDst});
    auto destination_result = context.device->CreateBuffer({.size = 32, .usage = BufferUsage::CopySrc | BufferUsage::CopyDst | BufferUsage::MapRead});
    REQUIRE(source_result);
    REQUIRE(destination_result);
    auto source = std::move(*source_result);
    auto destination = std::move(*destination_result);

    const std::array<u32, 4> first{1, 2, 3, 4};
    const std::array<u32, 2> replacement{9, 10};
    REQUIRE(context.device->GetQueue().WriteBuffer(*source, 0, first.data(), sizeof(first)));

    auto encoder_result = context.device->CreateCommandEncoder();
    REQUIRE(encoder_result);
    auto encoder = std::move(*encoder_result);
    REQUIRE(encoder->WriteBuffer(*source, sizeof(u32) * 2, reinterpret_cast<const u8*>(replacement.data()), sizeof(replacement)));
    REQUIRE(encoder->CopyBufferToBuffer(*source, 0, *destination, 0, sizeof(first)));
    REQUIRE(encoder->ClearBuffer(*destination, sizeof(first), 16));
    auto commands_result = encoder->Finish();
    REQUIRE(commands_result);
    auto commands = std::move(*commands_result);
    CommandBuffer* submitted[] = {commands.get()};
    auto ticket = context.device->GetQueue().Submit(submitted);
    REQUIRE(ticket);
    CHECK(ticket->IsValid());
    CHECK(context.device->GetQueue().CompletedSubmission().HasReached(*ticket));

    bool mapped = false;
    const auto map_future = destination->MapAsync(MapMode::Read, 0, 32, CallbackMode::AllowProcessEvents, [&](MapAsyncStatus status, std::string_view) { mapped = status == MapAsyncStatus::Success; });
    CHECK(map_future.success);
    REQUIRE(mapped);
    std::array<u32, 8> read{};
    REQUIRE(destination->ReadMappedRange(0, read.data(), sizeof(read)));
    CHECK(read[0] == 1);
    CHECK(read[1] == 2);
    CHECK(read[2] == 9);
    CHECK(read[3] == 10);
    CHECK(read[4] == 0);
    CHECK(read[7] == 0);
    destination->Unmap();
    CHECK(LogContains(*context.device, "queue.write_buffer"));
    CHECK(LogContains(*context.device, "queue.submit 1"));
}

TEST_CASE("NullRHI uploads and copies texture regions to readback buffers") {
    NullContext context;
    const TextureDesc desc{
        .size = {2, 2, 1},
        .mip_level_count = 1,
        .sample_count = 1,
        .dimension = TextureDimension::e2D,
        .format = TextureFormat::RGBA8Unorm,
        .usage = TextureUsage::CopySrc | TextureUsage::CopyDst | TextureUsage::TextureBinding,
        .view_formats = {TextureFormat::RGBA8UnormSrgb},
        .label = "texture",
    };
    auto first_result = context.device->CreateTexture(desc);
    auto second_result = context.device->CreateTexture(desc);
    auto upload_result = context.device->CreateBuffer({.size = 16, .usage = BufferUsage::CopySrc | BufferUsage::CopyDst});
    auto readback_result = context.device->CreateBuffer({.size = 16, .usage = BufferUsage::CopyDst | BufferUsage::MapRead});
    REQUIRE(first_result);
    REQUIRE(second_result);
    REQUIRE(upload_result);
    REQUIRE(readback_result);
    auto first = std::move(*first_result);
    auto second = std::move(*second_result);
    auto upload = std::move(*upload_result);
    auto readback = std::move(*readback_result);

    CHECK(first->GetWidth() == 2);
    CHECK(first->GetHeight() == 2);
    CHECK(first->GetDepthOrArrayLayers() == 1);
    CHECK(first->GetMipLevelCount() == 1);
    CHECK(first->GetSampleCount() == 1);
    CHECK(first->GetDimension() == TextureDimension::e2D);
    CHECK(first->GetTextureBindingViewDimension() == TextureViewDimension::e2D);
    CHECK(first->GetFormat() == TextureFormat::RGBA8Unorm);
    CHECK(HasFlag(first->GetUsage(), TextureUsage::TextureBinding));
    REQUIRE(first->CreateView());
    REQUIRE(first->CreateErrorView());
    first->Pin(TextureUsage::TextureBinding);
    first->Unpin();
    first->SetOwnershipForMemoryDump(42);

    const std::array<u8, 16> texels{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    REQUIRE(context.device->GetQueue().WriteTexture({.texture = first.get()}, texels.data(), texels.size(), {.bytes_per_row = 8, .rows_per_image = 2}, {2, 2, 1}));
    REQUIRE(context.device->GetQueue().WriteBuffer(*upload, 0, texels.data(), texels.size()));
    auto encoder_result = context.device->CreateCommandEncoder();
    REQUIRE(encoder_result);
    auto encoder = std::move(*encoder_result);
    REQUIRE(encoder->CopyBufferToTexture({{.bytes_per_row = 8, .rows_per_image = 2}, upload.get()}, {.texture = first.get()}, {2, 2, 1}));
    REQUIRE(encoder->CopyTextureToTexture({.texture = first.get()}, {.texture = second.get()}, {2, 2, 1}));
    REQUIRE(encoder->CopyTextureToBuffer({.texture = second.get()}, {{.bytes_per_row = 8, .rows_per_image = 2}, readback.get()}, {2, 2, 1}));
    auto commands_result = encoder->Finish();
    REQUIRE(commands_result);
    auto commands = std::move(*commands_result);
    CommandBuffer* submitted[] = {commands.get()};
    REQUIRE(context.device->GetQueue().Submit(submitted));

    bool mapped = false;
    const auto map_future = readback->MapAsync(MapMode::Read, 0, texels.size(), CallbackMode::AllowProcessEvents, [&](MapAsyncStatus status, std::string_view) { mapped = status == MapAsyncStatus::Success; });
    CHECK(map_future.success);
    REQUIRE(mapped);
    std::array<u8, 16> copied{};
    REQUIRE(readback->ReadMappedRange(0, copied.data(), copied.size()));
    CHECK(copied == texels);
    CHECK(LogContains(*context.device, "queue.write_texture"));

    auto invalid = desc;
    invalid.size.width = 0;
    CHECK_FALSE(context.device->CreateTexture(invalid));
}

TEST_CASE("NullRHI creates auxiliary resource descriptors and resolves queries") {
    NullContext context;
    auto buffer_result = context.device->CreateBuffer({.size = 32, .usage = BufferUsage::CopyDst | BufferUsage::QueryResolve});
    REQUIRE(buffer_result);
    auto buffer = std::move(*buffer_result);

    auto sampler = context.device->CreateSampler({
        .address_mode_u = AddressMode::Repeat,
        .mag_filter = FilterMode::Linear,
        .min_filter = FilterMode::Linear,
        .max_anisotropy = 4,
        .label = "filtered sampler",
    });
    REQUIRE(sampler);
    auto queries_result = context.device->CreateQuerySet({.type = QueryType::Timestamp, .count = 2, .label = "timestamps"});
    REQUIRE(queries_result);
    auto queries = std::move(*queries_result);
    CHECK(queries->GetType() == QueryType::Timestamp);
    CHECK(queries->GetCount() == 2);
    CHECK_FALSE(context.device->CreateQuerySet({.type = QueryType::Timestamp, .count = 0}));

    auto table_result = context.device->CreateResourceTable({.size = 2, .label = "resources"});
    REQUIRE(table_result);
    auto table = std::move(*table_result);
    CHECK(table->GetSize() == 2);
    REQUIRE(table->Update(1, {.buffer = buffer.get(), .offset = 8, .size = 8}));
    REQUIRE(table->RemoveBinding(1));
    CHECK_FALSE(table->Update(3, {.buffer = buffer.get()}));

    auto shader = CreateShader(*context.device);
    bool compilation_callback = false;
    const auto compilation = shader->GetCompilationInfo(CallbackMode::AllowProcessEvents, [&](CompilationInfoRequestStatus status, const CompilationInfo* info, std::string_view message) {
        compilation_callback = true;
        CHECK(status == CompilationInfoRequestStatus::Success);
        CHECK(info == nullptr);
        CHECK(message.empty());
    });
    CHECK(compilation.completed);
    CHECK(compilation.success);
    CHECK(compilation_callback);

    auto encoder_result = context.device->CreateCommandEncoder();
    REQUIRE(encoder_result);
    auto encoder = std::move(*encoder_result);
    REQUIRE(encoder->WriteTimestamp(*queries, 0));
    REQUIRE(encoder->ResolveQuerySet(*queries, 0, 2, *buffer, 0));
    auto commands_result = encoder->Finish();
    REQUIRE(commands_result);
    auto commands = std::move(*commands_result);
    CommandBuffer* submitted[] = {commands.get()};
    REQUIRE(context.device->GetQueue().Submit(submitted));

    auto bundle_encoder = context.device->CreateRenderBundleEncoder({
        .color_formats = {TextureFormat::RGBA8Unorm},
        .sample_count = 1,
        .label = "bundle",
    });
    REQUIRE(bundle_encoder);
    REQUIRE((*bundle_encoder)->Finish());
}

TEST_CASE("NullRHI accepts typed bind layouts, groups, constants, and multisample pipelines") {
    NullContext context;
    auto uniform_result = context.device->CreateBuffer({.size = 512, .usage = BufferUsage::Uniform | BufferUsage::CopyDst});
    REQUIRE(uniform_result);
    auto uniform = std::move(*uniform_result);

    const BindGroupLayoutEntryDesc layout_entry{
        .binding = 3,
        .visibility = static_cast<u32>(ShaderStage::Vertex | ShaderStage::Compute),
        .buffer = {.type = BufferBindingType::Uniform, .has_dynamic_offset = true, .min_binding_size = 16},
    };
    auto bind_layout_result = context.device->CreateBindGroupLayout({.entries = std::span(&layout_entry, 1), .label = "dynamic layout"});
    REQUIRE(bind_layout_result);
    auto bind_layout = std::move(*bind_layout_result);
    const BindGroupEntryDesc binding{.binding = 3, .buffer = uniform.get(), .offset = 0, .size = 16};
    auto bind_group_result = context.device->CreateBindGroup({.layout = bind_layout.get(), .entries = std::span(&binding, 1)});
    REQUIRE(bind_group_result);
    auto bind_group = std::move(*bind_group_result);
    BindGroupLayout* layouts[] = {bind_layout.get()};
    auto pipeline_layout_result = context.device->CreatePipelineLayout({.bind_group_layouts = layouts, .immediate_size = 16});
    REQUIRE(pipeline_layout_result);
    auto pipeline_layout = std::move(*pipeline_layout_result);

    auto shader = CreateShader(*context.device);
    auto compute = CreateComputePipeline(*context.device, *shader, pipeline_layout.get());
    auto render = CreateRenderPipeline(*context.device, *shader, 4);
    REQUIRE(compute->GetBindGroupLayout(0));
    REQUIRE(render->GetBindGroupLayout(0));

    auto encoder_result = context.device->CreateCommandEncoder();
    REQUIRE(encoder_result);
    auto encoder = std::move(*encoder_result);
    auto pass_result = encoder->BeginComputePass();
    REQUIRE(pass_result);
    auto pass = std::move(*pass_result);
    const u32 dynamic_offset = 256;
    pass->SetBindGroup(0, bind_group.get(), std::span(&dynamic_offset, 1));
    pass->SetPipeline(*compute);
    pass->DispatchWorkgroups(1);
    pass->End();
    REQUIRE(encoder->Finish());
}

TEST_CASE("NullRHI enforces encoder and pass scopes") {
    NullContext context;
    auto buffer_result = context.device->CreateBuffer({.size = 16, .usage = BufferUsage::CopyDst});
    auto encoder_result = context.device->CreateCommandEncoder();
    REQUIRE(buffer_result);
    REQUIRE(encoder_result);
    auto buffer = std::move(*buffer_result);
    auto encoder = std::move(*encoder_result);

    auto pass_result = encoder->BeginComputePass();
    REQUIRE(pass_result);
    auto pass = std::move(*pass_result);
    CHECK_FALSE(encoder->BeginComputePass());
    CHECK_FALSE(encoder->ClearBuffer(*buffer));
    CHECK_FALSE(encoder->Finish());
    pass->End();
    auto commands_result = encoder->Finish();
    REQUIRE(commands_result);
    CHECK_FALSE(encoder->Finish());

    auto commands = std::move(*commands_result);
    CommandBuffer* submitted[] = {commands.get()};
    REQUIRE(context.device->GetQueue().Submit(submitted));
    auto duplicate = context.device->GetQueue().Submit(submitted);
    REQUIRE_FALSE(duplicate);
    CHECK(duplicate.error().Code() == ErrorCode::ValidationInvalidState);
}

TEST_CASE("NullRHI records render, compute, indirect, marker, and submission breadcrumbs") {
    NullContext context;
    auto shader = CreateShader(*context.device);
    auto render_pipeline = CreateRenderPipeline(*context.device, *shader);
    auto compute_pipeline = CreateComputePipeline(*context.device, *shader);
    auto target_result = context.device->CreateTexture({.size = {4, 4, 1}, .format = TextureFormat::RGBA8Unorm, .usage = TextureUsage::RenderAttachment});
    auto indirect_result = context.device->CreateBuffer({.size = 128, .usage = BufferUsage::Indirect | BufferUsage::CopyDst});
    REQUIRE(target_result);
    REQUIRE(indirect_result);
    auto target = std::move(*target_result);
    auto indirect = std::move(*indirect_result);
    auto view = target->CreateView();

    auto encoder_result = context.device->CreateCommandEncoder({.label = "logged encoder"});
    REQUIRE(encoder_result);
    auto encoder = std::move(*encoder_result);
    encoder->InsertDebugMarker("before passes");
    const RenderPassColorAttachmentDesc color{.view = view.get()};
    auto render_result = encoder->BeginRenderPass(RenderPassDescTyped{.color_attachments = std::span(&color, 1)});
    REQUIRE(render_result);
    auto render = std::move(*render_result);
    render->SetPipeline(*render_pipeline);
    render->Draw(3, 2);
    render->DrawIndexed(6);
    render->DrawIndirectCommand(*indirect, 1);
    render->DrawIndexedIndirectCommand(*indirect, 1);
    render->MultiDrawIndirect(*indirect, 0, 2);
    render->MultiDrawIndexedIndirectCount(*indirect, 0, 2, *indirect, 20);
    render->InsertDebugMarker("render marker");
    render->End();

    auto compute_result = encoder->BeginComputePass();
    REQUIRE(compute_result);
    auto compute = std::move(*compute_result);
    compute->SetPipeline(*compute_pipeline);
    compute->DispatchWorkgroups(2, 3, 4);
    compute->DispatchWorkgroupsIndirect(*indirect, 0);
    compute->End();
    auto commands_result = encoder->Finish();
    REQUIRE(commands_result);
    auto commands = std::move(*commands_result);
    CommandBuffer* submitted[] = {commands.get()};
    REQUIRE(context.device->GetQueue().Submit(submitted));

    CHECK(LogContains(*context.device, "marker before passes"));
    CHECK(LogContains(*context.device, "render.begin"));
    CHECK(LogContains(*context.device, "render.draw 3 2"));
    CHECK(LogContains(*context.device, "render.draw_indexed 6 1"));
    CHECK(LogContains(*context.device, "render.draw_indirect"));
    CHECK(LogContains(*context.device, "render.draw_indexed_indirect"));
    CHECK(LogContains(*context.device, "render.multi_draw_indirect 2"));
    CHECK(LogContains(*context.device, "render.multi_draw_indexed_indirect 2 count"));
    CHECK(LogContains(*context.device, "compute.dispatch 2 3 4"));
    CHECK(LogContains(*context.device, "compute.dispatch_indirect"));
    CHECK(LogContains(*context.device, "queue.submit 1"));
}

TEST_CASE("NullRHI submission epochs advance monotonically on Tick") {
    NullRhiDescriptor descriptor;
    descriptor.complete_submissions_immediately = false;
    NullContext context(descriptor);
    CHECK_FALSE(context.device->GetQueue().CompletedSubmission().IsValid());
    CHECK(context.device->GetQueue().SubmissionTracking() == SubmissionTrackingStatus::Healthy);

    auto encoder_result = context.device->CreateCommandEncoder();
    REQUIRE(encoder_result);
    auto commands_result = (*encoder_result)->Finish();
    REQUIRE(commands_result);
    auto commands = std::move(*commands_result);
    CommandBuffer* submitted[] = {commands.get()};
    auto ticket = context.device->GetQueue().Submit(submitted);
    REQUIRE(ticket);
    CHECK_FALSE(context.device->GetQueue().CompletedSubmission().HasReached(*ticket));
    context.device->Tick();
    CHECK(context.device->GetQueue().CompletedSubmission().HasReached(*ticket));

    auto second_encoder = context.device->CreateCommandEncoder();
    REQUIRE(second_encoder);
    auto second_commands = (*second_encoder)->Finish();
    REQUIRE(second_commands);
    CommandBuffer* second_submission[] = {second_commands->get()};
    auto second_ticket = context.device->GetQueue().Submit(second_submission);
    REQUIRE(second_ticket);
    CHECK(second_ticket->Value() == ticket->Value() + 1);
    CHECK_FALSE(context.device->GetQueue().CompletedSubmission().HasReached(*second_ticket));
    context.device->Tick();
    CHECK(context.device->GetQueue().CompletedSubmission().HasReached(*second_ticket));

    bool completed = false;
    const auto future = context.device->GetQueue().OnSubmittedWorkDone(CallbackMode::AllowProcessEvents, [&](QueueWorkDoneStatus status, std::string_view message) {
        completed = status == QueueWorkDoneStatus::Success;
        CHECK(message.empty());
    });
    CHECK(future.completed);
    CHECK(future.success);
    CHECK(completed);
}

TEST_CASE("NullRHI reports device loss and permits deterministic replacement recovery") {
    std::atomic_bool callback_called{};
    DeviceLostReason callback_reason = DeviceLostReason::Unknown;
    std::string callback_message;
    DeviceDesc device_desc;
    device_desc.device_lost_callback = [&](DeviceLostReason reason, std::string_view message) {
        callback_reason = reason;
        callback_message = message;
        callback_called.store(true);
    };
    NullContext lost_context({}, device_desc);

    REQUIRE(LoseNullDevice(*lost_context.device, DeviceLostReason::FailedCreation, "injected loss"));
    CHECK(lost_context.device->IsLost());
    CHECK(lost_context.device->LossReason() == DeviceLostReason::FailedCreation);
    CHECK(lost_context.device->GetLostFuture().completed);
    CHECK(lost_context.device->GetLostFuture().success);
    CHECK(callback_called.load());
    CHECK(callback_reason == DeviceLostReason::FailedCreation);
    CHECK(callback_message == "injected loss");
    CHECK(LogContains(*lost_context.device, "device.lost injected loss"));

    auto encoder_result = lost_context.device->CreateCommandEncoder();
    REQUIRE(encoder_result);
    auto commands_result = (*encoder_result)->Finish();
    REQUIRE(commands_result);
    auto commands = std::move(*commands_result);
    CommandBuffer* submitted[] = {commands.get()};
    auto rejected = lost_context.device->GetQueue().Submit(submitted);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().Code() == ErrorCode::GraphicsDeviceLost);

    NullContext replacement;
    CHECK_FALSE(replacement.device->IsLost());
    auto replacement_encoder = replacement.device->CreateCommandEncoder();
    REQUIRE(replacement_encoder);
    auto replacement_commands = (*replacement_encoder)->Finish();
    REQUIRE(replacement_commands);
    CommandBuffer* recovery_submission[] = {replacement_commands->get()};
    REQUIRE(replacement.device->GetQueue().Submit(recovery_submission));
}
