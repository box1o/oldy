#include "internal.hpp"

namespace woki::gfx::feature_detail {
namespace {

class MeshFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;

    Result<void> PrepareScene(const RenderScenePreparationContext& context) const override {
        auto* frame = Frame();
        if (frame == nullptr || services_->visibility == nullptr || services_->meshes == nullptr
            || services_->materials == nullptr)
            return Err(ErrorCode::InvalidState, "mesh feature services are unavailable");
        frame->draws = {};
        frame->visible_objects = 0;
        frame->packet_count = 0;
        if (frame->view == nullptr)
            return Err(ErrorCode::InvalidState, "mesh feature has no active view");
        VisibilityResult visibility;
        TRY_ASSIGN(visibility, services_->visibility->Cull(context.world, *frame->view, {}, services_->scheduler));
        frame->visible_objects = visibility.visible.size();
        const auto& objects = context.world.ObjectData();
        for (size_t phase_index = 0; phase_index < visibility.phases.size(); ++phase_index)
            for (const auto& visible : visibility.phases[phase_index]) {
                const u32 index = visible.dense_index;
                const auto* product = services_->meshes->Product(objects.meshes[index]);
                const auto* resident = services_->meshes->Resident(objects.meshes[index]);
                const auto material = services_->materials->Resolve(objects.materials[index]);
                if (product == nullptr || resident == nullptr || visible.lod >= product->lods.size()
                    || visible.lod >= resident->vertices.size()) {
                    frame->draws.phases[phase_index].push_back(
                        {.packet = {},
                            .mesh = objects.meshes[index],
                            .material_instance = objects.materials[index],
                            .object_index = index,
                            .lod = visible.lod}
                    );
                    ++frame->skipped;
                    ++frame->packet_count;
                    continue;
                }
                if (!material
                    && (services_->material_library == nullptr || product->generated_material_instances.empty())) {
                    if (product->diagnostics.empty()) {
                        Diagnostic(
                            services_,
                            "RND2013",
                            "mesh has no cooked material slot; diagnostic fallback requires an explicit "
                            "unsupported/failed import diagnostic",
                            DiagnosticSeverity::Error
                        );
                        ++frame->skipped;
                        continue;
                    }
                    for (const auto& diagnostic : product->diagnostics)
                        Diagnostic(services_, diagnostic.code, diagnostic.message, diagnostic.severity);
                    for (const auto& submesh : product->lods[visible.lod].submeshes) {
                        DrawPacket packet{{},
                            {},
                            services_->meshes->PacketHandle(objects.meshes[index]),
                            objects.palettes[index],
                            submesh.first_index,
                            submesh.index_count,
                            submesh.vertex_offset,
                            index,
                            (static_cast<u64>(visible.source_index) << 32U) | submesh.first_index};
                        frame->draws.phases[phase_index].push_back(
                            {std::move(packet), objects.meshes[index], objects.materials[index], index, visible.lod}
                        );
                        ++frame->packet_count;
                    }
                    continue;
                }
                for (const auto& submesh : product->lods[visible.lod].submeshes) {
                    auto resolved_material = material;
                    MaterialInstanceHandle instance = objects.materials[index];
                    bool material_failed{};
                    if (!resolved_material && submesh.material_slot < product->generated_material_instances.size()) {
                        auto generated = services_->material_library->RequestInstance(product
                                ->generated_material_instances[submesh.material_slot]);
                        if (generated) {
                            instance = *generated;
                            resolved_material = services_->materials->Resolve(instance);
                            const auto* record = services_->material_library->TryGet(instance);
                            material_failed = record != nullptr && !record->diagnostic.empty();
                            if (material_failed)
                                Diagnostic(services_, "RND2014", record->diagnostic);
                        } else {
                            material_failed = true;
                            Diagnostic(services_, "RND2014", std::string(generated.error().Message()));
                        }
                    }
                    if (!resolved_material) {
                        if (!material_failed) {
                            ++frame->skipped;
                            continue;
                        }
                        DrawPacket packet{{},
                            {},
                            services_->meshes->PacketHandle(objects.meshes[index]),
                            objects.palettes[index],
                            submesh.first_index,
                            submesh.index_count,
                            submesh.vertex_offset,
                            index,
                            (static_cast<u64>(visible.source_index) << 32U) | submesh.first_index};
                        frame->draws.phases[phase_index]
                            .push_back({std::move(packet), objects.meshes[index], instance, index, visible.lod});
                        ++frame->packet_count;
                        continue;
                    }
                    auto forward = services_->materials->PipelineRequest(
                        *resolved_material,
                        MaterialPass::Forward,
                        frame->targets,
                        product->schema.id,
                        objects.palettes[index].IsValid()
                    );
                    if (!forward) {
                        ++frame->skipped;
                        continue;
                    }
                    DrawPacket packet{forward->pipeline,
                        *resolved_material,
                        services_->meshes->PacketHandle(objects.meshes[index]),
                        objects.palettes[index],
                        submesh.first_index,
                        submesh.index_count,
                        submesh.vertex_offset,
                        index,
                        (static_cast<u64>(visible.source_index) << 32U) | submesh.first_index};
                    frame->draws.phases[phase_index]
                        .push_back({std::move(packet), objects.meshes[index], instance, index, visible.lod});
                    ++frame->packet_count;
                }
            }
        for (size_t phase_index = 0; phase_index < frame->draws.phases.size(); ++phase_index) {
            TRY_VOID(services_->bindings->PrepareDiagnostic(frame->targets));
            const auto phase = static_cast<RenderPhase>(phase_index);
            const MaterialPass pass = phase == RenderPhase::Depth    ? MaterialPass::Depth
                                      : phase == RenderPhase::Shadow ? MaterialPass::Shadow
                                                                     : MaterialPass::Forward;
            const PipelineTargetSignature targets = phase == RenderPhase::Depth ? PipelineTargetSignature{{},
                                                                                      frame->targets.depth,
                                                                                      frame->targets.samples}
                                                    : phase == RenderPhase::Shadow
                                                        ? PipelineTargetSignature{{},
                                                              rhi::TextureFormat::Depth32Float,
                                                              1}
                                                        : frame->targets;
            for (const auto& draw : frame->draws.phases[phase_index]) {
                const auto* product = services_->meshes->Product(draw.mesh);
                const auto* resident = services_->meshes->Resident(draw.mesh);
                if (product != nullptr && resident != nullptr)
                    TRY_VOID(services_->bindings->PrepareFallback(targets, product->schema, draw.packet.palette));
                if (product == nullptr || resident == nullptr || !draw.packet.material.IsValid())
                    continue;
                PreparedMaterial prepared;
                TRY_ASSIGN(
                    prepared,
                    services_->materials->PipelineRequest(
                        draw.packet.material,
                        pass,
                        targets,
                        product->schema.id,
                        draw.packet.palette.IsValid()
                    )
                );
                BorrowedLayout layout;
                TRY_ASSIGN(layout, services_->bindings->PrepareLayout(prepared.layout, draw.packet.palette));
                auto request = services_->pipelines->Request(
                    prepared.pipeline,
                    [services = services_,
                        key = prepared.pipeline,
                        layout,
                        schema = product->schema,
                        state = prepared.render_state]() {
                        if (!services->create_graphics_pipeline)
                            return Result<ref<rhi::RenderPipeline>>(
                                Err(ErrorCode::InvalidState, "no reflected graphics pipeline factory was supplied")
                            );
                        return services->create_graphics_pipeline(key, layout.Pipeline(), schema, state);
                    }
                );
                if (!request || request->state != PipelineRequestState::Ready)
                    return Err(
                        ErrorCode::GraphicsResourceCreationFailed,
                        request ? request->diagnostic : std::string(request.error().Message())
                    );
            }
        }
        return Ok();
    }

