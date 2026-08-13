#include "internal.hpp"

namespace woki::gfx::feature_detail {
namespace {
bool Matches(const ConfigValue& value, const ConfigType type) {
    return (type == ConfigType::Boolean && std::holds_alternative<bool>(value)) || (type == ConfigType::Integer && std::holds_alternative<i64>(value)) || (type == ConfigType::Number && std::holds_alternative<f64>(value))
           || (type == ConfigType::String && std::holds_alternative<std::string>(value));
}

using CreateFeature = std::function<ref<const RenderFeature>(CompiledFeatureConfig, StandardFeatureServices*)>;

class Factory final : public RenderFeatureFactory {
public:
    Factory(RenderFeatureMetadata metadata, CreateFeature create)
        : metadata_(std::move(metadata)),
          create_(std::move(create)) {}

    const RenderFeatureMetadata& Metadata() const noexcept override {
        return metadata_;
    }

    Result<CompiledFeatureConfig> Compile(const std::map<std::string, ConfigValue, std::less<>>& input) const override {
        CompiledFeatureConfig result{metadata_.id, std::nullopt, metadata_.version, {}, {}};
        for (const auto& [name, value] : input) {
            const auto field = std::ranges::find(metadata_.config.fields, name, &FeatureConfigField::name);
            if (field == metadata_.config.fields.end() || !Matches(value, field->type))
                return Err(ErrorCode::InvalidArgument, "invalid standard feature setting: " + name);
        }
        for (const auto& field : metadata_.config.fields) {
            const auto value = input.find(field.name);
            if (value != input.end())
                result.values.emplace_back(field.name, value->second);
            else if (field.default_value)
                result.values.emplace_back(field.name, *field.default_value);
            else if (field.required)
                return Err(ErrorCode::InvalidArgument, "missing standard feature setting: " + field.name);
        }
        std::ranges::sort(result.values, {}, &decltype(result.values)::value_type::first);
        result.hash = HashFeatureConfig(result);
        return Ok(std::move(result));
    }

    Result<ref<const RenderFeature>> Create(const CompiledFeatureConfig& config, const FeatureServices& services) const override {
        if (services.standard == nullptr)
            return Err(ErrorCode::InvalidState, "concrete standard features require RenderRuntime services");
        return Ok(create_(config, services.standard));
    }

private:
    RenderFeatureMetadata metadata_;
    CreateFeature create_;
};

} // namespace

RenderFeatureMetadata Metadata(std::string name, FeatureScope scope, std::vector<StringId> required, std::vector<FeatureConfigField> fields) {
    RenderFeatureMetadata value;
    value.id = StringId(name);
    value.debug_name = std::move(name);
    value.scope = scope;
    value.required_features = std::move(required);
    value.compatible_render_paths = {StringId("forward"), StringId("mobile-forward"), StringId("compatibility-forward")};
    value.insertion_points = {StringId("opaque")};
    value.config.fields = std::move(fields);
    return value;
}

Result<void> RegisterFactory(FeatureRegistry& registry, RenderFeatureMetadata metadata, CreateFeature create) {
    return registry.Register(createRef<Factory>(std::move(metadata), std::move(create)));
}

} // namespace woki::gfx::feature_detail

namespace woki::gfx {

Result<void> RegisterConcreteStandardFeatures(FeatureRegistry& registry) {
    TRY_VOID(feature_detail::RegisterMeshFeatures(registry));
    TRY_VOID(feature_detail::RegisterDepthFeatures(registry));
    TRY_VOID(feature_detail::RegisterLightingFeatures(registry));
    TRY_VOID(feature_detail::RegisterShadowFeatures(registry));
    TRY_VOID(feature_detail::RegisterEnvironmentFeatures(registry));
    TRY_VOID(feature_detail::RegisterTemporalFeatures(registry));
    TRY_VOID(feature_detail::RegisterPostFeatures(registry));
    return feature_detail::RegisterPresentationFeatures(registry);
}

} // namespace woki::gfx
