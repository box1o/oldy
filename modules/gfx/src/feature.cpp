#include <bit>
#include <algorithm>

#include <woki/gfx/advanced/feature.hpp>
#include <woki/gfx/advanced/render_graph.hpp>
#include <woki/gfx/advanced/pipeline_library.hpp>
#include <woki/gfx/advanced/standard_features.hpp>
#include "canonical_hash.hpp"

namespace woki::gfx {
namespace {

ContentHash Canonical(const CompiledFeatureConfig& config) {
    detail::CanonicalHashWriter out;
    out.Value(config.feature.Value());
    out.Value(config.instance.has_value());
    if (config.instance)
        out.Value(*config.instance);
    out.Value(config.factory_version.major);
    out.Value(config.factory_version.minor);
    out.Value(config.factory_version.patch);
    out.Value(static_cast<u64>(config.values.size()));
    for (const auto& [name, value] : config.values) {
        out.Value(name);
        out.Value(static_cast<u8>(value.index()));
        std::visit(
            [&](const auto& item) {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::same_as<T, bool>)
                    out.Value(item);
                else if constexpr (std::same_as<T, f64>)
                    out.Value(std::bit_cast<u64>(item));
                else if constexpr (std::same_as<T, std::string>)
                    out.Value(item);
                else
                    out.Value(item);
            },
            value
        );
    }
    return out.Finish();
}

bool Matches(const ConfigValue& value, const ConfigType type) {
    return (type == ConfigType::Boolean && std::holds_alternative<bool>(value)) || (type == ConfigType::Integer && std::holds_alternative<i64>(value)) || (type == ConfigType::Number && std::holds_alternative<f64>(value))
           || (type == ConfigType::String && std::holds_alternative<std::string>(value));
}

Result<void> ValidateCompiledConfig(const CompiledFeatureConfig& config, const RenderFeatureMetadata& metadata, const bool validate_instance) {
    if (config.feature != metadata.id || config.factory_version != metadata.version || config.hash != HashFeatureConfig(config) || !std::ranges::is_sorted(config.values, {}, &decltype(config.values)::value_type::first)
        || std::ranges::adjacent_find(config.values, {}, &decltype(config.values)::value_type::first) != config.values.end())
        return Err(ErrorCode::ValidationInvalidState, "compiled feature config has invalid identity, ordering, or hash");
    for (const auto& [name, value] : config.values) {
        const auto field = std::ranges::find(metadata.config.fields, name, &FeatureConfigField::name);
        if (field == metadata.config.fields.end() || !Matches(value, field->type))
            return Err(ErrorCode::ValidationInvalidState, "compiled feature config is outside its registered schema");
    }
    for (const auto& field : metadata.config.fields)
        if ((field.required || field.default_value) && std::ranges::find(config.values, field.name, &decltype(config.values)::value_type::first) == config.values.end())
            return Err(ErrorCode::ValidationInvalidState, "compiled feature config omits a required or defaulted field");
    if ((!validate_instance && config.instance) || (validate_instance && metadata.multiplicity == FeatureMultiplicity::Once && config.instance)
        || (validate_instance && metadata.multiplicity == FeatureMultiplicity::Multiple && (!config.instance || config.instance->empty())))
        return Err(ErrorCode::ValidationInvalidState, "compiled feature config has invalid instance identity");
    return Ok();
}

} // namespace

ContentHash HashFeatureConfig(const CompiledFeatureConfig& config) {
    return Canonical(config);
}

