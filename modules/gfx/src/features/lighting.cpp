#include "internal.hpp"

namespace woki::gfx::feature_detail {
namespace {

class VelocityFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;

    Result<void> DeclareGraph(GraphDeclarationContext& context) const override {
        const auto* depth = context.blackboard.Get<DepthFeatureOutput>();
        if (depth == nullptr || context.blackboard.Get<MeshFeatureOutput>() == nullptr)
            return Err(ErrorCode::ValidationInvalidState, "velocity feature requires mesh and depth outputs");
        auto descriptor = TextureDescriptor("Physical velocity", rhi::TextureFormat::RG16Float, 1);
        descriptor.extent = GraphExtent::Fixed(context.width, context.height);
        auto velocity = context.graph.CreateTexture(descriptor);
        const PipelineTargetSignature targets{{rhi::TextureFormat::RG16Float}, Frame()->targets.depth, 1};
        for (const RenderPhase phase : {RenderPhase::Opaque, RenderPhase::AlphaTest})
            for (const auto& draw : Frame()->draws.phases[static_cast<size_t>(phase)]) {
                const auto* product = services_->meshes->Product(draw.mesh);
                if (product == nullptr || !draw.packet.material.IsValid())
                    continue;
                auto prepared = services_->materials->PipelineRequest(
                    draw.packet.material,
                    MaterialPass::Velocity,
                    targets,
                    product->schema.id,
                    draw.packet.palette.IsValid()
                );
                if (!prepared)
                    continue;
                BorrowedLayout layout;
                TRY_ASSIGN(layout, services_->bindings->PrepareLayout(prepared->layout, draw.packet.palette));
                auto request = services_->pipelines->Request(
                    prepared->pipeline,
                    [services = services_,
                        key = prepared->pipeline,
                        layout,
                        schema = product->schema,
                        state = prepared->render_state]() {
                        return services->create_graphics_pipeline(key, layout.Pipeline(), schema, state);
                    }
                );
                if (!request || request->state != PipelineRequestState::Ready)
                    return Err(
                        ErrorCode::GraphicsResourceCreationFailed,
                        request ? request->diagnostic : std::string(request.error().Message())
                    );
            }
        auto pass = context.graph.AddPass("Physical mesh velocity", PassKind::Render);
        const auto version = pass.Color(velocity, ColorLoad(rhi::LoadOp::Clear, {0, 0, 0, 0}));
        static_cast<void>(pass.Depth(depth->depth, DepthLoad(rhi::LoadOp::Load, true)));
        auto* services = services_;
        pass.Execute([services](RenderGraphContext& graph) -> Result<void> {
            const PipelineTargetSignature targets{{rhi::TextureFormat::RG16Float}, services->frame->targets.depth, 1};
            const auto& view = *services->frame->view;
            const auto previous_jitter = view.temporal.jitter
                                             ? TemporalJitter(
                                                   services->frame->frame_number - 1,
                                                   view.render_width == 0 ? view.output_width : view.render_width,
                                                   view.render_height == 0 ? view.output_height : view.render_height
                                               )
                                             : math::vec2f{};
            TRY_VOID(services->bindings->UseTemporalView(graph.CommandEncoder(), view, previous_jitter));
            TRY_VOID(Draw(services, graph, RenderPhase::Opaque, MaterialPass::Velocity, targets));
            return Draw(services, graph, RenderPhase::AlphaTest, MaterialPass::Velocity, targets);
        });
        auto result = context.blackboard.Emplace<VelocityOutputs>(VelocityOutputs{velocity, version});
        return result ? Ok() : Err(std::move(result).error());
    }
};

class ClusteredLightingFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;

    ~ClusteredLightingFeature() override {
        if (diagnostics_buffer_ != nullptr && services_ != nullptr && services_->releases != nullptr)
            services_->releases->Retire(std::move(diagnostics_buffer_), diagnostics_last_use_);
    }

    void OnSubmitted(const rhi::SubmissionTicket submission) const override {
        if (services_ == nullptr || services_->readbacks == nullptr || diagnostics_buffer_ == nullptr
            || pending_readback_.IsValid())
            return;
        auto queued = services_->readbacks->Enqueue({diagnostics_buffer_, 0, sizeof(u32)});
        if (!queued)
            return;
        pending_readback_ = *queued;
        diagnostics_last_use_ = std::max(diagnostics_last_use_, submission);
    }

