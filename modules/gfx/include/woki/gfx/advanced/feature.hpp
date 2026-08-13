#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <algorithm>
#include <functional>

#include <woki/core.hpp>
#include <woki/rhi/submission.hpp>

namespace woki::gfx {

class RenderGraphBuilder;
class GraphBlackboard;
struct StandardFeatureServices;
struct RenderScenePreparationContext;
class RenderFeature;

struct GraphDeclarationContext final {
    RenderGraphBuilder& graph;
    GraphBlackboard& blackboard;
    u32 width{};
    u32 height{};
};

struct SemanticVersion {
    u16 major{1};
    u16 minor{};
    u16 patch{};
    [[nodiscard]] friend constexpr auto operator<=>(const SemanticVersion&, const SemanticVersion&) noexcept = default;
};

enum class FeatureMultiplicity : u8 { Once, Multiple };
enum class FeatureScope : u8 { Runtime, Scene, Pipeline, ViewFamily, View };
enum class ConfigType : u8 { Boolean, Integer, Number, String };
using ConfigValue = std::variant<bool, i64, f64, std::string>;

struct FeatureConfigField {
    std::string name;
    ConfigType type{ConfigType::String};
    bool required{};
    std::optional<ConfigValue> default_value;
};

struct FeatureConfigSchema {
    u32 version{1};
    std::vector<FeatureConfigField> fields;
};

struct RenderFeatureMetadata {
    StringId id;
    std::string debug_name;
    SemanticVersion version;
    FeatureMultiplicity multiplicity{FeatureMultiplicity::Once};
    FeatureScope scope{FeatureScope::Pipeline};
    std::vector<StringId> required_features;
    std::vector<StringId> optional_features;
    std::vector<StringId> conflicting_features;
    std::vector<StringId> required_capabilities;
    std::vector<StringId> insertion_points;
    std::vector<StringId> compatible_render_paths;
    FeatureConfigSchema config;
};

struct CompiledFeatureConfig {
    StringId feature;
    std::optional<std::string> instance;
    SemanticVersion factory_version;
    std::vector<std::pair<std::string, ConfigValue>> values;
    ContentHash hash;
};

[[nodiscard]] ContentHash HashFeatureConfig(const CompiledFeatureConfig& config);

class RenderFeature {
public:
    virtual ~RenderFeature() = default;
    [[nodiscard]] virtual StringId Id() const noexcept = 0;
    [[nodiscard]] virtual const std::optional<std::string>& Instance() const noexcept = 0;
    [[nodiscard]] virtual const CompiledFeatureConfig& Config() const noexcept = 0;

    // Called during frame-local graph declaration. Features may exchange typed
    // values through the blackboard; no scene/entity services are implied.
    [[nodiscard]] virtual Result<void> DeclareGraph(GraphDeclarationContext&) const {
        return Ok();
    }

    // Called once per feature in canonical pipeline order, never per object.
    [[nodiscard]] virtual Result<void> PrepareScene(const RenderScenePreparationContext&) const {
        return Ok();
    }

    virtual void OnSubmitted(rhi::SubmissionTicket) const {}

    [[nodiscard]] const ConfigValue* Setting(std::string_view name) const noexcept {
        const auto& values = Config().values;
        const auto found = std::ranges::lower_bound(values, name, {}, [](const auto& value) -> const std::string& { return value.first; });
        return found != values.end() && found->first == name ? &found->second : nullptr;
    }
};

struct FeatureServices {
    StandardFeatureServices* standard{};
    std::function<Result<ref<const RenderFeature>>(const CompiledFeatureConfig&)> acquire;
};

class RenderFeatureFactory {
public:
    virtual ~RenderFeatureFactory() = default;
    [[nodiscard]] virtual const RenderFeatureMetadata& Metadata() const noexcept = 0;
    [[nodiscard]] virtual Result<CompiledFeatureConfig> Compile(const std::map<std::string, ConfigValue, std::less<>>& values) const = 0;
    [[nodiscard]] virtual Result<ref<const RenderFeature>> Create(const CompiledFeatureConfig& config, const FeatureServices& services) const = 0;
};

class FeatureRegistry {
public:
    [[nodiscard]] Result<void> Register(ref<const RenderFeatureFactory> factory);
    [[nodiscard]] Result<void> RegisterCapability(std::string name);
    [[nodiscard]] Result<void> RegisterExtensionPoint(std::string name);
    [[nodiscard]] const RenderFeatureMetadata* Metadata(StringId id) const noexcept;
    [[nodiscard]] bool KnowsCapability(StringId id) const noexcept;
    [[nodiscard]] bool KnowsExtensionPoint(StringId id) const noexcept;
    [[nodiscard]] std::string_view CapabilityName(StringId id) const noexcept;
    [[nodiscard]] Result<CompiledFeatureConfig> Compile(StringId id, const std::map<std::string, ConfigValue, std::less<>>& values, std::optional<std::string> instance = std::nullopt) const;
    [[nodiscard]] Result<ref<const RenderFeature>> Create(const CompiledFeatureConfig& config, const FeatureServices& services = {}) const;
    [[nodiscard]] std::vector<RenderFeatureMetadata> Features() const;

    void Freeze() noexcept {
        frozen_ = true;
    }

    [[nodiscard]] bool Frozen() const noexcept {
        return frozen_;
    }

private:
    std::map<StringId, ref<const RenderFeatureFactory>> factories_;
    std::map<StringId, std::string> capabilities_;
    std::map<StringId, std::string> extension_points_;
    bool frozen_{};
};

[[nodiscard]] Result<void> RegisterStandardFeatures(FeatureRegistry& registry);

} // namespace woki::gfx