Result<void> FeatureRegistry::Register(ref<const RenderFeatureFactory> factory) {
    if (frozen_)
        return Err(ErrorCode::InvalidState, "feature registry mutation is closed after startup");
    if (!factory)
        return Err(ErrorCode::InvalidArgument, "feature factory metadata is invalid");
    const auto& metadata = factory->Metadata();
    const auto enum_valid = metadata.multiplicity <= FeatureMultiplicity::Multiple && metadata.scope <= FeatureScope::View;
    if (metadata.id.Empty() || metadata.debug_name.empty() || metadata.id != StringId(metadata.debug_name) || metadata.version.major == 0 || !enum_valid || metadata.config.version == 0)
        return Err(ErrorCode::InvalidArgument, "feature factory metadata identity, version, scope, multiplicity, or config schema is invalid");
    const auto valid_ids = [&](const std::vector<StringId>& ids, bool reject_self) {
        std::set<StringId> unique;
        return std::ranges::all_of(ids, [&](StringId id) { return !id.Empty() && (!reject_self || id != metadata.id) && unique.insert(id).second; });
    };
    if (!valid_ids(metadata.required_features, true) || !valid_ids(metadata.optional_features, true) || !valid_ids(metadata.conflicting_features, true) || !valid_ids(metadata.required_capabilities, false)
        || !valid_ids(metadata.insertion_points, false) || !valid_ids(metadata.compatible_render_paths, false))
        return Err(ErrorCode::InvalidArgument, "feature factory metadata contains an empty, duplicate, or self reference");
    if (std::ranges::any_of(metadata.required_features, [&](StringId id) { return std::ranges::find(metadata.optional_features, id) != metadata.optional_features.end(); }))
        return Err(ErrorCode::InvalidArgument, "feature cannot require and optionally depend on the same feature");
    for (StringId capability : metadata.required_capabilities)
        if (!KnowsCapability(capability))
            return Err(ErrorCode::InvalidArgument, "feature metadata uses an unregistered capability");
    for (StringId point : metadata.insertion_points)
        if (!KnowsExtensionPoint(point))
            return Err(ErrorCode::InvalidArgument, "feature metadata uses an unregistered extension point");
    std::set<std::string, std::less<>> fields;
    for (const auto& field : metadata.config.fields)
        if (field.name.empty() || field.type > ConfigType::String || !fields.insert(field.name).second || (field.default_value && !Matches(*field.default_value, field.type)) || (field.required && field.default_value))
            return Err(ErrorCode::InvalidArgument, "feature config schema contains an invalid field");
    if (!factories_.emplace(metadata.id, std::move(factory)).second)
        return Err(ErrorCode::InvalidArgument, "feature factory is already registered");
    return Ok();
}

Result<void> FeatureRegistry::RegisterCapability(std::string name) {
    if (frozen_)
        return Err(ErrorCode::InvalidState, "feature registry mutation is closed after startup");
    if (name.empty() || !capabilities_.emplace(StringId(name), name).second)
        return Err(ErrorCode::InvalidArgument, "capability is empty or already registered");
    return Ok();
}

Result<void> FeatureRegistry::RegisterExtensionPoint(std::string name) {
    if (frozen_)
        return Err(ErrorCode::InvalidState, "feature registry mutation is closed after startup");
    if (name.empty() || !extension_points_.emplace(StringId(name), name).second)
        return Err(ErrorCode::InvalidArgument, "extension point is empty or already registered");
    return Ok();
}

const RenderFeatureMetadata* FeatureRegistry::Metadata(const StringId id) const noexcept {
    const auto found = factories_.find(id);
    return found == factories_.end() ? nullptr : &found->second->Metadata();
}

bool FeatureRegistry::KnowsCapability(const StringId id) const noexcept {
    return capabilities_.contains(id);
}

bool FeatureRegistry::KnowsExtensionPoint(const StringId id) const noexcept {
    return extension_points_.contains(id);
}

std::string_view FeatureRegistry::CapabilityName(const StringId id) const noexcept {
    const auto found = capabilities_.find(id);
    return found == capabilities_.end() ? std::string_view{} : found->second;
}

