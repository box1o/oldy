#include "internal.hpp"

namespace woki::gfx::feature_detail {
namespace {

class PresentationFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;

    Result<void> DeclareGraph(GraphDeclarationContext& context) const override {
        auto* color = context.blackboard.Get<SceneColorOutput>();
        const auto* target = context.blackboard.Get<RenderTargetOutput>();
        if (color == nullptr || target == nullptr)
            return Err(ErrorCode::ValidationInvalidState, "presentation feature inputs are incomplete");
        if (color->color != target->color) {
            const bool tone_map = color->hdr;
            auto pass = context.graph
                            .AddPass(tone_map ? "Tone map presentation" : "SDR presentation copy", PassKind::Render);
            const auto source = color->version;
            pass.Read(source, GraphAccess::Sampled);
            color->version = pass.Color(target->color, ColorLoad(rhi::LoadOp::Clear, {0, 0, 0, 1}));
            auto* services = services_;
            const f32 exposure = Frame() == nullptr ? 1.0F : Frame()->exposure;
            const bool srgb_attachment = target->encoding == SdrTargetEncoding::SrgbAttachment
                                         || (target->encoding == SdrTargetEncoding::Auto
                                             && (target->format == rhi::TextureFormat::RGBA8UnormSrgb
                                                 || target->format == rhi::TextureFormat::BGRA8UnormSrgb));
            ResolvedTexture grading;
            if (services_->textures != nullptr && services_->grading_lut.IsValid()) {
                auto resolved = services_->textures->Resolve(services_->grading_lut, TextureSemantic::Color);
                if (resolved)
                    grading = std::move(*resolved);
            }
            pass.Execute([services, source, target, exposure, srgb_attachment, tone_map, grading = std::move(grading)](
                             RenderGraphContext& graph
                         ) {
                return services->programs->DrawFullscreen(
                    graph,
                    {
                        .program = tone_map ? FullscreenProgram::ToneMap : FullscreenProgram::Copy,
                        .source = source,
                        .secondary = {},
                        .tertiary = {},
                        .quaternary = {},
                        .external_source = nullptr,
                        .grading_lut = grading.physical == nullptr ? nullptr : grading.physical->default_view.get(),
                        .parameters =
                            {exposure, srgb_attachment ? 0.0F : 1.0F, 0.0F, grading.physical == nullptr ? 0.0F : 1.0F},
                        .target_format = target->format,
                        .sample_count = target->samples,
                    }
                );
            });
        }
        return context.graph.Export(color->version, target->final_state);
    }
};

} // namespace

Result<void> RegisterPresentationFeatures(FeatureRegistry& registry) {
    auto presentation = Metadata("presentation", FeatureScope::View, {StringId("transparent")});
    presentation.optional_features = {StringId("sky"),
        StringId("editor-overlay"),
        StringId("bloom"),
        StringId("exposure"),
        StringId("temporal")};
    presentation.insertion_points = {StringId("post-process")};
    return RegisterFactory(
        registry,
        std::move(presentation),
        [](CompiledFeatureConfig config, StandardFeatureServices* services) {
            return createRef<PresentationFeature>(std::move(config), services);
        }
    );
}

} // namespace woki::gfx::feature_detail