    Result<void> PrepareScene(const RenderScenePreparationContext& context) const override {
        auto* frame = Frame();
        if (frame == nullptr)
            return Err(ErrorCode::InvalidState, "lighting feature has no frame state");
        if (pending_readback_.IsValid() && services_->readbacks != nullptr
            && services_->readbacks->State(pending_readback_) == ReadbackState::Ready) {
            auto bytes = services_->readbacks->Take(pending_readback_);
            if (bytes && bytes->size() == sizeof(u32)) {
                u32 overflow{};
                std::memcpy(&overflow, bytes->data(), sizeof(overflow));
                frame->cluster_overflow += overflow;
                if (overflow != 0)
                    Diagnostic(
                        services_,
                        "RND3001",
                        "Forward+ cluster light capacity overflowed by " + std::to_string(overflow) + " assignments"
                    );
            }
            pending_readback_ = {};
        }
        frame->local_lights.clear();
        frame->directional_count = 0;
        for (const auto& light : context.world.Lights()) {
            if (frame->view != nullptr && (light.visibility_mask & frame->view->layer_mask) == 0)
                continue;
            if (light.type == LightType::Directional && frame->directional_count < frame->directional_lights.size())
                frame->directional_lights[frame->directional_count++] = PackGpuLight(light, {});
            else if (light.type != LightType::Directional)
                frame->local_lights.push_back(PackGpuLight(light, {}));
        }
        return Ok();
    }

