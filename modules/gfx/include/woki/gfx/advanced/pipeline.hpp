#pragma once

#include <map>
#include <set>
#include <algorithm>

#include <woki/asset.hpp>
#include <woki/enums.hpp>

#include "types.hpp"
#include "feature.hpp"

namespace woki::gfx {

inline constexpr u32 kPipelineSchema = 1;
inline constexpr u32 kFeatureConfigSchema = 1;
inline constexpr u32 kQualitySchema = 1;

struct PipelineDiagnostic {
    std::string code;
    DiagnosticSeverity severity{DiagnosticSeverity::Error};
    std::string message;
    std::string pointer;
    asset::AssetPath asset;
    SourceRange range;
};

enum class PipelineTargetFormat : u8 { Surface, RGBA8Unorm, RGBA8UnormSrgb, BGRA8Unorm, BGRA8UnormSrgb, RGBA16Float, Depth24Plus, Depth24PlusStencil8, Depth32Float };

[[nodiscard]] std::optional<PipelineTargetFormat> ParsePipelineTargetFormat(std::string_view name) noexcept;
[[nodiscard]] std::string_view PipelineTargetFormatName(PipelineTargetFormat format) noexcept;
[[nodiscard]] std::optional<rhi::TextureFormat> ToRhiTextureFormat(PipelineTargetFormat format) noexcept;

struct TargetRequirements {
    std::vector<PipelineTargetFormat> color;
    std::optional<PipelineTargetFormat> depth;
    u32 samples{1};
};

struct FeatureSource {
    std::string id;
    std::optional<std::string> instance;
    std::optional<asset::AssetPath> config;
    std::map<std::string, ConfigValue, std::less<>> settings;
};

struct FeatureReferenceSource {
    std::string id;
    std::optional<std::string> instance;
};

struct FallbackSource {
    asset::AssetPath pipeline;
    std::vector<std::string> missing_capabilities;
};

struct PipelineSource {
    u32 schema{kPipelineSchema};
    std::string name;
    std::string render_path;
    TargetRequirements targets;
    std::vector<FeatureSource> features;
    std::map<std::string, std::vector<FeatureReferenceSource>, std::less<>> extension_points;
    std::optional<asset::AssetPath> quality_profile;
    std::vector<FallbackSource> fallbacks;
};

struct FeatureConfigSource {
    u32 schema{kFeatureConfigSchema};
    std::string feature;
    std::map<std::string, ConfigValue, std::less<>> settings;
};

struct QualityFeatureOverride {
    std::optional<bool> enabled;
    std::map<std::string, ConfigValue, std::less<>> settings;
};

struct QualitySource {
    u32 schema{kQualitySchema};
    std::string name;
    std::map<std::string, QualityFeatureOverride, std::less<>> features;
};

template <typename T>
struct PipelineParseResult {
    T value;
    std::vector<PipelineDiagnostic> diagnostics;

    [[nodiscard]] bool Valid() const noexcept {
        return std::ranges::none_of(diagnostics, [](const auto& diagnostic) { return diagnostic.severity == DiagnosticSeverity::Error; });
    }
};

[[nodiscard]] PipelineParseResult<PipelineSource> ParsePipeline(const asset::AssetPath& path, std::string_view jsonc);
[[nodiscard]] PipelineParseResult<FeatureConfigSource> ParseFeatureConfig(const asset::AssetPath& path, std::string_view jsonc);
[[nodiscard]] PipelineParseResult<QualitySource> ParseQuality(const asset::AssetPath& path, std::string_view jsonc);

struct CompiledFeatureRequest {
    StringId id;
    std::string debug_name;
    std::optional<std::string> instance;
    FeatureScope scope{FeatureScope::Pipeline};
    FeatureMultiplicity multiplicity{FeatureMultiplicity::Once};
    SemanticVersion factory_version;
    CompiledFeatureConfig config;
};

struct FeatureReference {
    StringId id;
    std::string debug_name;
    std::optional<std::string> instance;
    [[nodiscard]] friend auto operator<=>(const FeatureReference&, const FeatureReference&) noexcept = default;
};

struct ExtensionBinding {
    StringId point;
    std::string debug_name;
    std::vector<FeatureReference> features;
};

struct FallbackRule {
    asset::AssetId target;
    asset::AssetPath path;
    std::vector<StringId> missing_capabilities;
    std::vector<std::string> capability_names;
};

struct PipelineDependency {
    asset::AssetPath path;
    ContentHash hash;
    [[nodiscard]] friend auto operator<=>(const PipelineDependency&, const PipelineDependency&) noexcept = default;
};

using CapabilitySet = std::vector<StringId>;

struct RenderPipelineIR {
    asset::AssetId asset_id;
    StringId name_id;
    std::string debug_name;
    StringId render_path;
    std::string render_path_name;
    TargetRequirements targets;
    std::vector<CompiledFeatureRequest> features;
    std::vector<ExtensionBinding> extensions;
    std::optional<asset::AssetId> quality_id;
    std::optional<asset::AssetPath> quality_path;
    std::vector<FallbackRule> fallbacks;
    CapabilitySet required_capabilities;
    std::vector<std::string> required_capability_names;
    std::vector<PipelineDependency> dependencies;
    ContentHash root_source_hash;
    ContentHash content_hash;
};

struct PipelineCompileResult {
    std::optional<RenderPipelineIR> pipeline;
    std::vector<PipelineDiagnostic> diagnostics;

    [[nodiscard]] bool Valid() const noexcept {
        return pipeline.has_value();
    }
};

class RenderPipelineCompiler {
public:
    RenderPipelineCompiler(const asset::Vfs& vfs, const FeatureRegistry& registry)
        : vfs_(vfs),
          registry_(registry) {}

    [[nodiscard]] PipelineCompileResult Compile(const asset::AssetPath& path) const;

private:
    const asset::Vfs& vfs_;
    const FeatureRegistry& registry_;
};

[[nodiscard]] ContentHash HashPipeline(const RenderPipelineIR& pipeline);
[[nodiscard]] Result<asset::AssetPath> SelectSupportedPipeline(const asset::Vfs& vfs, const FeatureRegistry& registry, const asset::AssetPath& root, CapabilitySet capabilities);

} // namespace woki::gfx