Result<CompiledFeatureConfig> FeatureRegistry::Compile(const StringId id, const std::map<std::string, ConfigValue, std::less<>>& values, std::optional<std::string> instance) const {
    const auto found = factories_.find(id);
    if (found == factories_.end())
        return Err(ErrorCode::InvalidArgument, "unknown render feature");
    auto compiled = found->second->Compile(values);
    if (!compiled)
        return compiled;
    const auto& metadata = found->second->Metadata();
    TRY_VOID(ValidateCompiledConfig(*compiled, metadata, false));
    for (const auto& [name, value] : values) {
        const auto output = std::ranges::find(compiled->values, name, &decltype(compiled->values)::value_type::first);
        if (output == compiled->values.end() || output->second != value)
            return Err(ErrorCode::ValidationInvalidState, "feature factory changed or omitted an explicit config value");
    }
    if ((metadata.multiplicity == FeatureMultiplicity::Once && instance) || (metadata.multiplicity == FeatureMultiplicity::Multiple && (!instance || instance->empty())))
        return Err(ErrorCode::InvalidArgument, "feature instance does not match metadata multiplicity");
    compiled->instance = std::move(instance);
    compiled->hash = HashFeatureConfig(*compiled);
    return compiled;
}

Result<ref<const RenderFeature>> FeatureRegistry::Create(const CompiledFeatureConfig& config, const FeatureServices& services) const {
    const auto found = factories_.find(config.feature);
    if (found == factories_.end())
        return Err(ErrorCode::InvalidArgument, "unknown render feature");
    const auto& metadata = found->second->Metadata();
    TRY_VOID(ValidateCompiledConfig(config, metadata, true));
    auto verified = found->second->Compile(std::map<std::string, ConfigValue, std::less<>>(config.values.begin(), config.values.end()));
    if (!verified || !ValidateCompiledConfig(*verified, metadata, false) || verified->values != config.values)
        return Err(ErrorCode::ValidationInvalidState, "compiled feature config is not reproducible by its factory");
    auto created = found->second->Create(config, services);
    if (!created)
        return created;
    if (!*created || (*created)->Id() != config.feature || (*created)->Instance() != config.instance || (*created)->Config().feature != config.feature || (*created)->Config().instance != config.instance
        || (*created)->Config().values != config.values || (*created)->Config().hash != config.hash || (*created)->Config().factory_version != config.factory_version)
        return Err(ErrorCode::ValidationInvalidState, "feature factory created an instance with mismatched identity or config");
    return created;
}

std::vector<RenderFeatureMetadata> FeatureRegistry::Features() const {
    std::vector<RenderFeatureMetadata> result;
    for (const auto& [id, factory] : factories_) {
        static_cast<void>(id);
        result.push_back(factory->Metadata());
    }
    return result;
}

Result<void> RegisterStandardFeatures(FeatureRegistry& registry) {
    for (const std::string name : {"hdr", "compute", "depth-texture", "texture-compression-bc", "timestamp-query"})
        TRY_VOID(registry.RegisterCapability(name));
    for (const std::string name : {"depth-prepass", "opaque", "transparent", "post-process", "overlay"})
        TRY_VOID(registry.RegisterExtensionPoint(name));

    return RegisterConcreteStandardFeatures(registry);
}

Result<void> DeclarePipelineGraph(const PipelineInstance& instance, RenderGraphBuilder& graph, const u32 width, const u32 height) {
    if (width == 0 || height == 0)
        return Err(ErrorCode::ValidationOutOfRange, "graph declaration extent must be non-zero");
    GraphDeclarationContext context{graph, graph.Blackboard(), width, height};
    for (const auto& feature : instance.features) {
        if (feature == nullptr)
            return Err(ErrorCode::ValidationNullValue, "pipeline instance contains a null feature");
        TRY_VOID(feature->DeclareGraph(context));
    }
    return Ok();
}

Result<void> PreparePipelineScene(const PipelineInstance& instance, const RenderScenePreparationContext& context) {
    for (const auto& feature : instance.features) {
        if (feature == nullptr)
            return Err(ErrorCode::ValidationNullValue, "pipeline instance contains a null feature");
        TRY_VOID(feature->PrepareScene(context));
    }
    return Ok();
}

} // namespace woki::gfx