    Result<void> DeclareGraph(GraphDeclarationContext& context) const override {
        const auto integer = [&](const std::string_view name, const u32 fallback, const u32 maximum) {
            const auto* value = Setting(name);
            return value == nullptr ? fallback : static_cast<u32>(std::clamp<i64>(std::get<i64>(*value), 1, maximum));
        };
        abi::ClusterParams params;
        params.dimensions = {integer("clustersX", 16, 64),
            integer("clustersY", 9, 64),
            integer("clustersZ", 24, 64),
            integer("maxLights", 64, 256)};
        if (Frame() != nullptr && Frame()->view != nullptr) {
            const auto& camera = Frame()->view->camera;
            params.depth[0] = std::max(camera.near_plane, 0.001F);
            params.depth[1] = std::max(camera.far_plane, params.depth[0] + 0.001F);
            params.depth[2] = std::log(params.depth[1] / params.depth[0]);
            params.projection = {camera.projection(0, 0),
                camera.projection(1, 1),
                static_cast<f32>(context.width),
                static_cast<f32>(context.height)};
            params.view_row_0 = {camera.view(0, 0), camera.view(0, 1), camera.view(0, 2), camera.view(0, 3)};
            params.view_row_1 = {camera.view(1, 0), camera.view(1, 1), camera.view(1, 2), camera.view(1, 3)};
            params.view_row_2 = {camera.view(2, 0), camera.view(2, 1), camera.view(2, 2), camera.view(2, 3)};
        }
        params.depth[3] = Frame() == nullptr ? 0.0F : static_cast<f32>(Frame()->local_lights.size());
        const u32 cluster_count = params.dimensions[0] * params.dimensions[1] * params.dimensions[2];
        const u64 light_bytes = std::max<u64>(
            sizeof(GpuLightRecord),
            (Frame() == nullptr ? 0U : Frame()->local_lights.size()) * sizeof(GpuLightRecord)
        );
        auto lights = context.graph.CreateBuffer(
            {"Forward+ lights", light_bytes, 16, rhi::BufferUsage::Storage | rhi::BufferUsage::CopyDst}
        );
        auto clusters = context.graph.CreateBuffer(
            {"Forward+ cluster grid",
                static_cast<u64>(cluster_count) * sizeof(abi::ClusterGridRecord),
                16,
                rhi::BufferUsage::Storage | rhi::BufferUsage::CopyDst}
        );
        auto indices = context.graph.CreateBuffer(
            {"Forward+ light indices",
                static_cast<u64>(cluster_count) * params.dimensions[3] * sizeof(u32),
                4,
                rhi::BufferUsage::Storage | rhi::BufferUsage::CopyDst}
        );
        const GraphBufferDesc diagnostics_descriptor{"Forward+ overflow diagnostics",
            sizeof(u32),
            4,
            rhi::BufferUsage::Storage | rhi::BufferUsage::CopyDst | rhi::BufferUsage::CopySrc};
        if (diagnostics_buffer_ == nullptr) {
            if (services_ == nullptr || services_->device == nullptr)
                return Err(ErrorCode::InvalidState, "lighting diagnostics require a device");
            TRY_ASSIGN(
                diagnostics_buffer_,
                services_->device->CreateBuffer(
                    {.size = sizeof(u32), .usage = diagnostics_descriptor.usage, .label = diagnostics_descriptor.label}
                )
            );
        }
        auto diagnostics = context.graph.ImportBuffer(
            {.descriptor = diagnostics_descriptor,
                .initial_state = ExternalState::Undefined,
                .final_state = ExternalState::CopySource,
                .frame_bound = false},
            diagnostics_buffer_
        );
        auto upload = context.graph.AddPass("Upload Forward+ lights", PassKind::Copy);
        const auto light_version = upload.Write(lights, GraphAccess::CopyDestination);
        auto* services = services_;
        upload.Execute([services, light_version](RenderGraphContext& graph) -> Result<void> {
            const auto light_buffer = graph.Buffer(light_version);
            if (!light_buffer)
                return Err(ErrorCode::ValidationInvalidState, "Forward+ light buffer is unavailable");
            const auto& lights_data = services->frame->local_lights;
            if (!lights_data.empty())
                TRY_VOID(graph.CommandEncoder().WriteBuffer(
                    light_buffer->get(),
                    0,
                    reinterpret_cast<const u8*>(lights_data.data()),
                    lights_data.size() * sizeof(GpuLightRecord)
                ));
            return Ok();
        });
        const bool compute = services_ != nullptr && services_->capabilities != nullptr
                             && std::ranges::find(*services_->capabilities, StringId("compute"))
                                    != services_->capabilities->end();
        auto build = context.graph.AddPass(
            compute ? "Assign Forward+ lights (compute)" : "Assign Forward+ lights (CPU)",
            compute ? PassKind::Compute : PassKind::Copy
        );
        build.Read(light_version, GraphAccess::StorageRead);
        const auto cluster_version = build.Write(
            clusters,
            compute ? GraphAccess::StorageWrite : GraphAccess::CopyDestination
        );
        const auto index_version = build.Write(
            indices,
            compute ? GraphAccess::StorageWrite : GraphAccess::CopyDestination
        );
        const auto diagnostics_version = build.Write(
            diagnostics,
            compute ? GraphAccess::StorageWrite : GraphAccess::CopyDestination
        );
        build.Queue(QueuePreference::Graphics, false);
        build.Execute(
            [this,
                services,
                light_version,
                cluster_version,
                index_version,
                diagnostics_version,
                params,
                cluster_count,
                compute](RenderGraphContext& graph) -> Result<void> {
                auto light_buffer = graph.Buffer(light_version);
                auto cluster_buffer = graph.Buffer(cluster_version);
                auto index_buffer = graph.Buffer(index_version);
                auto diagnostic_buffer = graph.Buffer(diagnostics_version);
                if (!light_buffer || !cluster_buffer || !index_buffer || !diagnostic_buffer)
                    return Err(ErrorCode::ValidationInvalidState, "Forward+ graph buffers are unavailable");
                if (compute)
                    return DispatchClusters(
                        graph,
                        light_buffer->get(),
                        cluster_buffer->get(),
                        index_buffer->get(),
                        diagnostic_buffer->get(),
                        params,
                        cluster_count,
                        static_cast<u32>(services->frame->local_lights.size())
                    );
                return BuildClustersCpu(
                    graph,
                    cluster_buffer->get(),
                    index_buffer->get(),
                    diagnostic_buffer->get(),
                    params,
                    cluster_count
                );
            }
        );
        auto result = context.blackboard.Emplace<ClusteredLightingOutput>(ClusteredLightingOutput{lights,
            light_version,
            clusters,
            cluster_version,
            indices,
            index_version,
            diagnostics,
            diagnostics_version,
            params,
            Frame() == nullptr ? 0U : Frame()->directional_count,
            Frame() == nullptr ? 0U : static_cast<u32>(Frame()->local_lights.size())});
        return result ? Ok() : Err(std::move(result).error());
    }

private:
    Result<void> BuildClustersCpu(
        RenderGraphContext& graph,
        rhi::Buffer& clusters,
        rhi::Buffer& indices,
        rhi::Buffer& diagnostics,
        const abi::ClusterParams& params,
        const u32 cluster_count
    ) const {
        std::vector<abi::ClusterGridRecord> grid(cluster_count);
        std::vector<u32> packed(static_cast<size_t>(cluster_count) * params.dimensions[3]);
        u32 overflow{};
        const auto& lights = Frame()->local_lights;
        for (u32 z = 0; z < params.dimensions[2]; ++z) {
            const f32 z0 = params.depth[0]
                           * std::exp(params.depth[2] * static_cast<f32>(z) / static_cast<f32>(params.dimensions[2]));
            const f32 z1 = params.depth[0]
                           * std::exp(
                               params.depth[2] * static_cast<f32>(z + 1) / static_cast<f32>(params.dimensions[2])
                           );
            for (u32 y = 0; y < params.dimensions[1]; ++y)
                for (u32 x = 0; x < params.dimensions[0]; ++x) {
                    const u32 cell = (z * params.dimensions[1] + y) * params.dimensions[0] + x;
                    auto& record = grid[cell];
                    record.offset = cell * params.dimensions[3];
                    const f32 min_x = 2.0F * static_cast<f32>(x) / static_cast<f32>(params.dimensions[0]) - 1.0F;
                    const f32 max_x = 2.0F * static_cast<f32>(x + 1) / static_cast<f32>(params.dimensions[0]) - 1.0F;
                    const f32 min_y = 2.0F * static_cast<f32>(y) / static_cast<f32>(params.dimensions[1]) - 1.0F;
                    const f32 max_y = 2.0F * static_cast<f32>(y + 1) / static_cast<f32>(params.dimensions[1]) - 1.0F;
                    for (u32 light_index = 0; light_index < lights.size(); ++light_index) {
                        const auto& light = lights[light_index];
                        const math::vec3f p{light.position_range[0], light.position_range[1], light.position_range[2]};
                        const f32 vx = params.view_row_0[0] * p.x + params.view_row_0[1] * p.y
                                       + params.view_row_0[2] * p.z + params.view_row_0[3];
                        const f32 vy = params.view_row_1[0] * p.x + params.view_row_1[1] * p.y
                                       + params.view_row_1[2] * p.z + params.view_row_1[3];
                        const f32 vz = -(
                            params.view_row_2[0] * p.x + params.view_row_2[1] * p.y + params.view_row_2[2] * p.z
                            + params.view_row_2[3]
                        );
                        const f32 radius = light.position_range[3];
                        if (vz + radius < z0 || vz - radius > z1 || vz + radius <= 0.0F)
                            continue;
                        const f32 ndc_x = vx * params.projection[0] / std::max(vz, 0.001F);
                        const f32 ndc_y = vy * params.projection[1] / std::max(vz, 0.001F);
                        const f32 radius_x = radius * params.projection[0] / std::max(vz - radius, 0.001F);
                        const f32 radius_y = radius * params.projection[1] / std::max(vz - radius, 0.001F);
                        if (ndc_x + radius_x < min_x || ndc_x - radius_x > max_x || ndc_y + radius_y < min_y
                            || ndc_y - radius_y > max_y)
                            continue;
                        if (record.count < params.dimensions[3])
                            packed[record.offset + record.count++] = light_index;
                        else {
                            ++record.overflow;
                            ++overflow;
                        }
                    }
                }
        }
        TRY_VOID(
            graph.CommandEncoder()
                .WriteBuffer(clusters, 0, reinterpret_cast<const u8*>(grid.data()), grid.size() * sizeof(grid.front()))
        );
        TRY_VOID(graph.CommandEncoder().WriteBuffer(
            indices,
            0,
            reinterpret_cast<const u8*>(packed.data()),
            packed.size() * sizeof(packed.front())
        ));
        return graph.CommandEncoder()
            .WriteBuffer(diagnostics, 0, reinterpret_cast<const u8*>(&overflow), sizeof(overflow));
    }