    Result<void> DeclareGraph(GraphDeclarationContext& context) const override {
        auto result = context.blackboard
                          .Emplace<MeshFeatureOutput>(Frame() == nullptr ? MeshFeatureOutput{} : Frame()->draws);
        return result ? Ok() : Err(std::move(result).error());
    }
};

class GpuVisibilityFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;

    ~GpuVisibilityFeature() override {
        if (services_ == nullptr || services_->releases == nullptr)
            return;
        if (diagnostics_ != nullptr)
            services_->releases->Retire(std::move(diagnostics_), last_use_);
        if (visible_instances_ != nullptr)
            services_->releases->Retire(std::move(visible_instances_), last_use_);
    }

    Result<void> PrepareScene(const RenderScenePreparationContext&) const override {
        auto* frame = Frame();
        if (frame == nullptr)
            return Err(ErrorCode::InvalidState, "GPU visibility has no frame state");
        frame->gpu_visibility = {};
        frame->gpu_visibility.fallback = fallback_;
        if (diagnostics_readback_.IsValid() && services_->readbacks != nullptr
            && services_->readbacks->State(diagnostics_readback_) == ReadbackState::Ready) {
            auto bytes = services_->readbacks->Take(diagnostics_readback_);
            if (bytes && bytes->size() >= sizeof(GpuVisibilityDiagnostics)) {
                GpuVisibilityDiagnostics values;
                std::memcpy(&values, bytes->data(), sizeof(values));
                frame->gpu_visibility.candidates = values.counters[0];
                frame->gpu_visibility.layer_culled = values.counters[1];
                frame->gpu_visibility.frustum_culled = values.counters[2];
                frame->gpu_visibility.occlusion_culled = values.counters[3];
                frame->gpu_visibility.visible_objects = values.counters[4];
                frame->gpu_visibility.lod_changes = values.counters[5];
                frame->gpu_visibility.meshlets_tested = values.counters[6];
                frame->gpu_visibility.meshlets_cone_culled = values.counters[7];
                frame->gpu_visibility.meshlets_bounds_culled = values.counters[8];
                frame->gpu_visibility.indirect_commands = values.counters[9];
                frame->gpu_visibility.command_overflow = values.counters[10];
                frame->gpu_visibility.visible_meshlets = values.counters[11];
                frame->gpu_visibility.meshlet_overflow = values.counters[12];
                if (values.counters[10] != 0) {
                    fallback_ = GpuDrivenFallbackReason::CapacityOverflow;
                    Diagnostic(
                        services_,
                        "RND3101",
                        "GPU visibility capacity overflow; deterministic CPU packets selected"
                    );
                }
            }
            diagnostics_readback_ = {};
        }
        if (visible_readback_.IsValid() && services_->readbacks != nullptr
            && services_->readbacks->State(visible_readback_) == ReadbackState::Ready) {
            auto bytes = services_->readbacks->Take(visible_readback_);
            if (bytes) {
                std::vector<u64> gpu(bytes->size() / (sizeof(u32) * 2U));
                for (size_t index = 0; index < gpu.size(); ++index) {
                    std::array<u32, 2> record{};
                    std::memcpy(record.data(), bytes->data() + index * sizeof(record), sizeof(record));
                    gpu[index] = (static_cast<u64>(record[0]) << 32U) | record[1];
                }
                std::ranges::sort(gpu);
                gpu.erase(std::unique(gpu.begin(), gpu.end()), gpu.end());
                for (const u64 expected : pending_expected_)
                    if (!std::ranges::binary_search(gpu, expected))
                        Diagnostic(
                            services_,
                            "RND3102",
                            "GPU visibility false negative for dense object/LOD " + std::to_string(expected >> 32U)
                                + "/" + std::to_string(static_cast<u32>(expected)),
                            DiagnosticSeverity::Error
                        );
            }
            visible_readback_ = {};
            pending_expected_.clear();
        }
        return Ok();
    }

