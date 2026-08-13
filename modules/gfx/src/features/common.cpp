#include "internal.hpp"

namespace woki::gfx {

GraphTextureDesc feature_detail::TextureDescriptor(
    std::string label,
    const rhi::TextureFormat format,
    const u32 samples
) {
    GraphTextureDesc descriptor;
    descriptor.label = std::move(label);
    descriptor.extent = GraphExtent::Relative();
    descriptor.sample_count = samples;
    descriptor.format = format;
    descriptor.usage = rhi::TextureUsage::RenderAttachment | rhi::TextureUsage::TextureBinding;
    return descriptor;
}

ColorAttachment feature_detail::ColorLoad(const rhi::LoadOp load, const rhi::Color clear) {
    ColorAttachment attachment;
    attachment.load = load;
    attachment.clear = clear;
    return attachment;
}

DepthAttachment feature_detail::DepthLoad(const rhi::LoadOp load, const bool read_only, const f32 clear) {
    DepthAttachment attachment;
    attachment.load = load;
    attachment.clear = clear;
    attachment.read_only = read_only;
    return attachment;
}

math::mat4f feature_detail::JitteredViewProjection(const math::mat4f& matrix, const math::vec2f jitter) {
    auto result = matrix;
    for (u32 column = 0; column < 4; ++column) {
        result(0, column) += jitter.x * matrix(3, column);
        result(1, column) += jitter.y * matrix(3, column);
    }
    return result;
}

void feature_detail::Diagnostic(
    StandardFeatureServices* services,
    std::string code,
    std::string message,
    DiagnosticSeverity severity
) {
    if (services == nullptr || services->frame == nullptr || services->frame->diagnostics == nullptr)
        return;
    services->frame->diagnostics->push_back(
        {
            .feature = StringId("renderer"),
            .view = services->frame->view == nullptr ? ViewId{} : services->frame->view->id,
            .severity = severity,
            .code = std::move(code),
            .message = std::move(message),
            .stage = {},
            .pass = {},
            .resource = {},
        }
    );
}

Result<void> feature_detail::Draw(
    StandardFeatureServices* services,
    RenderGraphContext& context,
    const RenderPhase phase,
    const MaterialPass pass,
    PipelineTargetSignature targets,
    const bool apply_view_viewport
) {
    if (services == nullptr || services->frame == nullptr || services->materials == nullptr
        || services->meshes == nullptr || services->pipelines == nullptr)
        return Err(ErrorCode::InvalidState, "standard draw feature services are unavailable");
    auto* encoder = context.RenderEncoder();
    if (encoder == nullptr)
        return Err(ErrorCode::ValidationInvalidState, "draw callback has no render encoder");
    if (apply_view_viewport && services->frame->view != nullptr) {
        const auto viewport = services->frame->view->viewport;
        const u32 width = services->frame->view->render_width == 0 ? viewport.width
                                                                   : services->frame->view->render_width;
        const u32 height = services->frame->view->render_height == 0 ? viewport.height
                                                                     : services->frame->view->render_height;
        const u32 x = services->frame->direct_output ? viewport.x : 0;
        const u32 y = services->frame->direct_output ? viewport.y : 0;
        encoder->SetViewport(
            static_cast<f32>(x),
            static_cast<f32>(y),
            static_cast<f32>(width),
            static_cast<f32>(height),
            0.0F,
            1.0F
        );
        encoder->SetScissorRect(x, y, width, height);
    }
    for (const auto& draw : services->frame->draws.phases[static_cast<size_t>(phase)]) {
        const auto* resident = services->meshes->Resident(draw.mesh);
        const auto* product = services->meshes->Product(draw.mesh);
        const auto diagnostic = [&]() -> Result<void> {
            if (phase != RenderPhase::Opaque && phase != RenderPhase::AlphaTest && phase != RenderPhase::Transparent)
                return Ok();
            if (resident != nullptr && product != nullptr && !resident->vertices.empty() && !resident->indices.empty()
                && draw.packet.index_count != 0) {
                const u32 lod = std::min<u32>(
                    resident->last_resident_lod,
                    static_cast<u32>(resident->vertices.size() - 1)
                );
                return services->bindings->DrawFallbackMesh(
                    *encoder,
                    targets,
                    product->schema,
                    *resident,
                    lod,
                    draw.object_index,
                    draw.packet.palette,
                    draw.packet.first_index,
                    draw.packet.index_count,
                    draw.packet.vertex_offset
                );
            }
            return services->bindings->DrawDiagnostic(*encoder, targets, draw.object_index);
        };
        if (resident == nullptr || product == nullptr || resident->vertices.empty() || resident->indices.empty()) {
            ++services->frame->skipped;
            ++services->frame->fallback_materials;
            TRY_VOID(diagnostic());
            continue;
        }
        auto prepared = services->materials->PipelineRequest(
            draw.packet.material,
            pass,
            targets,
            draw.packet.pipeline.vertex_schema_id,
            draw.packet.palette.IsValid()
        );
        if (!prepared) {
            ++services->frame->skipped;
            ++services->frame->fallback_materials;
            TRY_VOID(diagnostic());
            continue;
        }
        BorrowedLayout physical_layout;
        TRY_ASSIGN(
            physical_layout,
            services->bindings->Bind(*encoder, *prepared, draw.object_index, draw.packet.palette)
        );
        const auto request = services->pipelines->Find(prepared->pipeline);
        if (request.state != PipelineRequestState::Ready || request.pipeline == nullptr) {
            ++services->frame->skipped;
            if (request.state == PipelineRequestState::Failed)
                Diagnostic(services, "RND2004", request.diagnostic);
            TRY_VOID(diagnostic());
            continue;
        }
        const u32 lod = std::min<u32>(resident->last_resident_lod, static_cast<u32>(resident->vertices.size() - 1));
        encoder->SetPipeline(*request.pipeline);
        encoder->SetVertexBuffer(
            0,
            resident->vertices[lod].buffer,
            resident->vertices[lod].offset,
            resident->vertices[lod].size
        );
        encoder->SetIndexBuffer(
            *resident->indices[lod].buffer,
            rhi::IndexFormat::Uint32,
            resident->indices[lod].offset,
            resident->indices[lod].size
        );
        encoder->DrawIndexed(
            draw.packet.index_count,
            1,
            draw.packet.first_index,
            draw.packet.vertex_offset,
            draw.packet.instance_offset
        );
        services->frame->used_pipelines.push_back(prepared->pipeline);
        ++services->frame->draw_calls;
    }
    return Ok();
}

Result<bool> feature_detail::DrawIndirect(
    StandardFeatureServices* services,
    RenderGraphContext& context,
    const IndirectDrawStream& stream,
    const RenderPhase phase,
    const MaterialPass pass,
    const PipelineTargetSignature& targets,
    const bool apply_view_viewport
) {
    if (!stream.ready || services == nullptr || services->materials == nullptr || services->meshes == nullptr
        || services->pipelines == nullptr || services->bindings == nullptr)
        return Ok(false);
    auto* encoder = context.RenderEncoder();
    if (encoder == nullptr)
        return Ok(false);
    auto commands = context.Buffer(stream.commands_version);
    auto counts = context.Buffer(stream.counts_version);
    if (!commands || !counts)
        return Ok(false);

    struct ReadyBin final {
        const IndirectBin* bin{};
        const MeshProduct* product{};
        const MeshResident* resident{};
        PreparedMaterial material;
        ref<rhi::RenderPipeline> pipeline;
    };

    std::vector<ReadyBin> ready;
    ready.reserve(stream.bins.size());
    for (const auto& bin : stream.bins) {
        if (bin.phase != phase)
            continue;
        const MeshHandle mesh = MeshHandle::Create(bin.key.mesh.Index(), bin.key.mesh.Generation());
        const auto* product = services->meshes->Product(mesh);
        const auto* resident = services->meshes->Resident(mesh);
        if (product == nullptr || resident == nullptr || bin.key.lod >= resident->vertices.size()
            || bin.key.lod >= resident->indices.size())
            return Ok(false);
        PreparedMaterial material;
        auto prepared = services->materials
                            ->PipelineRequest(bin.key.material, pass, targets, product->schema.id, false);
        if (!prepared)
            return Ok(false);
        material = std::move(*prepared);
        if (material.pipeline.vertex_entry == StringId("pbr_static_vs"))
            material.pipeline.vertex_entry = StringId("pbr_gpu_vs");
        else if (material.pipeline.vertex_entry == StringId("unlit_static_vs"))
            material.pipeline.vertex_entry = StringId("unlit_gpu_vs");
        else if (material.pipeline.vertex_entry == StringId("depth_static_vs"))
            material.pipeline.vertex_entry = StringId("depth_gpu_vs");
        else
            return Ok(false);
        BorrowedLayout layout;
        TRY_ASSIGN(layout, services->bindings->PrepareLayout(material.layout, {}));
        auto pipeline = services->pipelines->Request(
            material.pipeline,
            [services, key = material.pipeline, layout, schema = product->schema, state = material.render_state]() {
                return services->create_graphics_pipeline(key, layout.Pipeline(), schema, state);
            }
        );
        if (!pipeline || pipeline->state != PipelineRequestState::Ready || pipeline->pipeline == nullptr)
            return Ok(false);
        ready.push_back({&bin, product, resident, std::move(material), pipeline->pipeline});
    }
    if (apply_view_viewport) {
        const auto viewport = services->frame->view->viewport;
        const u32 width = services->frame->view->render_width == 0 ? viewport.width
                                                                   : services->frame->view->render_width;
        const u32 height = services->frame->view->render_height == 0 ? viewport.height
                                                                     : services->frame->view->render_height;
        const u32 x = services->frame->direct_output ? viewport.x : 0;
        const u32 y = services->frame->direct_output ? viewport.y : 0;
        encoder->SetViewport(
            static_cast<f32>(x),
            static_cast<f32>(y),
            static_cast<f32>(width),
            static_cast<f32>(height),
            0.0F,
            1.0F
        );
        encoder->SetScissorRect(x, y, width, height);
    }
    for (const auto& item : ready) {
        TRY_VOID(services->bindings->Bind(*encoder, item.material, 0, {}));
        encoder->SetPipeline(*item.pipeline);
        encoder->SetVertexBuffer(0, item.resident->vertices[item.bin->key.lod].buffer, 0);
        encoder->SetIndexBuffer(*item.resident->indices[item.bin->key.lod].buffer, rhi::IndexFormat::Uint32, 0);
        encoder->MultiDrawIndexedIndirectCount(
            commands->get(),
            item.bin->first_command,
            item.bin->command_capacity,
            counts->get(),
            item.bin->count_index
        );
        services->frame->draw_calls += item.bin->command_capacity;
        services->frame->used_pipelines.push_back(item.material.pipeline);
    }
    return Ok(true);
}

} // namespace woki::gfx