    Result<void> DispatchClusters(
        RenderGraphContext& graph,
        rhi::Buffer& lights,
        rhi::Buffer& clusters,
        rhi::Buffer& indices,
        rhi::Buffer& diagnostics,
        abi::ClusterParams params,
        u32 cluster_count,
        u32 light_count
    ) const;

    mutable ref<rhi::Buffer> diagnostics_buffer_;
    mutable ReadbackTicket pending_readback_;
    mutable rhi::SubmissionTicket diagnostics_last_use_;
};

Result<void> ClusteredLightingFeature::DispatchClusters(
    RenderGraphContext& graph,
    rhi::Buffer& lights,
    rhi::Buffer& clusters,
    rhi::Buffer& indices,
    rhi::Buffer& diagnostics,
    abi::ClusterParams params,
    const u32 cluster_count,
    const u32 light_count
) const {
    if (services_ == nullptr || services_->programs == nullptr)
        return Err(ErrorCode::InvalidState, "cooked cluster program generation is unavailable");
    return services_->programs
        ->DispatchClusters(graph, lights, clusters, indices, diagnostics, params, cluster_count, light_count);
}

class OpaqueForwardFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;

    Result<void> DeclareGraph(GraphDeclarationContext& context) const override {
        const auto* target = context.blackboard.Get<RenderTargetOutput>();
        const auto* depth = context.blackboard.Get<DepthFeatureOutput>();
        const auto* lighting = context.blackboard.Get<ClusteredLightingOutput>();
        const auto* shadows = context.blackboard.Get<ShadowFeatureOutput>();
        const auto* gpu_visibility = context.blackboard.Get<GpuVisibilityOutput>();
        const IndirectDrawStream* indirect = gpu_visibility != nullptr && gpu_visibility->stream.ready
                                                 ? &gpu_visibility->stream
                                                 : nullptr;
        if (target == nullptr || depth == nullptr || context.blackboard.Get<MeshFeatureOutput>() == nullptr)
            return Err(ErrorCode::ValidationInvalidState, "opaque forward feature inputs are incomplete");
        GraphTexture color = target->color;
        bool hdr = !target->direct;
        if (hdr) {
            auto descriptor = TextureDescriptor("HDR scene color", rhi::TextureFormat::RGBA16Float, target->samples);
            descriptor.extent = GraphExtent::Fixed(context.width, context.height);
            color = context.graph.CreateTexture(descriptor);
        }
        auto pass = context.graph.AddPass("Opaque Forward+", PassKind::Render);
        if (indirect != nullptr)
            pass.Read(indirect->commands_version, GraphAccess::Indirect)
                .Read(indirect->counts_version, GraphAccess::Indirect);
        if (lighting != nullptr) {
            pass.Read(lighting->lights_version, GraphAccess::StorageRead)
                .Read(lighting->clusters_version, GraphAccess::StorageRead)
                .Read(lighting->indices_version, GraphAccess::StorageRead);
        }
        if (shadows != nullptr) {
            GraphTextureViewDesc shadow_view;
            shadow_view.subresources = {.base_mip_level = 0,
                .mip_level_count = 1,
                .base_array_layer = 0,
                .array_layer_count = shadows->cascades,
                .aspect = rhi::TextureAspect::DepthOnly};
            shadow_view.format = rhi::TextureFormat::Depth32Float;
            shadow_view.dimension = rhi::TextureViewDimension::e2DArray;
            shadow_view.usage = rhi::TextureUsage::TextureBinding;
            shadow_view.label = "Directional shadow atlas sampling view";
            pass.Read(shadows->version, GraphAccess::Sampled, shadow_view.subresources, std::move(shadow_view));
        }
        const auto version = pass.Color(
            color,
            ColorLoad(
                hdr || (Frame() != nullptr && Frame()->clear_output) ? rhi::LoadOp::Clear : rhi::LoadOp::Load,
                {0.12, 0.16, 0.22, 1.0}
            )
        );
        static_cast<void>(pass.Depth(depth->depth, DepthLoad(rhi::LoadOp::Load, false)));
        auto* services = services_;
        const auto lights = lighting == nullptr ? GraphBufferRef{} : lighting->lights_version;
        const auto clusters = lighting == nullptr ? GraphBufferRef{} : lighting->clusters_version;
        const auto indices = lighting == nullptr ? GraphBufferRef{} : lighting->indices_version;
        const auto params = lighting == nullptr ? abi::ClusterParams{} : lighting->params;
        const auto shadow_version = shadows == nullptr ? GraphTextureRef{} : shadows->version;
        const auto shadow_matrices = shadows == nullptr ? std::array<math::mat4f, 4>{} : shadows->cascade_matrices;
        const auto shadow_splits = shadows == nullptr ? std::array<f32, 4>{} : shadows->split_depths;
        const u32 shadow_count = shadows == nullptr ? 0U : shadows->cascades;
        pass.Execute(
            [services,
                lights,
                clusters,
                indices,
                params,
                shadow_version,
                shadow_matrices,
                shadow_splits,
                shadow_count,
                indirect](RenderGraphContext& graph) -> Result<void> {
                const auto& camera = services->frame->view->camera;
                TRY_VOID(services->bindings->UseViewProjection(
                    graph.CommandEncoder(),
                    JitteredViewProjection(camera.view_projection, services->frame->view->jitter),
                    camera.inverse_view_projection,
                    camera.position,
                    camera.near_plane,
                    camera.far_plane,
                    static_cast<f32>(services->frame->view->viewport.width),
                    static_cast<f32>(services->frame->view->viewport.height)
                ));
                if (lights && clusters && indices) {
                    auto light_buffer = graph.Buffer(lights);
                    auto cluster_buffer = graph.Buffer(clusters);
                    auto index_buffer = graph.Buffer(indices);
                    if (!light_buffer || !cluster_buffer || !index_buffer)
                        return Err(ErrorCode::ValidationInvalidState, "Forward+ draw buffers are unavailable");
                    TRY_VOID(services->bindings->UseClusteredLighting(
                        graph.CommandEncoder(),
                        light_buffer->get(),
                        cluster_buffer->get(),
                        index_buffer->get(),
                        params
                    ));
                }
                if (shadow_version) {
                    GraphTextureViewDesc descriptor;
                    descriptor.subresources = {.base_mip_level = 0,
                        .mip_level_count = 1,
                        .base_array_layer = 0,
                        .array_layer_count = shadow_count,
                        .aspect = rhi::TextureAspect::DepthOnly};
                    descriptor.format = rhi::TextureFormat::Depth32Float;
                    descriptor.dimension = rhi::TextureViewDimension::e2DArray;
                    descriptor.usage = rhi::TextureUsage::TextureBinding;
                    descriptor.label = "Directional shadow atlas sampling view";
                    auto atlas = graph.TextureView(shadow_version, descriptor);
                    if (!atlas)
                        return Err(std::move(atlas).error());
                    abi::ShadowData data;
                    for (u32 cascade = 0; cascade < shadow_count; ++cascade)
                        std::memcpy(data.matrices[cascade].data(), &shadow_matrices[cascade], sizeof(math::mat4f));
                    data.split_depths = shadow_splits;
                    const auto extent = graph.Extent(shadow_version);
                    data.atlas_size_count = {1.0F / static_cast<f32>(extent.width),
                        1.0F / static_cast<f32>(extent.height),
                        static_cast<f32>(shadow_count),
                        0.0F};
                    TRY_VOID(services->bindings->UseShadows(graph.CommandEncoder(), atlas->get(), data));
                }
                for (const RenderPhase phase : {RenderPhase::Opaque, RenderPhase::AlphaTest}) {
                    bool gpu_drawn{};
                    if (indirect != nullptr)
                        TRY_ASSIGN(
                            gpu_drawn,
                            DrawIndirect(
                                services,
                                graph,
                                *indirect,
                                phase,
                                MaterialPass::Forward,
                                services->frame->targets
                            )
                        );
                    if (!gpu_drawn) {
                        if (indirect != nullptr)
                            services->frame->gpu_visibility.fallback = GpuDrivenFallbackReason::PipelinePending;
                        TRY_VOID(Draw(services, graph, phase, MaterialPass::Forward, services->frame->targets));
                    }
                }
                return Ok();
            }
        );
        auto result = context.blackboard.Emplace<SceneColorOutput>(SceneColorOutput{color, version, hdr});
        return result ? Ok() : Err(std::move(result).error());
    }
};

class TransparentFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;

    Result<void> DeclareGraph(GraphDeclarationContext& context) const override {
        auto* color = context.blackboard.Get<SceneColorOutput>();
        const auto* depth = context.blackboard.Get<DepthFeatureOutput>();
        const auto* lighting = context.blackboard.Get<ClusteredLightingOutput>();
        const auto* shadows = context.blackboard.Get<ShadowFeatureOutput>();
        if (color == nullptr || depth == nullptr)
            return Err(ErrorCode::ValidationInvalidState, "transparent feature inputs are incomplete");
        auto pass = context.graph.AddPass("Transparent forward", PassKind::Render);
        if (lighting != nullptr)
            pass.Read(lighting->lights_version, GraphAccess::StorageRead)
                .Read(lighting->clusters_version, GraphAccess::StorageRead)
                .Read(lighting->indices_version, GraphAccess::StorageRead);
        if (shadows != nullptr) {
            GraphTextureViewDesc shadow_view;
            shadow_view.subresources = {.base_mip_level = 0,
                .mip_level_count = 1,
                .base_array_layer = 0,
                .array_layer_count = shadows->cascades,
                .aspect = rhi::TextureAspect::DepthOnly};
            shadow_view.format = rhi::TextureFormat::Depth32Float;
            shadow_view.dimension = rhi::TextureViewDimension::e2DArray;
            shadow_view.usage = rhi::TextureUsage::TextureBinding;
            pass.Read(shadows->version, GraphAccess::Sampled, shadow_view.subresources, std::move(shadow_view));
        }
        color->version = pass.Color(color->color, ColorLoad(rhi::LoadOp::Load));
        static_cast<void>(pass.Depth(depth->depth, DepthLoad(rhi::LoadOp::Load, true)));
        auto* services = services_;
        const auto lights = lighting == nullptr ? GraphBufferRef{} : lighting->lights_version;
        const auto clusters = lighting == nullptr ? GraphBufferRef{} : lighting->clusters_version;
        const auto indices = lighting == nullptr ? GraphBufferRef{} : lighting->indices_version;
        const auto params = lighting == nullptr ? abi::ClusterParams{} : lighting->params;
        pass.Execute([services, lights, clusters, indices, params](RenderGraphContext& graph) -> Result<void> {
            if (lights && clusters && indices) {
                auto light_buffer = graph.Buffer(lights);
                auto cluster_buffer = graph.Buffer(clusters);
                auto index_buffer = graph.Buffer(indices);
                if (!light_buffer || !cluster_buffer || !index_buffer)
                    return Err(ErrorCode::ValidationInvalidState, "transparent Forward+ buffers are unavailable");
                TRY_VOID(services->bindings->UseClusteredLighting(
                    graph.CommandEncoder(),
                    light_buffer->get(),
                    cluster_buffer->get(),
                    index_buffer->get(),
                    params
                ));
            }
            return Draw(services, graph, RenderPhase::Transparent, MaterialPass::Forward, services->frame->targets);
        });
        return Ok();
    }
};

} // namespace

