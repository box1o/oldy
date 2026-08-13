#include "internal.hpp"

namespace woki::gfx::feature_detail {
namespace {

class SkyFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;

    Result<void> DeclareGraph(GraphDeclarationContext& context) const override {
        auto* color = context.blackboard.Get<SceneColorOutput>();
        const auto* depth = context.blackboard.Get<DepthFeatureOutput>();
        if (color == nullptr || depth == nullptr)
            return Err(ErrorCode::ValidationInvalidState, "sky feature requires scene color and depth");
        if (Frame() != nullptr && Frame()->view != nullptr && !Frame()->view->sky_visible)
            return Ok();
        auto pass = context.graph.AddPass("Sky environment", PassKind::Render);
        pass.Read(depth->version, GraphAccess::Sampled);
        color->version = pass.Color(color->color, ColorLoad(rhi::LoadOp::Load));
        auto* services = services_;
        const auto format = Frame() == nullptr || Frame()->targets.colors.empty() ? rhi::TextureFormat::RGBA16Float
                                                                                  : Frame()->targets.colors.front();
        const auto samples = Frame() == nullptr ? 1U : Frame()->targets.samples;
        ResolvedTexture radiance;
        if (Frame() != nullptr && Frame()->view != nullptr && Frame()->view->environment.IsValid()
            && services_->resolve_environment) {
            auto environment = services_->resolve_environment(Frame()->view->environment);
            if (environment)
                radiance = environment->radiance;
        }
        const auto depth_version = depth->version;
        pass.Execute([services, format, samples, depth_version, radiance = std::move(radiance)](
                         RenderGraphContext& graph
                     ) {
            return services->programs->DrawFullscreen(
                graph,
                {
                    .program = FullscreenProgram::Sky,
                    .source = {},
                    .secondary = {},
                    .tertiary = depth_version,
                    .quaternary = {},
                    .external_source = radiance.physical == nullptr ? nullptr : radiance.physical->default_view.get(),
                    .grading_lut = nullptr,
                    .parameters = {},
                    .target_format = format,
                    .sample_count = samples,
                }
            );
        });
        return Ok();
    }
};

} // namespace

Result<void> RegisterEnvironmentFeatures(FeatureRegistry& registry) {
    auto sky = Metadata("sky", FeatureScope::View, {StringId("forward")});
    sky.insertion_points = {StringId("opaque")};
    return RegisterFactory(
        registry,
        std::move(sky),
        [](CompiledFeatureConfig config, StandardFeatureServices* services) {
            return createRef<SkyFeature>(std::move(config), services);
        }
    );
}

} // namespace woki::gfx::feature_detail