    void OnSubmitted(const rhi::SubmissionTicket submission) const override {
        last_use_ = std::max(last_use_, submission);
        if (services_ == nullptr || services_->readbacks == nullptr)
            return;
        if (!diagnostics_readback_.IsValid() && diagnostics_ != nullptr) {
            auto queued = services_->readbacks->Enqueue({diagnostics_, 0, sizeof(GpuVisibilityDiagnostics)});
            if (queued)
                diagnostics_readback_ = *queued;
        }
        if (validation_enabled_ && !visible_readback_.IsValid() && visible_instances_ != nullptr
            && validation_candidate_count_ != 0) {
            auto queued = services_->readbacks
                              ->Enqueue({visible_instances_, 0, validation_candidate_count_ * sizeof(u32) * 2U});
            if (queued) {
                visible_readback_ = *queued;
                pending_expected_ = expected_;
            }
        }
    }

    Result<void> DeclareGraph(GraphDeclarationContext& context) const override {
        fallback_ = CapabilityFallback();
        if (fallback_ == GpuDrivenFallbackReason::None && Frame()->targets.samples != 1)
            fallback_ = GpuDrivenFallbackReason::ProgramsUnavailable;
        const HiZDepthPyramidOutput* hiz = context.blackboard.Get<HiZDepthPyramidOutput>();
        if (fallback_ == GpuDrivenFallbackReason::None && hiz == nullptr) {
            const u32 mip_count = 1U
                                  + static_cast<u32>(std::floor(
                                      std::log2(static_cast<f32>(std::max(1U, std::max(context.width, context.height))))
                                  ));
            GraphTextureDesc descriptor;
            descriptor.label = "Persistent temporal Hi-Z depth pyramid";
            descriptor.extent = GraphExtent::Fixed(context.width, context.height);
            descriptor.mip_levels = mip_count;
            descriptor.format = rhi::TextureFormat::R32Float;
            descriptor.usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::StorageBinding;
            auto binding = services_->histories->Acquire(
                {Frame()->view->history, TemporalSemantic::HiZDepth},
                descriptor,
                Frame()->frame_number,
                Frame()->view->HistoryInvalid()
            );
            if (!binding)
                fallback_ = GpuDrivenFallbackReason::ProgramsUnavailable;
            else {
                const auto history = context.graph.ImportTemporal(
                    {.previous = binding->previous, .current = binding->current, .descriptor = descriptor}
                );
                auto inserted = context.blackboard
                                    .Emplace<HiZDepthPyramidOutput>(HiZDepthPyramidOutput{history.previous,
                                        history.previous_version,
                                        history.current,
                                        {},
                                        mip_count,
                                        binding->valid});
                if (!inserted)
                    return Err(std::move(inserted).error());
                hiz = &inserted->get();
            }
        }
        if (fallback_ == GpuDrivenFallbackReason::None && (hiz == nullptr || !hiz->previous_version))
            fallback_ = GpuDrivenFallbackReason::ProgramsUnavailable;
        if (fallback_ != GpuDrivenFallbackReason::None) {
            ReportFallback();
            auto inserted = context.blackboard.Emplace<GpuVisibilityOutput>(GpuVisibilityOutput{
                .stream = {},
                .warmup = {},
                .hiz_used = false,
                .validation_pending = false,
                .fallback = fallback_,
            });
            Frame()->gpu_visibility.fallback = fallback_;
            return inserted ? Ok() : Err(std::move(inserted).error());
        }

        const u32 command_capacity = Setting("commandCapacity") == nullptr
                                         ? 65'536U
                                         : static_cast<u32>(
                                               std::max<i64>(1, std::get<i64>(*Setting("commandCapacity")))
                                           );
        const u32 meshlet_capacity = Setting("meshletCapacity") == nullptr
                                         ? 1'048'576U
                                         : static_cast<u32>(
                                               std::max<i64>(1, std::get<i64>(*Setting("meshletCapacity")))
                                           );
        validation_enabled_ = Setting("validate") != nullptr && std::get<bool>(*Setting("validate"));

        struct Work final {
            IndirectBinKey key;
            RenderPhase phase{RenderPhase::Opaque};
            GpuVisibilityCandidate candidate;
            std::vector<GpuLodRecord> lods;
            std::vector<MeshletBounds> meshlets;
        };

        std::vector<Work> work;
        const auto& objects = Frame()->world->ObjectData();
        for (const RenderPhase phase :
            {RenderPhase::Depth, RenderPhase::Opaque, RenderPhase::AlphaTest, RenderPhase::Shadow})
            for (const auto& draw : Frame()->draws.phases[static_cast<size_t>(phase)]) {
                const auto* product = services_->meshes->Product(draw.mesh);
                const auto* resident = services_->meshes->Resident(draw.mesh);
                if (product == nullptr || resident == nullptr || resident->vertices.empty() || resident->indices.empty()
                    || !draw.packet.material.IsValid()) {
                    fallback_ = GpuDrivenFallbackReason::MeshStreamsUnavailable;
                    break;
                }
                if (draw.packet.palette.IsValid()) {
                    fallback_ = GpuDrivenFallbackReason::MeshStreamsUnavailable;
                    break;
                }
                const u32 lod_index = std::min<u32>(
                    draw.lod,
                    std::min<u32>(resident->last_resident_lod, static_cast<u32>(product->lods.size() - 1))
                );
                Work item;
                item.key = {draw.packet.pipeline, draw.packet.material, draw.packet.mesh, lod_index};
                item.phase = phase;
                item.candidate.flags[0] = static_cast<u32>(phase);
                const auto& bounds = objects.bounds[draw.object_index];
                item.candidate.sphere = {bounds.center.x, bounds.center.y, bounds.center.z, bounds.radius};
                f32 nearest_depth = 1.0F;
                for (const f32 x : {bounds.minimum.x, bounds.maximum.x})
                    for (const f32 y : {bounds.minimum.y, bounds.maximum.y})
                        for (const f32 z : {bounds.minimum.z, bounds.maximum.z}) {
                            const auto clip = Frame()->view->camera.view_projection * math::vec4f{x, y, z, 1.0F};
                            nearest_depth = clip.w <= 0.0F ? 0.0F : std::min(nearest_depth, clip.z / clip.w);
                        }
                item.candidate.bounds_extent_lod_scale = {std::bit_cast<f32>(lod_index),
                    std::max(0.0F, nearest_depth),
                    std::bit_cast<f32>(draw.object_index),
                    0.5F * std::abs(Frame()->view->camera.projection(1, 1))};
                const u64 layers = objects.layers[draw.object_index];
                item.candidate.masks = {static_cast<u32>(layers),
                    static_cast<u32>(layers >> 32U),
                    static_cast<u32>(Frame()->view->layer_mask),
                    static_cast<u32>(Frame()->view->layer_mask >> 32U)};
                const auto gpu_instance = services_->gpu_scene->Resolve(objects.ids[draw.object_index]);
                if (!gpu_instance) {
                    fallback_ = GpuDrivenFallbackReason::SceneUnavailable;
                    break;
                }
                item.candidate.bin[3] = gpu_instance->Index();
                const auto selected_submesh = std::ranges::find_if(
                    product->lods[lod_index].submeshes,
                    [&](const MeshSubmesh& submesh) {
                        return submesh.first_index == draw.packet.first_index
                               && submesh.index_count == draw.packet.index_count;
                    }
                );
                if (selected_submesh == product->lods[lod_index].submeshes.end()) {
                    fallback_ = GpuDrivenFallbackReason::MeshStreamsUnavailable;
                    break;
                }
                const u32 submesh_index = static_cast<u32>(
                    std::distance(product->lods[lod_index].submeshes.begin(), selected_submesh)
                );
                for (u32 level = resident->first_resident_lod;
                    level <= resident->last_resident_lod && level < product->lods.size();
                    ++level) {
                    if (submesh_index >= product->lods[level].submeshes.size())
                        continue;
                    const auto& submesh = product->lods[level].submeshes[submesh_index];
                    item.lods.push_back(
                        {product->lods[level].geometric_error,
                            submesh.index_count,
                            static_cast<u32>(resident->indices[level].offset / sizeof(u32)) + submesh.first_index,
                            static_cast<i32>(resident->vertices[level].offset / product->schema.streams.front().stride)
                                + submesh.vertex_offset,
                            level}
                    );
                }
                if (item.lods.empty()) {
                    fallback_ = GpuDrivenFallbackReason::MeshStreamsUnavailable;
                    break;
                }
                const auto& transform = objects.transforms[draw.object_index];
                const math::vec3f axis_x{transform(0, 0), transform(1, 0), transform(2, 0)};
                const math::vec3f axis_y{transform(0, 1), transform(1, 1), transform(2, 1)};
                const math::vec3f axis_z{transform(0, 2), transform(1, 2), transform(2, 2)};
                const f32 scale_x = axis_x.length();
                const f32 scale_y = axis_y.length();
                const f32 scale_z = axis_z.length();
                const f32 maximum_scale = std::max({scale_x, scale_y, scale_z});
                const bool uniform_scale = std::max(
                                               {std::abs(scale_x - scale_y),
                                                   std::abs(scale_x - scale_z),
                                                   std::abs(scale_y - scale_z)}
                                           )
                                           <= maximum_scale * 1.0e-4F;
                for (u32 level = resident->first_resident_lod;
                    level <= resident->last_resident_lod && level < product->lods.size();
                    ++level) {
                    if (product->lods[level].meshlets.size == 0)
                        continue;
                    auto decoded = DecodeMeshletStreams(product->Chunk(product->lods[level].meshlets));
                    if (decoded) {
                        const size_t first_meshlet = item.meshlets.size();
                        item.meshlets.insert(item.meshlets.end(), decoded->bounds.begin(), decoded->bounds.end());
                        for (auto& meshlet : std::span(item.meshlets).subspan(first_meshlet)) {
                            const auto center = transform
                                                * math::vec4f{meshlet.sphere[0],
                                                    meshlet.sphere[1],
                                                    meshlet.sphere[2],
                                                    1.0F};
                            meshlet.sphere = {center.x - bounds.center.x,
                                center.y - bounds.center.y,
                                center.z - bounds.center.z,
                                meshlet.sphere[3] * maximum_scale};
                            if (uniform_scale) {
                                const auto cone = transform
                                                  * math::vec4f{meshlet.cone[0],
                                                      meshlet.cone[1],
                                                      meshlet.cone[2],
                                                      0.0F};
                                const auto normalized = math::vec3f{cone.x, cone.y, cone.z}.normalized();
                                meshlet.cone = {normalized.x, normalized.y, normalized.z, meshlet.cone[3]};
                            } else
                                meshlet.cone[3] = 2.0F;
                        }
                    }
                }
                work.push_back(std::move(item));
            }
        if (fallback_ != GpuDrivenFallbackReason::None || work.empty() || work.size() > command_capacity) {
            if (work.size() > command_capacity)
                fallback_ = GpuDrivenFallbackReason::CapacityOverflow;
            ReportFallback();
            auto inserted = context.blackboard.Emplace<GpuVisibilityOutput>(GpuVisibilityOutput{
                .stream = {},
                .warmup = {},
                .hiz_used = false,
                .validation_pending = false,
                .fallback = fallback_,
            });
            return inserted ? Ok() : Err(std::move(inserted).error());
        }
        std::ranges::sort(work, [](const Work& left, const Work& right) {
            if (left.phase != right.phase)
                return left.phase < right.phase;
            if (left.key != right.key)
                return left.key < right.key;
            return left.candidate.bin[3] < right.candidate.bin[3];
        });

        struct UploadData final {
            std::vector<GpuVisibilityCandidate> candidates;
            std::vector<GpuLodRecord> lods;
            std::vector<MeshletBounds> meshlets;
        };

        auto upload_data = std::make_shared<UploadData>();
        std::vector<IndirectBin> bins;
        for (u32 first = 0; first < work.size();) {
            u32 end = first + 1;
            while (end < work.size() && work[end].phase == work[first].phase && work[end].key == work[first].key)
                ++end;
            const u32 bin_index = static_cast<u32>(bins.size());
            bins.push_back({work[first].key, work[first].phase, first, end - first, bin_index, first, end - first});
            for (u32 index = first; index < end; ++index) {
                auto& candidate = work[index].candidate;
                candidate.lod_meshlets[0] = static_cast<u32>(upload_data->lods.size());
                candidate.lod_meshlets[1] = static_cast<u32>(work[index].lods.size());
                candidate.lod_meshlets[2] = static_cast<u32>(upload_data->meshlets.size());
                candidate.lod_meshlets[3] = static_cast<u32>(work[index].meshlets.size());
                candidate.bin[0] = first;
                candidate.bin[1] = end - first;
                candidate.bin[2] = bin_index;
                upload_data->candidates.push_back(candidate);
                upload_data->lods.insert(upload_data->lods.end(), work[index].lods.begin(), work[index].lods.end());
                upload_data->meshlets
                    .insert(upload_data->meshlets.end(), work[index].meshlets.begin(), work[index].meshlets.end());
            }
            first = end;
        }
        if (upload_data->meshlets.size() > meshlet_capacity) {
            fallback_ = GpuDrivenFallbackReason::CapacityOverflow;
            ReportFallback();
            auto inserted = context.blackboard.Emplace<GpuVisibilityOutput>(GpuVisibilityOutput{
                .stream = {},
                .warmup = {},
                .hiz_used = false,
                .validation_pending = false,
                .fallback = fallback_,
            });
            return inserted ? Ok() : Err(std::move(inserted).error());
        }
        expected_.clear();
        for (const auto& item : work)
            expected_.push_back(
                (static_cast<u64>(std::bit_cast<u32>(item.candidate.bounds_extent_lod_scale[2])) << 32U) | item.key.lod
            );
        std::ranges::sort(expected_);
        expected_.erase(std::unique(expected_.begin(), expected_.end()), expected_.end());
        validation_candidate_count_ = upload_data->candidates.size();

        const auto make_buffer = [&](std::string label, const u64 size, const rhi::BufferUsage usage) {
            return context.graph.CreateBuffer({std::move(label), std::max<u64>(size, 16), 16, usage});
        };
        auto candidates = make_buffer(
            "GPU visibility candidates",
            upload_data->candidates.size() * sizeof(GpuVisibilityCandidate),
            rhi::BufferUsage::Storage | rhi::BufferUsage::CopyDst
        );
        auto lods = make_buffer(
            "GPU projected-error LOD records",
            upload_data->lods.size() * sizeof(GpuLodRecord),
            rhi::BufferUsage::Storage | rhi::BufferUsage::CopyDst
        );
        auto meshlet_bounds = make_buffer(
            "GPU meshlet bounds and cones",
            upload_data->meshlets.size() * sizeof(MeshletBounds),
            rhi::BufferUsage::Storage | rhi::BufferUsage::CopyDst
        );
        auto commands = make_buffer(
            "IndirectDrawStream indexed commands",
            command_capacity * sizeof(rhi::DrawIndexedIndirectArguments),
            rhi::BufferUsage::Storage | rhi::BufferUsage::Indirect
        );
        auto counts = make_buffer(
            "IndirectDrawStream bin counts",
            upload_data->candidates.size() * sizeof(u32),
            rhi::BufferUsage::Storage | rhi::BufferUsage::Indirect | rhi::BufferUsage::CopyDst
        );
        const u64 visible_size = upload_data->candidates.size() * sizeof(u32) * 2U;
        if (visible_instances_ == nullptr || visible_capacity_ < visible_size) {
            if (visible_instances_ != nullptr)
                services_->releases->Retire(std::move(visible_instances_), last_use_);
            auto created = services_->device->CreateBuffer(
                {.size = std::max<u64>(visible_size, 16),
                    .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::CopySrc | rhi::BufferUsage::CopyDst,
                    .label = "IndirectDrawStream visible instances readback"}
            );
            if (!created) {
                fallback_ = GpuDrivenFallbackReason::SceneUnavailable;
                ReportFallback();
                auto inserted = context.blackboard.Emplace<GpuVisibilityOutput>(GpuVisibilityOutput{
                    .stream = {},
                    .warmup = {},
                    .hiz_used = false,
                    .validation_pending = false,
                    .fallback = fallback_,
                });
                return inserted ? Ok() : Err(std::move(inserted).error());
            }
            visible_instances_ = ref<rhi::Buffer>(std::move(*created));
            visible_capacity_ = std::max<u64>(visible_size, 16);
        }
        if (diagnostics_ == nullptr) {
            auto created = services_->device->CreateBuffer(
                {.size = sizeof(GpuVisibilityDiagnostics),
                    .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::CopySrc | rhi::BufferUsage::CopyDst,
                    .label = "GPU visibility overflow and culling counters"}
            );
            if (!created) {
                fallback_ = GpuDrivenFallbackReason::SceneUnavailable;
                ReportFallback();
                auto inserted = context.blackboard.Emplace<GpuVisibilityOutput>(GpuVisibilityOutput{
                    .stream = {},
                    .warmup = {},
                    .hiz_used = false,
                    .validation_pending = false,
                    .fallback = fallback_,
                });
                return inserted ? Ok() : Err(std::move(inserted).error());
            }
            diagnostics_ = ref<rhi::Buffer>(std::move(*created));
        }
        auto visible_instances = context.graph.ImportBuffer(
            {{"IndirectDrawStream visible instance list",
                 visible_capacity_,
                 16,
                 rhi::BufferUsage::Storage | rhi::BufferUsage::CopySrc | rhi::BufferUsage::CopyDst},
                ExternalState::Undefined,
                ExternalState::CopySource,
                false},
            visible_instances_
        );
        auto visible_meshlets = make_buffer(
            "IndirectDrawStream visible meshlet list",
            meshlet_capacity * sizeof(u32),
            rhi::BufferUsage::Storage
        );
        auto diagnostics = context.graph.ImportBuffer(
            {{"IndirectDrawStream overflow diagnostics",
                 sizeof(GpuVisibilityDiagnostics),
                 16,
                 rhi::BufferUsage::Storage | rhi::BufferUsage::CopySrc | rhi::BufferUsage::CopyDst},
                ExternalState::Undefined,
                ExternalState::CopySource,
                false},
            diagnostics_
        );

        auto upload = context.graph.AddPass("Upload GPU visibility candidates and immutable bins", PassKind::Copy);
        const auto candidate_version = upload.Write(candidates, GraphAccess::CopyDestination);
        const auto lod_version = upload.Write(lods, GraphAccess::CopyDestination);
        const auto meshlet_version = upload.Write(meshlet_bounds, GraphAccess::CopyDestination);
        upload.Execute(
            [upload_data, candidate_version, lod_version, meshlet_version](RenderGraphContext& graph) -> Result<void> {
                auto candidates_buffer = graph.Buffer(candidate_version);
                auto lod_buffer = graph.Buffer(lod_version);
                auto meshlet_buffer = graph.Buffer(meshlet_version);
                if (!candidates_buffer || !lod_buffer || !meshlet_buffer)
                    return Err(ErrorCode::ValidationInvalidState, "GPU visibility upload buffers are unavailable");
                TRY_VOID(graph.CommandEncoder().WriteBuffer(
                    candidates_buffer->get(),
                    0,
                    reinterpret_cast<const u8*>(upload_data->candidates.data()),
                    upload_data->candidates.size() * sizeof(GpuVisibilityCandidate)
                ));
                TRY_VOID(graph.CommandEncoder().WriteBuffer(
                    lod_buffer->get(),
                    0,
                    reinterpret_cast<const u8*>(upload_data->lods.data()),
                    upload_data->lods.size() * sizeof(GpuLodRecord)
                ));
                if (!upload_data->meshlets.empty())
                    TRY_VOID(graph.CommandEncoder().WriteBuffer(
                        meshlet_buffer->get(),
                        0,
                        reinterpret_cast<const u8*>(upload_data->meshlets.data()),
                        upload_data->meshlets.size() * sizeof(MeshletBounds)
                    ));
                return Ok();
            }
        );
        auto classify = context.graph
                            .AddPass("Cull and classify GPU visibility into IndirectDrawStream", PassKind::Compute);
        classify.Read(candidate_version, GraphAccess::StorageRead)
            .Read(lod_version, GraphAccess::StorageRead)
            .Read(meshlet_version, GraphAccess::StorageRead)
            .Read(hiz->previous_version, GraphAccess::Sampled);
        const auto command_version = classify.Write(commands, GraphAccess::StorageWrite);
        const auto count_version = classify.Write(counts, GraphAccess::StorageWrite);
        const auto visible_version = classify.Write(visible_instances, GraphAccess::StorageWrite);
        const auto visible_meshlet_version = classify.Write(visible_meshlets, GraphAccess::StorageWrite);
        const auto diagnostic_version = classify.Write(diagnostics, GraphAccess::StorageWrite);
        GpuVisibilityParams params;
        const auto frustum = Frustum::FromWebGpuViewProjection(Frame()->view->camera.view_projection);
        for (u32 plane = 0; plane < 6; ++plane)
            params.frustum_planes[plane] = {frustum.Planes()[plane].normal.x,
                frustum.Planes()[plane].normal.y,
                frustum.Planes()[plane].normal.z,
                frustum.Planes()[plane].distance};
        for (u32 row = 0; row < 4; ++row)
            for (u32 column = 0; column < 4; ++column)
                params.view_projection[column][row] = Frame()->view->camera.view_projection(row, column);
        params.camera_position_threshold = {Frame()->view->camera.position.x,
            Frame()->view->camera.position.y,
            Frame()->view->camera.position.z,
            1.0F};
        params.dimensions_flags = {context.width,
            context.height,
            hiz->mip_count,
            hiz->history_valid && !HasFlag(Frame()->view->flags, ViewFlags::DisableOcclusion) && !validation_enabled_
                ? 1U
                : 0U};
        params.counts = {static_cast<u32>(upload_data->candidates.size()),
            command_capacity,
            static_cast<u32>(upload_data->meshlets.size()),
            meshlet_capacity};
        auto* services = services_;
        classify.Queue(QueuePreference::Compute, true)
            .Execute(
                [services,
                    candidate_version,
                    lod_version,
                    meshlet_version,
                    previous = hiz->previous_version,
                    command_version,
                    count_version,
                    visible_version,
                    visible_meshlet_version,
                    diagnostic_version,
                    params](RenderGraphContext& graph) -> Result<void> {
                    auto candidate_buffer = graph.Buffer(candidate_version);
                    auto lod_buffer = graph.Buffer(lod_version);
                    auto meshlet_buffer = graph.Buffer(meshlet_version);
                    auto command_buffer = graph.Buffer(command_version);
                    auto count_buffer = graph.Buffer(count_version);
                    auto visible_buffer = graph.Buffer(visible_version);
                    auto visible_meshlet_buffer = graph.Buffer(visible_meshlet_version);
                    auto diagnostic_buffer = graph.Buffer(diagnostic_version);
                    auto hiz_view = graph.TextureView(previous);
                    if (!candidate_buffer || !lod_buffer || !meshlet_buffer || !command_buffer || !count_buffer
                        || !visible_buffer || !visible_meshlet_buffer || !diagnostic_buffer || !hiz_view)
                        return Err(ErrorCode::ValidationInvalidState, "GPU visibility graph resources are unavailable");
                    return services->programs->DispatchVisibility(
                        graph,
                        candidate_buffer->get(),
                        lod_buffer->get(),
                        meshlet_buffer->get(),
                        hiz_view->get(),
                        command_buffer->get(),
                        count_buffer->get(),
                        visible_buffer->get(),
                        visible_meshlet_buffer->get(),
                        diagnostic_buffer->get(),
                        params
                    );
                }
            );
        fallback_ = GpuDrivenFallbackReason::None;
        Frame()->gpu_visibility.bins = bins.size();
        Frame()->gpu_visibility.fallback = fallback_;
        std::vector<PsoWarmupEntry> reachable;
        reachable.reserve(bins.size() * 2U);
        for (const auto& bin : bins) {
            reachable.push_back({bin.key.pipeline, bin.key.material});
            auto gpu_pipeline = bin.key.pipeline;
            if (gpu_pipeline.vertex_entry == StringId("pbr_static_vs"))
                gpu_pipeline.vertex_entry = StringId("pbr_gpu_vs");
            else if (gpu_pipeline.vertex_entry == StringId("unlit_static_vs"))
                gpu_pipeline.vertex_entry = StringId("unlit_gpu_vs");
            reachable.push_back({std::move(gpu_pipeline), bin.key.material});
        }
        IndirectDrawStream stream{commands,
            command_version,
            counts,
            count_version,
            visible_instances,
            visible_version,
            visible_meshlets,
            visible_meshlet_version,
            diagnostics,
            diagnostic_version,
            {},
            std::move(bins),
            command_capacity,
            meshlet_capacity,
            true};
        auto inserted = context.blackboard.Emplace<GpuVisibilityOutput>(GpuVisibilityOutput{std::move(stream),
            BuildPsoWarmupManifest(reachable),
            params.dimensions_flags[3] != 0,
            validation_enabled_,
            fallback_});
        return inserted ? Ok() : Err(std::move(inserted).error());
    }

private:
    void ReportFallback() const {
        if (fallback_ == GpuDrivenFallbackReason::None || fallback_ == reported_fallback_)
            return;
        Diagnostic(
            services_,
            "RND3100",
            "GPU-driven rendering fallback: " + std::string(GpuDrivenFallbackName(fallback_)),
            DiagnosticSeverity::Note
        );
        reported_fallback_ = fallback_;
    }