Result<void> RegisterLightingFeatures(FeatureRegistry& registry) {
    auto velocity = Metadata("velocity", FeatureScope::View, {StringId("mesh"), StringId("depth")});
    velocity.required_capabilities = {StringId("hdr")};
    velocity.insertion_points = {StringId("opaque")};
    auto lighting = Metadata(
        "lighting",
        FeatureScope::ViewFamily,
        {StringId("mesh")},
        {{"clustersX", ConfigType::Integer, false, ConfigValue{i64{16}}},
            {"clustersY", ConfigType::Integer, false, ConfigValue{i64{9}}},
            {"clustersZ", ConfigType::Integer, false, ConfigValue{i64{24}}},
            {"maxLights", ConfigType::Integer, false, ConfigValue{i64{64}}}}
    );
    auto forward = Metadata(
        "forward",
        FeatureScope::Pipeline,
        {StringId("mesh"), StringId("depth")},
        {{"shader", ConfigType::String, false, ConfigValue{std::string("shaders/descriptors/pbr.woki-shader")}}}
    );
    auto transparent = Metadata("transparent", FeatureScope::View, {StringId("forward")});
    transparent.insertion_points = {StringId("transparent")};
    TRY_VOID(RegisterFactory(
        registry,
        std::move(velocity),
        [](CompiledFeatureConfig config, StandardFeatureServices* services) {
            return createRef<VelocityFeature>(std::move(config), services);
        }
    ));
    TRY_VOID(RegisterFactory(
        registry,
        std::move(lighting),
        [](CompiledFeatureConfig config, StandardFeatureServices* services) {
            return createRef<ClusteredLightingFeature>(std::move(config), services);
        }
    ));
    TRY_VOID(RegisterFactory(
        registry,
        std::move(forward),
        [](CompiledFeatureConfig config, StandardFeatureServices* services) {
            return createRef<OpaqueForwardFeature>(std::move(config), services);
        }
    ));
    return RegisterFactory(
        registry,
        std::move(transparent),
        [](CompiledFeatureConfig config, StandardFeatureServices* services) {
            return createRef<TransparentFeature>(std::move(config), services);
        }
    );
}

} // namespace woki::gfx::feature_detail
