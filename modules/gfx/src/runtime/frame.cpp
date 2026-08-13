#include "../internal/runtime_state.hpp"

namespace woki::gfx {

namespace {

u64 OutputSignature(
    const FrameOutputBinding& output,
    const RenderView& view,
    const PipelineHandle requested,
    const BorrowedPipeline& pipeline,
    const u64 light_count
) {
    detail::CanonicalHashWriter out;
    out.Value(pipeline.Version());
    out.Value(requested.Index());
    out.Value(requested.Generation());
    out.Value(output.format);
    out.Value(output.sample_count);
    out.Value(output.width);
    out.Value(output.height);
    out.Value(output.initial_state);
    out.Value(output.final_state);
    out.Value(output.present);
    out.Value(output.clear);
    out.Value(output.encoding);
    out.Value(view.render_width);
    out.Value(view.render_height);
    out.Value(light_count);
    return detail::Hash64(out.Finish());
}

const FrameOutputBinding* FindOutput(const std::span<const FrameOutputBinding> outputs, const ViewId view) {
    const auto found = std::ranges::find(outputs, view, &FrameOutputBinding::view);
    return found == outputs.end() ? nullptr : &*found;
}

void AddDiagnostic(
    std::vector<FeatureDiagnostic>& diagnostics,
    const StringId feature,
    const ViewId view,
    const DiagnosticSeverity severity,
    std::string code,
    std::string message
) {
    diagnostics.push_back(
        {
            .feature = feature,
            .view = view,
            .severity = severity,
            .code = std::move(code),
            .message = std::move(message),
            .stage = {},
            .pass = {},
            .resource = {},
        }
    );
}

} // namespace

Result<ref<rhi::RenderPipeline>> RenderRuntime::Impl::CreateGraphicsPipeline(
    const GraphicsPipelineKey& key,
    rhi::PipelineLayout& layout,
    const VertexSchema& schema,
    const MaterialRenderState& state
) {
    BorrowedShader shader;
    auto local = shaders.BorrowProduct(key.shader_product, key.shader_generation);
    if (local)
        shader = std::move(*local);
    else
        TRY_ASSIGN(shader, feature_services.programs->BorrowShader(key.shader_product, key.shader_generation));
    std::string vertex_entry;
    for (const auto& entry : shader.Interface().entry_points) {
        if (entry.stage != ShaderStage::Vertex || StringId(entry.name) != key.vertex_entry)
            continue;
        if (ValidateVertexSchema(schema, shader.Interface(), entry.name))
            vertex_entry = entry.name;
        break;
    }
    if (vertex_entry.empty())
        for (const auto& entry : shader.Interface().entry_points)
            if (entry.stage == ShaderStage::Vertex && ValidateVertexSchema(schema, shader.Interface(), entry.name)) {
                vertex_entry = entry.name;
                break;
            }
    if (vertex_entry.empty())
        return Err(
            ErrorCode::ValidationInvalidState,
            "shader has no reflected vertex entry compatible with the mesh schema"
        );
    std::string fragment_entry;
    if (!key.fragment_entry.Empty())
        for (const auto& entry : shader.Interface().entry_points)
            if (entry.stage == ShaderStage::Fragment && StringId(entry.name) == key.fragment_entry) {
                fragment_entry = entry.name;
                break;
            }
    if (!key.fragment_entry.Empty() && fragment_entry.empty())
        return Err(
            ErrorCode::ValidationInvalidState,
            "material fragment entry is absent from the published shader product"
        );
    std::vector<std::vector<rhi::VertexAttributeDesc>> attributes(schema.streams.size());
    std::vector<rhi::VertexBufferLayoutDesc> streams;
    streams.reserve(schema.streams.size());
    for (u32 stream_index = 0; stream_index < schema.streams.size(); ++stream_index) {
        for (const auto& attribute : schema.streams[stream_index].attributes)
            attributes[stream_index].push_back(
                {.format = detail::ToRhiVertexFormat(attribute.format),
                    .offset = attribute.offset,
                    .shader_location = attribute.location}
            );
        streams.push_back(
            {.step_mode = schema.streams[stream_index].step_mode == VertexStepMode::Instance
                              ? rhi::VertexStepMode::Instance
                              : rhi::VertexStepMode::Vertex,
                .array_stride = schema.streams[stream_index].stride,
                .attributes = attributes[stream_index]}
        );
    }
    const rhi::VertexStateDesc vertex{.module = &shader.Module(), .entry_point = vertex_entry, .buffers = streams};
    rhi::BlendStateDesc alpha_blend{
        {rhi::BlendOperation::Add, rhi::BlendFactor::SrcAlpha, rhi::BlendFactor::OneMinusSrcAlpha},
        {rhi::BlendOperation::Add, rhi::BlendFactor::One, rhi::BlendFactor::OneMinusSrcAlpha}};
    rhi::BlendStateDesc additive{{rhi::BlendOperation::Add, rhi::BlendFactor::SrcAlpha, rhi::BlendFactor::One},
        {rhi::BlendOperation::Add, rhi::BlendFactor::One, rhi::BlendFactor::One}};
    std::vector<rhi::ColorTargetStateDesc> colors;
    for (const auto format : key.targets.colors)
        colors.push_back(
            {.format = format,
                .blend = state.blend == MaterialBlendMode::Alpha      ? &alpha_blend
                         : state.blend == MaterialBlendMode::Additive ? &additive
                                                                      : nullptr}
        );
    const rhi::FragmentStateDesc fragment{.module = &shader.Module(), .entry_point = fragment_entry, .targets = colors};
    const rhi::PrimitiveStateDesc primitive{.topology = rhi::PrimitiveTopology::TriangleList,
        .front_face = rhi::FrontFace::CCW,
        .cull_mode = state.cull};
    const rhi::DepthStencilStateDesc depth{.format = key.targets.depth,
        .depth_write_enabled = state.depth_write,
        .depth_compare = state.depth_compare};
    rhi::RenderPipelineDescTyped descriptor{.layout = &layout,
        .vertex = &vertex,
        .primitive = &primitive,
        .depth_stencil = key.targets.depth == rhi::TextureFormat::Undefined ? nullptr : &depth,
        .multisample = {.count = key.targets.samples},
        .fragment = fragment_entry.empty() ? nullptr : &fragment,
        .label = "Material pipeline"};
    auto created = device->CreateRenderPipeline(descriptor);
    if (!created)
        return Err(std::move(created).error());
    return Ok(ref<rhi::RenderPipeline>(std::move(*created)));
}

Result<RenderRuntime::Impl::Executable*> RenderRuntime::Impl::GetExecutable(
    const RenderWorldSnapshot& world_snapshot,
    const RenderView& view,
    const FrameOutputBinding& output,
    const u64 family_id
) {
    BorrowedPipeline selected;
    TRY_ASSIGN(selected, pipelines.SelectSupported(view.pipeline, capabilities));
    const u64 signature = OutputSignature(output, view, view.pipeline, selected, world_snapshot.Lights().size());
    if (const auto found = executables.find(view.id);
        found != executables.end() && found->second->signature == signature) {
        TRY_VOID(
            PreparePipelineScene(found->second->pipeline, {world_snapshot, std::span<const RenderView>(&view, 1)})
        );
        return Ok(found->second.get());
    }

    PipelineInstance pipeline_instance;
    FeatureServices services{.standard = &feature_services, .acquire = {}};
    services.acquire = [this, &view, family_id, pipeline = selected](
                           const CompiledFeatureConfig& config
                       ) -> Result<ref<const RenderFeature>> {
        const auto* metadata = features.Metadata(config.feature);
        if (metadata == nullptr)
            return Err(ErrorCode::InvalidArgument, "unknown pooled render feature");
        detail::CanonicalHashWriter identity_writer;
        identity_writer.Value(metadata->scope);
        switch (metadata->scope) {
            case FeatureScope::Runtime:
                identity_writer.Value(1U);
                break;
            case FeatureScope::Scene:
                identity_writer.Value(view.scene.Index());
                identity_writer.Value(view.scene.Generation());
                break;
            case FeatureScope::Pipeline:
                identity_writer.Value(pipeline.Get().asset_id);
                identity_writer.Value(pipeline.Version());
                break;
            case FeatureScope::ViewFamily:
                identity_writer.Value(family_id);
                break;
            case FeatureScope::View:
                identity_writer.Value(view.id.Index());
                identity_writer.Value(view.id.Generation());
                break;
        }
        const FeaturePoolKey key{config.hash,
            metadata->scope,
            identity_writer.Finish(),
            metadata->scope == FeatureScope::Pipeline ? pipeline.Version() : 1};
        if (auto found = feature_pool.find(key); found != feature_pool.end()) {
            found->second.last_frame = frame_number;
            return Ok(found->second.feature);
        }
        ref<const RenderFeature> created;
        // Standard services are injected only while constructing registered
        // runtime features; no service object is retained outside the instance.
        TRY_ASSIGN(created, features.Create(config, {.standard = &feature_services, .acquire = {}}));
        feature_pool.emplace(key, FeaturePoolEntry{created, frame_number});
        return Ok(std::move(created));
    };
    TRY_ASSIGN(pipeline_instance, pipelines.Compose(view.pipeline, capabilities, features, services));
    TRY_VOID(PreparePipelineScene(pipeline_instance, {world_snapshot, std::span<const RenderView>(&view, 1)}));
    RenderGraphBuilder builder;
    GraphTextureDesc output_descriptor;
    output_descriptor.label = "Frame output";
    output_descriptor.extent = GraphExtent::Fixed(output.width, output.height);
    output_descriptor.sample_count = output.sample_count;
    output_descriptor.format = output.format;
    output_descriptor
        .usage = rhi::TextureUsage::RenderAttachment | rhi::TextureUsage::TextureBinding | rhi::TextureUsage::CopySrc;
    ExternalTextureContract contract;
    contract.descriptor = output_descriptor;
    contract.initial_state = output.present ? ExternalState::Present : output.initial_state;
    contract.final_state = output.present ? ExternalState::Present : output.final_state;
    contract.frame_bound = true;
    const auto target = builder.ImportTexture(std::move(contract));
    const bool hdr = std::ranges::find(pipeline_instance.pipeline.Get().required_capabilities, StringId("hdr"))
                     != pipeline_instance.pipeline.Get().required_capabilities.end();
    const bool direct = !hdr && (view.render_width == 0 || view.render_width == output.width)
                        && (view.render_height == 0 || view.render_height == output.height)
                        && (pipeline_instance.pipeline.Get().targets.color.empty()
                            || pipeline_instance.pipeline.Get().targets.color.front() == PipelineTargetFormat::Surface
                            || ToRhiTextureFormat(pipeline_instance.pipeline.Get().targets.color.front())
                                   == output.format)
                        && pipeline_instance.pipeline.Get().targets.samples == output.sample_count;
    auto inserted = builder.Blackboard().Emplace<RenderTargetOutput>(RenderTargetOutput{target,
        builder.Initial(target),
        output.format,
        output.sample_count,
        direct,
        output.present ? ExternalState::Present : output.final_state,
        output.encoding});
    if (!inserted)
        return Err(std::move(inserted).error());
    const u32 render_width = view.render_width == 0 ? output.width : view.render_width;
    const u32 render_height = view.render_height == 0 ? output.height : view.render_height;
    TRY_VOID(DeclarePipelineGraph(pipeline_instance, builder, render_width, render_height));
    auto report = builder.CompileWithReport(output.width, output.height);
    if (!report) {
        for (const auto& diagnostic : report.diagnostics)
            if (frame_state.diagnostics != nullptr)
                frame_state.diagnostics->push_back(
                    {StringId("render-graph"),
                        view.id,
                        DiagnosticSeverity::Error,
                        diagnostic.code,
                        diagnostic.message,
                        diagnostic.stage,
                        diagnostic.pass,
                        diagnostic.resource}
                );
        return Err(
            ErrorCode::ValidationInvalidState,
            report.diagnostics.empty() ? "render graph compilation failed" : report.diagnostics.front().message
        );
    }
    auto graph = std::move(*report.graph);
    auto executable = createScope<Executable>();
    executable->signature = signature;
    executable->pipeline = std::move(pipeline_instance);
    executable->output = target;
    executable->output_descriptor = output_descriptor;
    executable->direct = direct;
    executable->executor = createScope<GraphExecutor>(device, std::move(graph), resources->Releases());
    auto* result = executable.get();
    executables.insert_or_assign(view.id, std::move(executable));
    return Ok(result);
}

Result<CoreFrameResult> RenderRuntime::Impl::Frame(const CoreFrameRequest& request) {
    Impl* runtime = this;
    const auto frame_started = std::chrono::steady_clock::now();
    if (runtime->shutdown)
        return Err(ErrorCode::InvalidState, "render runtime is shut down");
    if (runtime->device->IsLost() || (runtime->recovery_factory != nullptr && runtime->recovery_factory->DeviceLost()))
        runtime->MarkDeviceLost();
    if (runtime->recovery_state == RenderRecoveryState::DeviceLost) {
        auto recovered = runtime->RecoverDevice();
        if (!recovered) {
            runtime->recovery_state = RenderRecoveryState::Failed;
            return Err(std::move(recovered).error());
        }
    }
    if (runtime->recovery_state != RenderRecoveryState::Ready)
        return Err(ErrorCode::GraphicsDeviceLost, "render submissions are stopped while the device is lost");
    if (runtime->completions != nullptr)
        static_cast<void>(runtime->completions->Drain());
    const auto publications = runtime->assets.manager->PumpPublications();
    if (publications != 0 && runtime->flush_asset_manifest)
        TRY_VOID(runtime->flush_asset_manifest());
    runtime->resources->Collect();
    if (runtime->feature_readbacks != nullptr)
        static_cast<void>(runtime->feature_readbacks->Poll(runtime->device->GetQueue().CompletedSubmission()));
    std::erase_if(runtime->feature_pool, [frame = runtime->frame_number](const auto& item) {
        return item.second.last_frame + 3 < frame && item.second.feature.use_count() == 1;
    });
    TRY_VOID(runtime->textures->Pump());
    TRY_VOID(runtime->meshes->Pump());
    std::vector<std::string> generated_material_failures;
    for (const auto id : runtime->meshes->DrainGeneratedMaterials()) {
        auto requested = runtime->materials->RequestInstance(id);
        if (!requested)
            generated_material_failures.emplace_back(requested.error().Message());
    }
    TRY_VOID(runtime->materials->Pump());
    TRY_VOID(runtime->materials->PublishReloads());
    std::optional<std::string> optional_material_failure;
    if (auto prepared = runtime->material_preparation->PrepareDirty(); !prepared) {
        if (runtime->material_failure_policy == PreparationFailurePolicy::Required)
            return Err(std::move(prepared).error());
        optional_material_failure = std::string(prepared.error().Message());
    }

    std::map<SceneHandle, std::shared_ptr<const RenderWorldSnapshot>> snapshots;
    for (const auto& scene : request.scenes) {
        if (scene.value == nullptr)
            continue;
        auto drained = scene.value->FreezeAndDrain();
        if (!drained)
            return Err(std::move(drained).error());
        auto [builder, _] = runtime->scene_worlds.try_emplace(scene.handle, scene.handle);
        std::shared_ptr<const RenderWorldSnapshot> snapshot;
        TRY_ASSIGN(snapshot, builder->second.Apply(*drained));
        snapshots.insert_or_assign(scene.handle, std::move(snapshot));
    }
    auto uploads = runtime->resources->Uploads().PrepareAndSubmit();
    auto frame_context_result = runtime->resources->AcquireFrame();
    if (!frame_context_result)
        return Err(std::move(frame_context_result).error());
    FrameContext& frame_context = frame_context_result->get();

    CoreFrameResult result;
    ++runtime->frame_number;
    if (optional_material_failure)
        AddDiagnostic(
            result.diagnostics,
            StringId("materials"),
            {},
            DiagnosticSeverity::Warning,
            "RND2012",
            std::move(*optional_material_failure)
        );
    for (auto& failure : generated_material_failures)
        AddDiagnostic(
            result.diagnostics,
            StringId("materials"),
            {},
            DiagnosticSeverity::Warning,
            "RND2011",
            std::move(failure)
        );
    for (const auto& [_, snapshot] : snapshots)
        result.stats.extracted_objects += snapshot->ObjectData().ids.size();
    runtime->frame_state.diagnostics = &result.diagnostics;
    rhi::SubmissionTicket frame_submission;
    if (!uploads && runtime->upload_failure_policy == PreparationFailurePolicy::Required) {
        static_cast<void>(frame_context.Cancel());
        return Err(std::move(uploads).error());
    }
    if (!uploads)
        AddDiagnostic(
            result.diagnostics,
            StringId("uploads"),
            {},
            DiagnosticSeverity::Error,
            "RND1007",
            std::string(uploads.error().Message())
        );
    else
        for (const auto& upload : *uploads)
            if (!frame_submission.IsValid() || frame_submission < upload.submission)
                frame_submission = upload.submission;
    for (const auto& family : request.view_families) {
        for (const auto& requested_view : family.views) {
            auto view = requested_view;
            const auto scene_snapshot = snapshots.find(view.scene);
            if (scene_snapshot == snapshots.end()) {
                AddDiagnostic(
                    result.diagnostics,
                    StringId("runtime"),
                    view.id,
                    DiagnosticSeverity::Error,
                    "RND1009",
                    "view scene snapshot is unavailable"
                );
                continue;
            }
            const auto& snapshot = scene_snapshot->second;
            const auto* output = FindOutput(request.outputs, view.id);
            if (output == nullptr || output->view_texture == nullptr || output->width == 0 || output->height == 0) {
                AddDiagnostic(
                    result.diagnostics,
                    StringId("runtime"),
                    view.id,
                    DiagnosticSeverity::Error,
                    "RND1001",
                    "view output binding is missing or invalid"
                );
                continue;
            }
            const u32 jitter_width = view.render_width == 0 ? output->width : view.render_width;
            const u32 jitter_height = view.render_height == 0 ? output->height : view.render_height;
            view.output_width = output->width;
            view.output_height = output->height;
            view.jitter = view.temporal.taa && view.temporal.jitter
                              ? TemporalJitter(runtime->frame_number, jitter_width, jitter_height)
                              : math::vec2f{};
            if (output->encoding == SdrTargetEncoding::Hdr10Pq) {
                AddDiagnostic(
                    result.diagnostics,
                    StringId("presentation"),
                    view.id,
                    DiagnosticSeverity::Error,
                    "RND4001",
                    "HDR10/PQ output is reserved but not supported by this renderer"
                );
                continue;
            }
            TRY_VOID(runtime->world->gpu_scene->Synchronize(*snapshot));
            TRY_VOID(runtime->world->gpu_scene->UploadDirty());
            runtime->bindings->UseGpuScene(runtime->world->gpu_scene->Tables().instances);
            runtime->frame_state.world = snapshot.get();
            runtime->frame_state.view = &view;
            runtime->frame_state.draw_calls = 0;
            runtime->frame_state.skipped = 0;
            runtime->frame_state.cluster_overflow = 0;
            runtime->frame_state.shadow_draws = 0;
            runtime->frame_state.shadow_cascades = 0;
            runtime->frame_state.bloom_passes = 0;
            runtime->frame_state.fallback_materials = 0;
            runtime->frame_state.fallback_textures = 0;
            runtime->frame_state.gpu_visibility = {};
            runtime->frame_state.exposure = view.exposure;
            runtime->frame_state.delta_time = request.delta_time;
            runtime->frame_state.frame_number = runtime->frame_number;
            runtime->frame_state.used_pipelines.clear();
            runtime->frame_state.clear_output = output->clear;
            if (runtime->bindings == nullptr) {
                AddDiagnostic(
                    result.diagnostics,
                    StringId("runtime"),
                    view.id,
                    DiagnosticSeverity::Error,
                    "RND1008",
                    "physical ABI resources are unavailable"
                );
                continue;
            }
            auto prepared_bindings = runtime->bindings->Prepare(*snapshot, view, request.time, request.delta_time);
            if (!prepared_bindings) {
                AddDiagnostic(
                    result.diagnostics,
                    StringId("runtime"),
                    view.id,
                    DiagnosticSeverity::Error,
                    "RND1008",
                    std::string(prepared_bindings.error().Message())
                );
                continue;
            }
            if (runtime->feature_services.resolve_environment && view.environment.IsValid()) {
                auto environment = runtime->feature_services.resolve_environment(view.environment);
                if (environment)
                    static_cast<void>(runtime->bindings
                            ->UseEnvironment(*environment, view.environment_lighting, view.sky_visible));
                else
                    AddDiagnostic(
                        result.diagnostics,
                        StringId("environment"),
                        view.id,
                        DiagnosticSeverity::Warning,
                        "RND5001",
                        std::string(environment.error().Message())
                    );
            }
            auto binding_uploads = runtime->resources->Uploads().PrepareAndSubmit();
            if (!binding_uploads) {
                AddDiagnostic(
                    result.diagnostics,
                    StringId("uploads"),
                    view.id,
                    DiagnosticSeverity::Error,
                    "RND1007",
                    std::string(binding_uploads.error().Message())
                );
                continue;
            }
            for (const auto& upload : *binding_uploads)
                if (!frame_submission.IsValid() || frame_submission < upload.submission)
                    frame_submission = upload.submission;
            auto selected = runtime->pipelines.SelectSupported(view.pipeline, runtime->capabilities);
            if (!selected) {
                AddDiagnostic(
                    result.diagnostics,
                    StringId("runtime"),
                    view.id,
                    DiagnosticSeverity::Error,
                    "RND1002",
                    std::string(selected.error().Message())
                );
                continue;
            }
            const bool hdr = std::ranges::find(selected->Get().required_capabilities, StringId("hdr"))
                             != selected->Get().required_capabilities.end();
            runtime->frame_state.targets
                .colors = {hdr ? rhi::TextureFormat::RGBA16Float
                           : selected->Get().targets.color.empty()
                                   || selected->Get().targets.color.front() == PipelineTargetFormat::Surface
                               ? output->format
                               : *ToRhiTextureFormat(selected->Get().targets.color.front())};
            runtime->frame_state.targets.depth = selected->Get().targets.depth
                                                     ? *ToRhiTextureFormat(*selected->Get().targets.depth)
                                                     : rhi::TextureFormat::Depth24Plus;
            runtime->frame_state.targets.samples = selected->Get().targets.samples;
            Impl::Executable* executable{};
            if (view.temporal.taa
                || (runtime->feature_services.programs != nullptr
                    && runtime->feature_services.programs->GpuDrivenReady()))
                runtime->executables.erase(view.id);
            auto resolved = runtime->GetExecutable(*snapshot, view, *output, family.family_id);
            if (!resolved) {
                AddDiagnostic(
                    result.diagnostics,
                    StringId("runtime"),
                    view.id,
                    DiagnosticSeverity::Error,
                    "RND1002",
                    std::string(resolved.error().Message())
                );
                continue;
            }
            executable = *resolved;
            runtime->frame_state.direct_output = executable->direct;
            auto graph_frame = executable->executor->Begin(output->width, output->height);
            if (!graph_frame) {
                AddDiagnostic(
                    result.diagnostics,
                    StringId("runtime"),
                    view.id,
                    DiagnosticSeverity::Error,
                    "RND1004",
                    std::string(graph_frame.error().Message())
                );
                continue;
            }
            Result<void> bound = output->texture != nullptr ? graph_frame->Bind(
                                                                  {.resource = executable->output,
                                                                      .texture = output->texture,
                                                                      .view = output->view_texture,
                                                                      .signature = executable->output_descriptor,
                                                                      .view_signature = {}}
                                                              )
                                                            : graph_frame->Bind(
                                                                  GraphTextureBinding{.resource = executable->output,
                                                                      .texture = {},
                                                                      .view = output->view_texture,
                                                                      .signature = executable->output_descriptor,
                                                                      .view_signature = {}}
                                                              );
            if (!bound) {
                AddDiagnostic(
                    result.diagnostics,
                    StringId("runtime"),
                    view.id,
                    DiagnosticSeverity::Error,
                    "RND1004",
                    std::string(bound.error().Message())
                );
                continue;
            }
            auto submitted = graph_frame->Execute();
            if (!submitted) {
                AddDiagnostic(
                    result.diagnostics,
                    StringId("runtime"),
                    view.id,
                    DiagnosticSeverity::Error,
                    "RND1005",
                    std::string(submitted.error().Message())
                );
                continue;
            }
            frame_submission = std::max(frame_submission, *submitted);
            result.submitted_views.push_back(view.id);
            runtime->feature_services.programs->OnSubmitted(*submitted);
            for (const auto& feature : executable->pipeline.features)
                feature->OnSubmitted(*submitted);
            if (runtime->histories != nullptr)
                runtime->histories->Collect(
                    runtime->device->GetQueue().CompletedSubmission(),
                    runtime->frame_number,
                    view.temporal.maximum_age
                );
            result.stats.visible_objects += runtime->frame_state.visible_objects;
            result.stats.packets += runtime->frame_state.packet_count;
            result.stats.draw_calls += runtime->frame_state.draw_calls;
            result.stats.skipped_pending_resources += runtime->frame_state.skipped;
            result.stats.cluster_overflow += runtime->frame_state.cluster_overflow;
            result.stats.shadow_draws += runtime->frame_state.shadow_draws;
            result.stats.shadow_cascades += runtime->frame_state.shadow_cascades;
            result.stats.bloom_passes += runtime->frame_state.bloom_passes;
            result.stats.fallback_materials += runtime->frame_state.fallback_materials;
            result.stats.fallback_textures += runtime->frame_state.fallback_textures;
            result.stats.gpu_visibility = runtime->frame_state.gpu_visibility;
            result.stats.graph_hash ^= executable->executor->Graph().DeterministicHash();
            result.stats.transient_bytes += executable->executor->Graph().EstimatedTransientPeakBytes();
            for (const auto& phase : runtime->frame_state.draws.phases)
                for (const auto& draw : phase) {
                    runtime->meshes->MarkUsed(draw.mesh, *submitted);
                    runtime->material_preparation->MarkUsed(draw.packet.material, *submitted);
                }
            for (const auto& pipeline : runtime->frame_state.used_pipelines)
                static_cast<void>(runtime->pipeline_cache.MarkUsed(pipeline, *submitted));
            for (const auto& pipeline : runtime->frame_state.used_pipelines) {
                auto shader = runtime->shaders.BorrowProduct(pipeline.shader_product, pipeline.shader_generation);
                if (shader)
                    runtime->shaders.MarkUsed(*shader, *submitted);
            }
            runtime->world->gpu_scene->MarkUsed(*submitted);
            runtime->bindings->MarkUsed(*submitted);
            if (output->offscreen.IsValid()) {
                auto& target = runtime->offscreen_slots[output->offscreen.Index()];
                target.last_used = std::max(target.last_used, *submitted);
            } else if (output->surface.IsValid()) {
                auto& surface = runtime->surface_slots[output->surface.Index()];
                surface.last_used = std::max(surface.last_used, *submitted);
            }
        }
    }
    if (runtime->feature_readbacks != nullptr) {
        auto diagnostic_readbacks = runtime->feature_readbacks->PrepareAndSubmit();
        if (diagnostic_readbacks)
            for (const auto& readback : *diagnostic_readbacks)
                frame_submission = std::max(frame_submission, readback.submission);
        else
            AddDiagnostic(
                result.diagnostics,
                StringId("gpu-visibility"),
                {},
                DiagnosticSeverity::Warning,
                "RND3103",
                std::string(diagnostic_readbacks.error().Message())
            );
    }
    if (request.canvas != nullptr && request.canvas_target != nullptr) {
        auto canvas_result = runtime->canvas->Execute(
            *request.canvas,
            *request.canvas_target,
            [runtime](const CanvasImageSource& source) -> Result<CanvasResolvedImage> {
                if (const auto* handle = std::get_if<TextureHandle>(&source)) {
                    ResolvedTexture resolved;
                    TRY_ASSIGN(resolved, runtime->textures->Resolve(*handle, TextureSemantic::Color));
                    if (resolved.physical == nullptr)
                        return Err(
                            ErrorCode::GraphicsResourceCreationFailed,
                            "canvas texture has no physical generation"
                        );
                    return Ok(
                        CanvasResolvedImage{resolved.physical->texture, resolved.physical->default_view, *handle}
                    );
                }
                if (const auto* handle = std::get_if<OffscreenTargetHandle>(&source)) {
                    if (!handle->IsValid() || handle->Index() >= runtime->offscreen_slots.size())
                        return Err(ErrorCode::InvalidArgument, "canvas image references a stale offscreen target");
                    const auto& slot = runtime->offscreen_slots[handle->Index()];
                    if (slot.generation != handle->Generation() || !slot.descriptor || slot.texture == nullptr
                        || slot.view == nullptr)
                        return Err(ErrorCode::InvalidArgument, "canvas image references a stale offscreen target");
                    return Ok(CanvasResolvedImage{slot.texture, slot.view, {}});
                }
                return Err(ErrorCode::InvalidArgument, "canvas image has no logical source");
            }
        );
        if (!canvas_result) {
            AddDiagnostic(
                result.diagnostics,
                StringId("canvas"),
                {},
                DiagnosticSeverity::Error,
                "RND6001",
                std::string(canvas_result.error().Message())
            );
        } else if (canvas_result->submission.IsValid()) {
            frame_submission = std::max(frame_submission, canvas_result->submission);
            if (request.canvas->surface.IsValid()) {
                auto& surface = runtime->surface_slots[request.canvas->surface.Index()];
                surface.last_used = std::max(surface.last_used, canvas_result->submission);
            }
            for (const auto texture : canvas_result->textures)
                runtime->textures->MarkUsed(texture, canvas_result->submission);
        }
    }
    auto facade_submission = runtime->PumpFacade();
    if (!facade_submission) {
        static_cast<void>(frame_context.Cancel());
        return Err(std::move(facade_submission).error());
    }
    if (facade_submission->IsValid())
        frame_submission = std::max(frame_submission, *facade_submission);
    if (!frame_submission.IsValid()) {
        static_cast<void>(frame_context.Cancel());
        result.stats.uploads = runtime->resources->Uploads().Stats();
        result.stats.recovery_state = runtime->recovery_state;
        return Ok(std::move(result));
    }
    TRY_VOID(frame_context.Submit(frame_submission));
    result.submission = frame_submission;
    runtime->last_submission = frame_submission;
    result.stats.uploads = runtime->resources->Uploads().Stats();
    const u64 fallback_textures = runtime->textures->Stats().fallback_resolves;
    result.stats
        .fallback_textures += fallback_textures - std::min(fallback_textures, runtime->reported_fallback_textures);
    runtime->reported_fallback_textures = fallback_textures;
    result.stats.submission_epoch = rhi::SubmissionEpoch(frame_submission.Value());
    result.stats.recovery_state = runtime->recovery_state;
    const auto frame_finished = std::chrono::steady_clock::now();
    result.stats.cpu_stages
        .push_back({"frame", std::chrono::duration<f64, std::milli>(frame_finished - frame_started).count()});
    return Ok(std::move(result));
}

} // namespace woki::gfx
