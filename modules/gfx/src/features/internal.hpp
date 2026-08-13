#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>

#include <woki/gfx/advanced/standard_features.hpp>
#include <woki/rhi/compute_pass_encoder.hpp>
#include <woki/rhi/device.hpp>
#include <woki/rhi/queue.hpp>
#include <woki/rhi/render_pass_encoder.hpp>

#include "../internal/readback_manager.hpp"

namespace woki::gfx::feature_detail {

class FeatureBase : public RenderFeature {
public:
    FeatureBase(CompiledFeatureConfig config, StandardFeatureServices* services)
        : config_(std::move(config)),
          services_(services) {}

    StringId Id() const noexcept final {
        return config_.feature;
    }

    const std::optional<std::string>& Instance() const noexcept final {
        return config_.instance;
    }

    const CompiledFeatureConfig& Config() const noexcept final {
        return config_;
    }

protected:
    [[nodiscard]] StandardFrameFeatureState* Frame() const noexcept {
        return services_ == nullptr ? nullptr : services_->frame;
    }

    CompiledFeatureConfig config_;
    StandardFeatureServices* services_{};
};

using CreateFeature = std::function<ref<const RenderFeature>(CompiledFeatureConfig, StandardFeatureServices*)>;

[[nodiscard]] GraphTextureDesc TextureDescriptor(std::string label, rhi::TextureFormat format, u32 samples);
[[nodiscard]] ColorAttachment ColorLoad(rhi::LoadOp load, rhi::Color clear = {0.12, 0.12, 0.18, 1.0});
[[nodiscard]] DepthAttachment DepthLoad(rhi::LoadOp load, bool read_only = false, f32 clear = 1.0F);
[[nodiscard]] math::mat4f JitteredViewProjection(const math::mat4f& matrix, math::vec2f jitter);
void Diagnostic(StandardFeatureServices* services, std::string code, std::string message, DiagnosticSeverity severity = DiagnosticSeverity::Warning);
[[nodiscard]] Result<void> Draw(StandardFeatureServices* services, RenderGraphContext& context, RenderPhase phase, MaterialPass pass, PipelineTargetSignature targets, bool apply_view_viewport = true);
[[nodiscard]] Result<bool> DrawIndirect(StandardFeatureServices* services,
    RenderGraphContext& context,
    const IndirectDrawStream& stream,
    RenderPhase phase,
    MaterialPass pass,
    const PipelineTargetSignature& targets,
    bool apply_view_viewport = true);
[[nodiscard]] RenderFeatureMetadata Metadata(std::string name, FeatureScope scope, std::vector<StringId> required = {}, std::vector<FeatureConfigField> fields = {});
[[nodiscard]] Result<void> RegisterFactory(FeatureRegistry& registry, RenderFeatureMetadata metadata, CreateFeature create);

[[nodiscard]] Result<void> RegisterMeshFeatures(FeatureRegistry& registry);
[[nodiscard]] Result<void> RegisterDepthFeatures(FeatureRegistry& registry);
[[nodiscard]] Result<void> RegisterLightingFeatures(FeatureRegistry& registry);
[[nodiscard]] Result<void> RegisterShadowFeatures(FeatureRegistry& registry);
[[nodiscard]] Result<void> RegisterEnvironmentFeatures(FeatureRegistry& registry);
[[nodiscard]] Result<void> RegisterTemporalFeatures(FeatureRegistry& registry);
[[nodiscard]] Result<void> RegisterPostFeatures(FeatureRegistry& registry);
[[nodiscard]] Result<void> RegisterPresentationFeatures(FeatureRegistry& registry);

} // namespace woki::gfx::feature_detail
