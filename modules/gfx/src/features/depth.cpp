#include "internal.hpp"

namespace woki::gfx::feature_detail {
namespace {

class DepthFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;

    Result<void> DeclareGraph(GraphDeclarationContext& context) const override {
        if (context.blackboard.Get<MeshFeatureOutput>() == nullptr)
            return Err(ErrorCode::ValidationInvalidState, "depth feature requires mesh packets");
        const auto format = Frame() == nullptr ? rhi::TextureFormat::Depth24Plus : Frame()->targets.depth;
        auto descriptor = TextureDescriptor("View depth", format, Frame() == nullptr ? 1U : Frame()->targets.samples);
        descriptor.extent = GraphExtent::Fixed(context.width, context.height);
        auto texture = context.graph.CreateTexture(descriptor);
        const auto* gpu_visibility = context.blackboard.Get<GpuVisibilityOutput>();
        const IndirectDrawStream* indirect = gpu_visibility != nullptr && gpu_visibility->stream.ready ? &gpu_visibility->stream : nullptr;
        auto pass = context.graph.AddPass("Depth prepass", PassKind::Render);
        if (indirect != nullptr)
            pass.Read(indirect->commands_version, GraphAccess::Indirect).Read(indirect->counts_version, GraphAccess::Indirect);
        const auto version = pass.Depth(texture, DepthLoad(rhi::LoadOp::Clear));
        auto* services = services_;
        pass.Execute([services, format, indirect](RenderGraphContext& graph) -> Result<void> {
            bool gpu_drawn{};
            if (indirect != nullptr)
                TRY_ASSIGN(gpu_drawn, DrawIndirect(services, graph, *indirect, RenderPhase::Depth, MaterialPass::Depth, {{}, format, services->frame->targets.samples}));
            if (gpu_drawn)
                return Ok();
            if (indirect != nullptr)
                services->frame->gpu_visibility.fallback = GpuDrivenFallbackReason::PipelinePending;
            return Draw(services, graph, RenderPhase::Depth, MaterialPass::Depth, {{}, format, services->frame->targets.samples});
        });
        auto result = context.blackboard.Emplace<DepthFeatureOutput>(DepthFeatureOutput{texture, version});
        return result ? Ok() : Err(std::move(result).error());
    }
};

class HiZDepthPyramidFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;

    void OnSubmitted(const rhi::SubmissionTicket submission) const override {
        if (pending_ && services_ != nullptr && services_->histories != nullptr) {
            services_->histories->Submitted(pending_->first, submission, pending_->second);
            pending_.reset();
        }
    }

    Result<void> DeclareGraph(GraphDeclarationContext& context) const override {
        const auto* depth = context.blackboard.Get<DepthFeatureOutput>();
        auto* history_output = context.blackboard.Get<HiZDepthPyramidOutput>();
        if (history_output == nullptr) {
            auto inserted = context.blackboard.Emplace<HiZDepthPyramidOutput>();
            if (!inserted)
                return Err(std::move(inserted).error());
            history_output = &inserted->get();
        }
        const bool capable = depth != nullptr && services_ != nullptr && services_->device != nullptr && services_->histories != nullptr && services_->programs != nullptr && services_->programs->GpuDrivenReady()
                             && services_->device->Capabilities().Has(rhi::CapabilityFeature::Compute) && services_->device->Capabilities().Has(rhi::CapabilityFeature::StorageTextures)
                             && services_->device->Capabilities().Supports(rhi::TextureFormat::R32Float, rhi::TextureUsage::TextureBinding | rhi::TextureUsage::StorageBinding);
        if (!capable || Frame() == nullptr || Frame()->view == nullptr || history_output == nullptr || !history_output->current)
            return Ok();
        const u32 mip_count = history_output->mip_count;
        const RenderHistoryKey key{Frame()->view->history, TemporalSemantic::HiZDepth};

        GraphTextureRef current_version;
        for (u32 mip = 0; mip < mip_count; ++mip) {
            auto pass = context.graph.AddPass(mip == 0 ? "Hi-Z depth seed mip 0" : "Hi-Z downsample mip " + std::to_string(mip), PassKind::Compute);
            const GraphTextureRef source = mip == 0 ? depth->version : current_version;
            GraphTextureViewDesc source_view;
            source_view.subresources = {.base_mip_level = mip == 0 ? 0U : mip - 1U,
                .mip_level_count = 1,
                .base_array_layer = 0,
                .array_layer_count = 1,
                .aspect = mip == 0 ? rhi::TextureAspect::DepthOnly : rhi::TextureAspect::All};
            source_view.dimension = rhi::TextureViewDimension::e2D;
            source_view.usage = rhi::TextureUsage::TextureBinding;
            pass.Read(source, GraphAccess::Sampled, source_view.subresources, source_view);
            GraphTextureViewDesc destination_view;
            destination_view.subresources = {.base_mip_level = mip, .mip_level_count = 1, .base_array_layer = 0, .array_layer_count = 1};
            destination_view.format = rhi::TextureFormat::R32Float;
            destination_view.dimension = rhi::TextureViewDimension::e2D;
            destination_view.usage = rhi::TextureUsage::StorageBinding;
            current_version = pass.Write(history_output->current, GraphAccess::StorageWrite, destination_view.subresources, destination_view);
            const u32 width = std::max(1U, context.width >> mip);
            const u32 height = std::max(1U, context.height >> mip);
            auto* services = services_;
            pass.Queue(QueuePreference::Compute, true)
                .Execute([services, source, current_version, source_view, destination_view, width, height, depth_source = mip == 0](RenderGraphContext& graph) mutable -> Result<void> {
                    auto source_texture = graph.TextureView(source, source_view);
                    auto destination_texture = graph.TextureView(current_version, destination_view);
                    if (!source_texture || !destination_texture)
                        return Err(ErrorCode::ValidationInvalidState, "Hi-Z mip views are unavailable");
                    return services->programs->DispatchHiZ(graph, source_texture->get(), destination_texture->get(), width, height, depth_source);
                });
        }
        history_output->current_version = current_version;
        pending_ = std::pair{key, Frame()->frame_number};
        return Ok();
    }

private:
    mutable std::optional<std::pair<RenderHistoryKey, u64>> pending_;
};

} // namespace

Result<void> RegisterDepthFeatures(FeatureRegistry& registry) {
    auto depth = Metadata("depth", FeatureScope::View, {StringId("mesh")});
    depth.insertion_points = {StringId("depth-prepass")};
    auto hiz = Metadata("hiz-depth-pyramid", FeatureScope::View, {StringId("depth"), StringId("gpu-visibility")});
    hiz.insertion_points = {StringId("depth-prepass")};
    TRY_VOID(RegisterFactory(registry, std::move(depth), [](CompiledFeatureConfig config, StandardFeatureServices* services) { return createRef<DepthFeature>(std::move(config), services); }));
    return RegisterFactory(registry, std::move(hiz), [](CompiledFeatureConfig config, StandardFeatureServices* services) { return createRef<HiZDepthPyramidFeature>(std::move(config), services); });
}

} // namespace woki::gfx::feature_detail
