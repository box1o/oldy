#include "internal.hpp"

namespace woki::gfx::feature_detail {
namespace {

class TemporalFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;

    void OnSubmitted(const rhi::SubmissionTicket submission) const override {
        if (pending_ && services_ != nullptr && services_->histories != nullptr) {
            services_->histories->Submitted(pending_->first, submission, pending_->second);
            pending_.reset();
        }
    }

    Result<void> DeclareGraph(GraphDeclarationContext& context) const override {
        auto* scene = context.blackboard.Get<SceneColorOutput>();
        const auto* depth = context.blackboard.Get<DepthFeatureOutput>();
        const auto* velocity = context.blackboard.Get<VelocityOutputs>();
        if (scene == nullptr || depth == nullptr)
            return Err(ErrorCode::ValidationInvalidState, "temporal anti-aliasing requires scene color and depth");
        const bool taa = Frame() != nullptr && Frame()->view != nullptr && Frame()->view->temporal.taa
                         && velocity != nullptr && services_->histories != nullptr;
        auto descriptor = TextureDescriptor(
            taa ? "TAA color" : "FXAA color",
            scene->hdr ? rhi::TextureFormat::RGBA16Float : Frame()->targets.colors.front(),
            1
        );
        descriptor.extent = GraphExtent::Fixed(context.width, context.height);
        if (!taa) {
            auto output = context.graph.CreateTexture(descriptor);
            auto pass = context.graph.AddPass("Physical FXAA", PassKind::Render);
            const auto source = scene->version;
            pass.Read(source, GraphAccess::Sampled);
            scene->color = output;
            scene->version = pass.Color(output, ColorLoad(rhi::LoadOp::Clear, {0, 0, 0, 1}));
            auto* services = services_;
            pass.Execute([services, source, format = descriptor.format](RenderGraphContext& graph) {
                return services->programs->DrawFullscreen(
                    graph,
                    {
                        .program = FullscreenProgram::Fxaa,
                        .source = source,
                        .secondary = {},
                        .tertiary = {},
                        .quaternary = {},
                        .external_source = nullptr,
                        .grading_lut = nullptr,
                        .parameters = {},
                        .target_format = format,
                        .sample_count = 1,
                    }
                );
            });
            return Ok();
        }
        const auto& view = *Frame()->view;
        const RenderHistoryKey key{view.history, TemporalSemantic::HdrColor};
        RenderHistoryBinding binding;
        TRY_ASSIGN(
            binding,
            services_->histories->Acquire(key, descriptor, Frame()->frame_number, view.HistoryInvalid())
        );
        const auto history = context.graph.ImportTemporal(
            {.previous = binding.previous, .current = binding.current, .descriptor = descriptor}
        );
        auto pass = context.graph.AddPass("Physical temporal anti-aliasing", PassKind::Render);
        const auto source = scene->version;
        pass.Read(source, GraphAccess::Sampled)
            .Read(history.previous_version, GraphAccess::Sampled)
            .Read(depth->version, GraphAccess::Sampled)
            .Read(velocity->version, GraphAccess::Sampled);
        const auto resolved = pass.Color(history.current, ColorLoad(rhi::LoadOp::Clear, {0, 0, 0, 1}));
        scene->color = history.current;
        scene->version = resolved;
        const f32 feedback = Setting("historyWeight") == nullptr
                                 ? view.temporal.feedback
                                 : static_cast<f32>(std::get<f64>(*Setting("historyWeight")));
        const std::array<f32, 4> parameters{feedback, binding.valid ? 1.0F : 0.0F, 0.0F, 0.0F};
        auto* services = services_;
        pass.Execute([services,
                         source,
                         previous = history.previous_version,
                         depth_version = depth->version,
                         velocity_version = velocity->version,
                         parameters,
                         format = descriptor.format](RenderGraphContext& graph) {
            return services->programs->DrawFullscreen(
                graph,
                {.program = FullscreenProgram::Taa,
                    .source = source,
                    .secondary = previous,
                    .tertiary = depth_version,
                    .quaternary = velocity_version,
                    .parameters = parameters,
                    .target_format = format,
                    .sample_count = 1}
            );
        });
        pending_ = std::pair{key, Frame()->frame_number};
        return Ok();
    }

private:
    mutable std::optional<std::pair<RenderHistoryKey, u64>> pending_;
};

} // namespace

Result<void> RegisterTemporalFeatures(FeatureRegistry& registry) {
    auto temporal = Metadata(
        "temporal",
        FeatureScope::View,
        {StringId("forward")},
        {{"historyWeight", ConfigType::Number, false, ConfigValue{0.9}},
            {"jitter", ConfigType::Boolean, false, ConfigValue{true}}}
    );
    temporal.optional_features = {StringId("velocity")};
    temporal.insertion_points = {StringId("post-process")};
    return RegisterFactory(
        registry,
        std::move(temporal),
        [](CompiledFeatureConfig config, StandardFeatureServices* services) {
            return createRef<TemporalFeature>(std::move(config), services);
        }
    );
}

} // namespace woki::gfx::feature_detail
