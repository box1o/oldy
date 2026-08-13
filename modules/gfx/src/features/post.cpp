#include "internal.hpp"

namespace woki::gfx::feature_detail {
namespace {

class BloomFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;

    Result<void> DeclareGraph(GraphDeclarationContext& context) const override {
        auto* scene = context.blackboard.Get<SceneColorOutput>();
        if (scene == nullptr || !scene->hdr)
            return Ok();
        const u32 requested = Setting("levels") == nullptr
                                  ? 5U
                                  : static_cast<u32>(std::clamp<i64>(std::get<i64>(*Setting("levels")), 1, 6));
        const u32 levels = std::min(
            requested,
            static_cast<u32>(
                std::max(1.0, std::floor(std::log2(static_cast<double>(std::min(context.width, context.height)))) - 3.0)
            )
        );
        std::vector<std::pair<GraphTexture, GraphTextureRef>> downsampled;
        GraphTextureRef source = scene->version;
        for (u32 level = 0; level < levels; ++level) {
            auto descriptor = TextureDescriptor(
                "Bloom level " + std::to_string(level),
                rhi::TextureFormat::RGBA16Float,
                1
            );
            const f32 scale = 1.0F / static_cast<f32>(2U << level);
            descriptor.extent = GraphExtent::Fixed(
                std::max(1U, static_cast<u32>(static_cast<f32>(context.width) * scale)),
                std::max(1U, static_cast<u32>(static_cast<f32>(context.height) * scale))
            );
            auto threshold = context.graph.CreateTexture(descriptor);
            auto extract = context.graph.AddPass(
                level == 0 ? "Bloom threshold" : "Bloom downsample " + std::to_string(level),
                PassKind::Render
            );
            extract.Read(source, GraphAccess::Sampled);
            auto extracted = extract.Color(threshold, ColorLoad(rhi::LoadOp::Clear, {0, 0, 0, 1}));
            auto* services = services_;
            const f32 threshold_value = Setting("threshold") == nullptr
                                            ? 1.0F
                                            : static_cast<f32>(std::get<f64>(*Setting("threshold")));
            extract.Execute([services, source, threshold_value, level](RenderGraphContext& graph) {
                ++services->frame->bloom_passes;
                return services->programs->DrawFullscreen(
                    graph,
                    {
                        .program = level == 0 ? FullscreenProgram::BloomThreshold : FullscreenProgram::BloomDownsample,
                        .source = source,
                        .secondary = {},
                        .tertiary = {},
                        .quaternary = {},
                        .external_source = nullptr,
                        .grading_lut = nullptr,
                        .parameters = {threshold_value, 0.5F, 0.0F, 0.0F},
                        .target_format = rhi::TextureFormat::RGBA16Float,
                        .sample_count = 1,
                    }
                );
            });
            auto blurred = context.graph.CreateTexture(descriptor);
            auto blur_x = context.graph.AddPass("Bloom horizontal blur " + std::to_string(level), PassKind::Render);
            blur_x.Read(extracted, GraphAccess::Sampled);
            auto horizontal = blur_x.Color(blurred, ColorLoad(rhi::LoadOp::Clear, {0, 0, 0, 1}));
            blur_x.Execute([services, extracted](RenderGraphContext& graph) {
                ++services->frame->bloom_passes;
                return services->programs->DrawFullscreen(
                    graph,
                    {
                        .program = FullscreenProgram::BloomBlur,
                        .source = extracted,
                        .secondary = {},
                        .tertiary = {},
                        .quaternary = {},
                        .external_source = nullptr,
                        .grading_lut = nullptr,
                        .parameters = {0, 0, 1, 0},
                        .target_format = rhi::TextureFormat::RGBA16Float,
                        .sample_count = 1,
                    }
                );
            });
            auto vertical_texture = context.graph.CreateTexture(descriptor);
            auto blur_y = context.graph.AddPass("Bloom vertical blur " + std::to_string(level), PassKind::Render);
            blur_y.Read(horizontal, GraphAccess::Sampled);
            auto vertical = blur_y.Color(vertical_texture, ColorLoad(rhi::LoadOp::Clear, {0, 0, 0, 1}));
            blur_y.Execute([services, horizontal](RenderGraphContext& graph) {
                ++services->frame->bloom_passes;
                return services->programs->DrawFullscreen(
                    graph,
                    {
                        .program = FullscreenProgram::BloomBlur,
                        .source = horizontal,
                        .secondary = {},
                        .tertiary = {},
                        .quaternary = {},
                        .external_source = nullptr,
                        .grading_lut = nullptr,
                        .parameters = {0, 0, 0, 1},
                        .target_format = rhi::TextureFormat::RGBA16Float,
                        .sample_count = 1,
                    }
                );
            });
            downsampled.emplace_back(vertical_texture, vertical);
            source = vertical;
        }
        GraphTextureRef accumulated = downsampled.empty() ? scene->version : downsampled.back().second;
        for (size_t level = downsampled.size(); level > 1; --level) {
            auto descriptor = TextureDescriptor(
                "Bloom upsample " + std::to_string(level - 2),
                rhi::TextureFormat::RGBA16Float,
                1
            );
            const f32 scale = 1.0F / static_cast<f32>(2U << (level - 2));
            descriptor.extent = GraphExtent::Fixed(
                std::max(1U, static_cast<u32>(static_cast<f32>(context.width) * scale)),
                std::max(1U, static_cast<u32>(static_cast<f32>(context.height) * scale))
            );
            auto texture = context.graph.CreateTexture(descriptor);
            auto upsample = context.graph.AddPass(
                "Bloom upsample and accumulate " + std::to_string(level - 2),
                PassKind::Render
            );
            const auto lower = downsampled[level - 2].second;
            const auto upper = accumulated;
            upsample.Read(lower, GraphAccess::Sampled).Read(upper, GraphAccess::Sampled);
            accumulated = upsample.Color(texture, ColorLoad(rhi::LoadOp::Clear, {0, 0, 0, 1}));
            auto* services = services_;
            upsample.Execute([services, lower, upper](RenderGraphContext& graph) {
                ++services->frame->bloom_passes;
                return services->programs->DrawFullscreen(
                    graph,
                    {
                        .program = FullscreenProgram::BloomComposite,
                        .source = lower,
                        .secondary = upper,
                        .tertiary = {},
                        .quaternary = {},
                        .external_source = nullptr,
                        .grading_lut = nullptr,
                        .parameters = {1.0F},
                        .target_format = rhi::TextureFormat::RGBA16Float,
                        .sample_count = 1,
                    }
                );
            });
        }
        auto composite_descriptor = TextureDescriptor("HDR scene with bloom", rhi::TextureFormat::RGBA16Float, 1);
        composite_descriptor.extent = GraphExtent::Fixed(context.width, context.height);
        auto composite_texture = context.graph.CreateTexture(composite_descriptor);
        auto composite = context.graph.AddPass("Bloom composite", PassKind::Render);
        const auto scene_source = scene->version;
        composite.Read(scene_source, GraphAccess::Sampled);
        for (const auto& level : downsampled)
            composite.Read(level.second, GraphAccess::Sampled);
        scene->color = composite_texture;
        scene->version = composite.Color(composite_texture, ColorLoad(rhi::LoadOp::Clear, {0, 0, 0, 1}));
        auto* services = services_;
        const auto bloom_source = accumulated;
        const f32 intensity = Setting("intensity") == nullptr ? 0.8F
                                                              : static_cast<f32>(std::get<f64>(*Setting("intensity")));
        composite.Execute([services, scene_source, bloom_source, intensity](RenderGraphContext& graph) {
            ++services->frame->bloom_passes;
            return services->programs->DrawFullscreen(
                graph,
                {
                    .program = FullscreenProgram::BloomComposite,
                    .source = scene_source,
                    .secondary = bloom_source,
                    .tertiary = {},
                    .quaternary = {},
                    .external_source = nullptr,
                    .grading_lut = nullptr,
                    .parameters = {intensity},
                    .target_format = rhi::TextureFormat::RGBA16Float,
                    .sample_count = 1,
                }
            );
        });
        return Ok();
    }
};

class ExposureFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;