    [[nodiscard]] GpuDrivenFallbackReason CapabilityFallback() const noexcept {
        if (services_ == nullptr || services_->device == nullptr || services_->programs == nullptr
            || services_->gpu_scene == nullptr)
            return GpuDrivenFallbackReason::SceneUnavailable;
        const auto& capabilities = services_->device->Capabilities();
        if (!capabilities.Has(rhi::CapabilityFeature::Compute))
            return GpuDrivenFallbackReason::MissingCompute;
        if (!capabilities.Has(rhi::CapabilityFeature::IndirectDraw))
            return GpuDrivenFallbackReason::MissingIndirect;
        if (!capabilities.Has(rhi::CapabilityFeature::IndirectCount)
            || !capabilities.Has(rhi::CapabilityFeature::GpuDrivenIndirect))
            return GpuDrivenFallbackReason::MissingIndirectCount;
        if (!capabilities.Has(rhi::CapabilityFeature::StorageBuffers))
            return GpuDrivenFallbackReason::MissingStorage;
        if (!services_->programs->GpuDrivenReady())
            return GpuDrivenFallbackReason::ProgramsUnavailable;
        if (services_->gpu_scene->Tables().generation == 0)
            return GpuDrivenFallbackReason::SceneUnavailable;
        return GpuDrivenFallbackReason::None;
    }