    ~ExposureFeature() override {
        if (exposure_ != nullptr && services_ != nullptr && services_->releases != nullptr)
            services_->releases->Retire(std::move(exposure_), last_use_);
    }

    Result<void> PrepareScene(const RenderScenePreparationContext&) const override {
        if (pending_.IsValid() && services_->readbacks != nullptr
            && services_->readbacks->State(pending_) == ReadbackState::Ready) {
            auto bytes = services_->readbacks->Take(pending_);
            if (bytes && bytes->size() >= sizeof(f32)) {
                std::memcpy(&state_.value, bytes->data(), sizeof(f32));
                state_.valid = true;
                Frame()->exposure = state_.value;
            }
            pending_ = {};
        } else if (state_.valid) {
            Frame()->exposure = state_.value;
        }
        return Ok();
    }

    void OnSubmitted(const rhi::SubmissionTicket submission) const override {
        last_use_ = std::max(last_use_, submission);
        if (!pending_.IsValid() && services_ != nullptr && services_->readbacks != nullptr && exposure_ != nullptr) {
            auto queued = services_->readbacks->Enqueue({exposure_, 0, 16});
            if (queued)
                pending_ = *queued;
        }
    }

    Result<void> DeclareGraph(GraphDeclarationContext& context) const override {
        const auto* scene = context.blackboard.Get<SceneColorOutput>();
        if (scene == nullptr || !scene->hdr)
            return Ok();
        ExposureSettings settings;
        settings.mode = ExposureMode::Histogram;
        if (const auto* value = Setting("key"))
            settings.key = static_cast<f32>(std::get<f64>(*value));
        if (const auto* value = Setting("minimum"))
            settings.minimum = static_cast<f32>(std::get<f64>(*value));
        if (const auto* value = Setting("maximum"))
            settings.maximum = static_cast<f32>(std::get<f64>(*value));
        if (exposure_ == nullptr) {
            TRY_ASSIGN(
                exposure_,
                services_->device->CreateBuffer(
                    {.size = 16,
                        .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::CopyDst | rhi::BufferUsage::CopySrc,
                        .label = "Persistent adapted exposure"}
                )
            );
            const std::array<f32, 4> initial{Frame()->exposure, 0.0F, 0.0F, 0.0F};
            TRY_VOID(services_->device->GetQueue().WriteBuffer(*exposure_, 0, initial.data(), sizeof(initial)));
        }
        const GraphBufferDesc exposure_descriptor{"Persistent adapted exposure",
            16,
            16,
            rhi::BufferUsage::Storage | rhi::BufferUsage::CopyDst | rhi::BufferUsage::CopySrc};
        auto exposure = context.graph.ImportBuffer(
            {.descriptor = exposure_descriptor,
                .initial_state = ExternalState::ShaderRead,
                .final_state = ExternalState::CopySource,
                .frame_bound = false},
            exposure_
        );
        auto histogram = context.graph.CreateBuffer(
            {"Luminance histogram", 256U * sizeof(u32), 4, rhi::BufferUsage::Storage | rhi::BufferUsage::CopyDst}
        );
        const bool compute = services_->capabilities != nullptr
                             && std::ranges::find(*services_->capabilities, StringId("compute"))
                                    != services_->capabilities->end();
        if (!compute) {
            state_.Update(settings, Frame()->view->cpu_log_average_luminance.value_or(0.0F), Frame()->delta_time);
            auto pass = context.graph.AddPass("CPU adapted exposure fallback", PassKind::Copy);
            const auto version = pass.Write(exposure, GraphAccess::CopyDestination);
            pass.Execute([this, version](RenderGraphContext& graph) -> Result<void> {
                auto buffer = graph.Buffer(version);
                if (!buffer)
                    return Err(ErrorCode::ValidationInvalidState, "CPU exposure buffer is unavailable");
                const std::array<f32, 4> value{state_.value, 1.0F, 0.0F, 0.0F};
                return graph.CommandEncoder()
                    .WriteBuffer(buffer->get(), 0, reinterpret_cast<const u8*>(value.data()), sizeof(value));
            });
            auto result = context.blackboard.Emplace<ExposureOutput>(ExposureOutput{exposure, version, state_.value});
            if (!result)
                return Err(std::move(result).error());
            return context.graph.Export(version, ExternalState::CopySource);
        }
        auto build = context.graph.AddPass("Luminance histogram", PassKind::Compute);
        build.Read(scene->version, GraphAccess::Sampled);
        const auto exposure_initial = context.graph.Initial(exposure);
        build.Read(exposure_initial, GraphAccess::StorageRead);
        const auto histogram_version = build.Write(histogram, GraphAccess::StorageWrite);
        auto* services = services_;
        const auto source = scene->version;
        const f32 delta_time = Frame()->delta_time;
        build.Execute(
            [services,
                source,
                histogram_version,
                exposure_ref = exposure_initial,
                settings,
                delta_time,
                width = context.width,
                height = context.height](RenderGraphContext& graph) -> Result<void> {
                auto source_view = graph.TextureView(source);
                auto histogram_buffer = graph.Buffer(histogram_version);
                auto exposure_buffer = graph.Buffer(exposure_ref);
                if (!source_view || !histogram_buffer || !exposure_buffer)
                    return Err(ErrorCode::ValidationInvalidState, "histogram resources are unavailable");
                std::array<u32, 256> zero{};
                TRY_VOID(
                    graph.CommandEncoder()
                        .WriteBuffer(histogram_buffer->get(), 0, reinterpret_cast<const u8*>(zero.data()), sizeof(zero))
                );
                return services->programs->DispatchExposure(
                    graph,
                    source_view->get(),
                    histogram_buffer->get(),
                    exposure_buffer->get(),
                    settings,
                    delta_time,
                    width,
                    height,
                    false
                );
            }
        );
        auto reduce = context.graph.AddPass("Adapted exposure reduction", PassKind::Compute);
        reduce.Read(source, GraphAccess::Sampled);
        reduce.Read(histogram_version, GraphAccess::StorageRead);
        const auto exposure_version = reduce.Write(exposure, GraphAccess::StorageWrite);
        reduce.Execute(
            [services,
                source,
                histogram_version,
                exposure_version,
                settings,
                delta_time,
                width = context.width,
                height = context.height](RenderGraphContext& graph) -> Result<void> {
                auto source_view = graph.TextureView(source);
                auto histogram_buffer = graph.Buffer(histogram_version);
                auto exposure_buffer = graph.Buffer(exposure_version);
                if (!source_view || !histogram_buffer || !exposure_buffer)
                    return Err(ErrorCode::ValidationInvalidState, "exposure reduction resources are unavailable");
                return services->programs->DispatchExposure(
                    graph,
                    source_view->get(),
                    histogram_buffer->get(),
                    exposure_buffer->get(),
                    settings,
                    delta_time,
                    width,
                    height,
                    true
                );
            }
        );
        auto result = context.blackboard
                          .Emplace<ExposureOutput>(ExposureOutput{exposure, exposure_version, Frame()->exposure});
        if (!result)
            return Err(std::move(result).error());
        return context.graph.Export(exposure_version, ExternalState::CopySource);
    }

private:
    mutable ref<rhi::Buffer> exposure_;
    mutable ExposureState state_;
    mutable ReadbackTicket pending_;
    mutable rhi::SubmissionTicket last_use_;
};

class DebugOverlayFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;

    Result<void> DeclareGraph(GraphDeclarationContext& context) const override {
        auto* color = context.blackboard.Get<SceneColorOutput>();
        if (color == nullptr)
            return Ok();
        auto pass = context.graph.AddPass("Debug overlay hook", PassKind::Render);
        color->version = pass.Color(color->color, ColorLoad(rhi::LoadOp::Load));
        pass.SideEffect("debug-overlay").Execute([](RenderGraphContext&) { return Ok(); });
        return Ok();
    }
};

class NoOpFeature final : public FeatureBase {
public:
    using FeatureBase::FeatureBase;
};

} // namespace

Result<void> RegisterPostFeatures(FeatureRegistry& registry) {
    auto overlay = Metadata("editor-overlay", FeatureScope::View, {StringId("forward")});
    overlay.insertion_points = {StringId("overlay")};
    auto bloom = Metadata(
        "bloom",
        FeatureScope::View,
        {StringId("forward")},
        {{"threshold", ConfigType::Number, false, ConfigValue{1.0}},
            {"intensity", ConfigType::Number, false, ConfigValue{0.8}},
            {"levels", ConfigType::Integer, false, ConfigValue{i64{5}}}}
    );
    bloom.required_capabilities = {StringId("hdr")};
    bloom.insertion_points = {StringId("post-process")};
    auto exposure = Metadata(
        "exposure",
        FeatureScope::View,
        {StringId("forward")},
        {{"key", ConfigType::Number, false, ConfigValue{0.18}},
            {"minimum", ConfigType::Number, false, ConfigValue{0.03}},
            {"maximum", ConfigType::Number, false, ConfigValue{32.0}}}
    );
    exposure.insertion_points = {StringId("post-process")};
    TRY_VOID(RegisterFactory(
        registry,
        std::move(overlay),
        [](CompiledFeatureConfig config, StandardFeatureServices* services) {
            return createRef<DebugOverlayFeature>(std::move(config), services);
        }
    ));
    TRY_VOID(RegisterFactory(
        registry,
        std::move(bloom),
        [](CompiledFeatureConfig config, StandardFeatureServices* services) {
            return createRef<BloomFeature>(std::move(config), services);
        }
    ));
    return RegisterFactory(
        registry,
        std::move(exposure),
        [](CompiledFeatureConfig config, StandardFeatureServices* services) {
            return createRef<ExposureFeature>(std::move(config), services);
        }
    );
}

} // namespace woki::gfx::feature_detail