    mutable ref<rhi::Buffer> diagnostics_;
    mutable ref<rhi::Buffer> visible_instances_;
    mutable u64 visible_capacity_{};
    mutable ReadbackTicket diagnostics_readback_;
    mutable ReadbackTicket visible_readback_;
    mutable std::vector<u64> expected_;
    mutable std::vector<u64> pending_expected_;
    mutable u64 validation_candidate_count_{};
    mutable rhi::SubmissionTicket last_use_;
    mutable GpuDrivenFallbackReason fallback_{GpuDrivenFallbackReason::Disabled};
    mutable GpuDrivenFallbackReason reported_fallback_{GpuDrivenFallbackReason::None};
    mutable bool validation_enabled_{};
};

} // namespace

Result<void> RegisterMeshFeatures(FeatureRegistry& registry) {
    auto mesh = Metadata("mesh", FeatureScope::View);
    auto visibility = Metadata(
        "gpu-visibility",
        FeatureScope::View,
        {StringId("mesh")},
        {{"commandCapacity", ConfigType::Integer, false, ConfigValue{i64{65'536}}},
            {"meshletCapacity", ConfigType::Integer, false, ConfigValue{i64{1'048'576}}},
            {"validate", ConfigType::Boolean, false, ConfigValue{false}}}
    );
    visibility.insertion_points = {StringId("depth-prepass")};
    TRY_VOID(
        RegisterFactory(registry, std::move(mesh), [](CompiledFeatureConfig config, StandardFeatureServices* services) {
            return createRef<MeshFeature>(std::move(config), services);
        })
    );
    return RegisterFactory(
        registry,
        std::move(visibility),
        [](CompiledFeatureConfig config, StandardFeatureServices* services) {
            return createRef<GpuVisibilityFeature>(std::move(config), services);
        }
    );
}

} // namespace woki::gfx::feature_detail
