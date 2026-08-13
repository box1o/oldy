#include <bit>
#include <queue>
#include <algorithm>
#include <woki/config.hpp>

#include <woki/gfx/advanced/pipeline.hpp>
#include "canonical_hash.hpp"

namespace woki::gfx {
namespace {

using Json = config::Json;

void Add(std::vector<PipelineDiagnostic>& out, const asset::AssetPath& path, std::string code, std::string message, std::string pointer = {}, u64 offset = 0) {
    out.push_back({std::move(code), DiagnosticSeverity::Error, std::move(message), std::move(pointer), path, SourceRange{path, offset, 1, 0, 0}});
}

bool Only(const Json& value, std::initializer_list<std::string_view> names) {
    return value.is_object() && std::ranges::all_of(value.items(), [&](const auto& item) { return std::ranges::find(names, item.first) != names.end(); });
}

std::optional<ConfigValue> Scalar(const Json& value) {
    if (value.is_boolean())
        return value.get<bool>();
    if (value.is_number_integer())
        return value.get<i64>();
    if (value.is_number_float())
        return value.get<f64>();
    if (value.is_string())
        return value.get<std::string>();
    return std::nullopt;
}

ContentHash ConfigHash(std::string_view source, std::string_view family, u32 version) {
    auto document = config::Document::Parse(source, {}, config::ParsePolicy{true, true, false, {}});
    return document ? config::CanonicalHash(*document, family, version) : Sha256(source);
}

Result<asset::AssetPath> Relative(const asset::AssetPath& parent, std::string_view child) {
    if (child.empty() || child.front() == '/' || child.contains('\\') || child.contains(':'))
        return Err(ErrorCode::InvalidArgument, "reference must be a safe relative VFS path");
    const auto slash = parent.String().rfind('/');
    return asset::AssetPath::Parse(slash == std::string::npos ? child : parent.String().substr(0, slash + 1) + std::string(child));
}

template <typename T>
bool ParseRoot(const asset::AssetPath& path, std::string_view source, PipelineParseResult<T>& result, Json& root, std::initializer_list<std::string_view> keys) {
    auto parsed = Json::Parse(source, path.String());
    if (!parsed) {
        const auto& error = parsed.error().front();
        Add(result.diagnostics, path, "PIP1001", error.message, {}, error.range.begin.byte);
        return false;
    }
    root = std::move(*parsed);
    if (!root.is_object()) {
        Add(result.diagnostics, path, "PIP1002", "source root must be an object", "");
        return false;
    }
    if (!Only(root, keys))
        Add(result.diagnostics, path, "PIP1003", "source contains an unknown key", "");
    return true;
}

void ParseSettings(const asset::AssetPath& path, const Json& value, std::string pointer, std::map<std::string, ConfigValue, std::less<>>& output, std::vector<PipelineDiagnostic>& diagnostics) {
    if (!value.is_object()) {
        Add(diagnostics, path, "PIP1010", "settings must be an object", std::move(pointer));
        return;
    }
    for (const auto& [name, item] : value.items()) {
        auto scalar = Scalar(item);
        if (!scalar)
            Add(diagnostics, path, "PIP1011", "feature settings must be boolean, integer, number, or string", pointer + '/' + name);
        else
            output.emplace(name, std::move(*scalar));
    }
}

bool ReadSchemaName(const asset::AssetPath& path, const Json& root, u32 schema, u32& output_schema, std::string& name, std::vector<PipelineDiagnostic>& diagnostics) {
    if (!root.contains("schema") || !root["schema"].is_number_unsigned() || root["schema"] != schema)
        Add(diagnostics, path, "PIP1004", "schema has an unsupported value", "/schema");
    else
        output_schema = root["schema"].get<u32>();
    if (!root.contains("name") || !root["name"].is_string() || root["name"].get_ref<const std::string&>().empty())
        Add(diagnostics, path, "PIP1005", "name must be a non-empty string", "/name");
    else
        name = root["name"].get<std::string>();
    return diagnostics.empty();
}

ContentHash Canonical(const RenderPipelineIR& pipeline) {
    detail::CanonicalHashWriter out;
    out.Hash(pipeline.asset_id);
    out.Int(pipeline.name_id.Value());
    out.String(pipeline.debug_name);
    out.Int(pipeline.render_path.Value());
    out.String(pipeline.render_path_name);
    out.Int(static_cast<u64>(pipeline.targets.color.size()));
    for (auto format : pipeline.targets.color)
        out.Int(static_cast<u8>(format));
    out.Int(static_cast<u8>(pipeline.targets.depth.has_value()));
    if (pipeline.targets.depth)
        out.Int(static_cast<u8>(*pipeline.targets.depth));
    out.Int(pipeline.targets.samples);
    out.Int(static_cast<u64>(pipeline.features.size()));
    for (const auto& feature : pipeline.features) {
        out.Int(feature.id.Value());
        out.String(feature.debug_name);
        out.Optional(feature.instance);
        out.Int(static_cast<u8>(feature.scope));
        out.Int(static_cast<u8>(feature.multiplicity));
        out.Int(feature.factory_version.major);
        out.Int(feature.factory_version.minor);
        out.Int(feature.factory_version.patch);
        out.Hash(feature.config.hash);
    }
    out.Int(static_cast<u64>(pipeline.extensions.size()));
    for (const auto& extension : pipeline.extensions) {
        out.Int(extension.point.Value());
        out.String(extension.debug_name);
        out.Int(static_cast<u64>(extension.features.size()));
        for (const auto& feature : extension.features) {
            out.Int(feature.id.Value());
            out.String(feature.debug_name);
            out.Optional(feature.instance);
        }
    }
    out.Int(static_cast<u8>(pipeline.quality_id.has_value()));
    if (pipeline.quality_id)
        out.Hash(*pipeline.quality_id);
    out.Int(static_cast<u8>(pipeline.quality_path.has_value()));
    if (pipeline.quality_path)
        out.String(pipeline.quality_path->String());
    out.Int(static_cast<u64>(pipeline.fallbacks.size()));
    for (const auto& fallback : pipeline.fallbacks) {
        out.Hash(fallback.target);
        out.String(fallback.path.String());
        out.Int(static_cast<u64>(fallback.missing_capabilities.size()));
        for (StringId capability : fallback.missing_capabilities)
            out.Int(capability.Value());
        out.Int(static_cast<u64>(fallback.capability_names.size()));
        for (const auto& name : fallback.capability_names)
            out.String(name);
    }
    out.Int(static_cast<u64>(pipeline.required_capabilities.size()));
    for (StringId capability : pipeline.required_capabilities)
        out.Int(capability.Value());
    out.Int(static_cast<u64>(pipeline.required_capability_names.size()));
    for (const auto& name : pipeline.required_capability_names)
        out.String(name);
    out.Int(static_cast<u64>(pipeline.dependencies.size()));
    for (const auto& dependency : pipeline.dependencies) {
        out.String(dependency.path.String());
        out.Hash(dependency.hash);
    }
    out.Hash(pipeline.root_source_hash);
    return out.Finish();
}

} // namespace

std::optional<PipelineTargetFormat> ParsePipelineTargetFormat(const std::string_view name) noexcept {
    constexpr std::pair<std::string_view, PipelineTargetFormat> formats[] = {{"surface", PipelineTargetFormat::Surface}, {"rgba8unorm", PipelineTargetFormat::RGBA8Unorm},
        {"rgba8unorm-srgb", PipelineTargetFormat::RGBA8UnormSrgb}, {"bgra8unorm", PipelineTargetFormat::BGRA8Unorm}, {"bgra8unorm-srgb", PipelineTargetFormat::BGRA8UnormSrgb},
        {"rgba16float", PipelineTargetFormat::RGBA16Float}, {"depth24plus", PipelineTargetFormat::Depth24Plus}, {"depth24plus-stencil8", PipelineTargetFormat::Depth24PlusStencil8},
        {"depth32float", PipelineTargetFormat::Depth32Float}};
    const auto found = std::ranges::find(formats, name, &std::pair<std::string_view, PipelineTargetFormat>::first);
    return found == std::ranges::end(formats) ? std::nullopt : std::optional(found->second);
}

std::string_view PipelineTargetFormatName(const PipelineTargetFormat format) noexcept {
    switch (format) {
        case PipelineTargetFormat::Surface:
            return "surface";
        case PipelineTargetFormat::RGBA8Unorm:
            return "rgba8unorm";
        case PipelineTargetFormat::RGBA8UnormSrgb:
            return "rgba8unorm-srgb";
        case PipelineTargetFormat::BGRA8Unorm:
            return "bgra8unorm";
        case PipelineTargetFormat::BGRA8UnormSrgb:
            return "bgra8unorm-srgb";
        case PipelineTargetFormat::RGBA16Float:
            return "rgba16float";
        case PipelineTargetFormat::Depth24Plus:
            return "depth24plus";
        case PipelineTargetFormat::Depth24PlusStencil8:
            return "depth24plus-stencil8";
        case PipelineTargetFormat::Depth32Float:
            return "depth32float";
    }
    return {};
}

std::optional<rhi::TextureFormat> ToRhiTextureFormat(const PipelineTargetFormat format) noexcept {
    switch (format) {
        case PipelineTargetFormat::Surface:
            return std::nullopt;
        case PipelineTargetFormat::RGBA8Unorm:
            return rhi::TextureFormat::RGBA8Unorm;
        case PipelineTargetFormat::RGBA8UnormSrgb:
            return rhi::TextureFormat::RGBA8UnormSrgb;
        case PipelineTargetFormat::BGRA8Unorm:
            return rhi::TextureFormat::BGRA8Unorm;
        case PipelineTargetFormat::BGRA8UnormSrgb:
            return rhi::TextureFormat::BGRA8UnormSrgb;
        case PipelineTargetFormat::RGBA16Float:
            return rhi::TextureFormat::RGBA16Float;
        case PipelineTargetFormat::Depth24Plus:
            return rhi::TextureFormat::Depth24Plus;
        case PipelineTargetFormat::Depth24PlusStencil8:
            return rhi::TextureFormat::Depth24PlusStencil8;
        case PipelineTargetFormat::Depth32Float:
            return rhi::TextureFormat::Depth32Float;
    }
    return std::nullopt;
}

PipelineParseResult<PipelineSource> ParsePipeline(const asset::AssetPath& path, const std::string_view jsonc) {
    PipelineParseResult<PipelineSource> result;
    Json root;
    if (!ParseRoot(path, jsonc, result, root, {"$schema", "schema", "name", "renderPath", "targets", "features", "extensionPoints", "qualityProfile", "fallbacks"}))
        return result;
    {
        ReadSchemaName(path, root, kPipelineSchema, result.value.schema, result.value.name, result.diagnostics);
        if (!root.contains("renderPath") || !root["renderPath"].is_string() || root["renderPath"].get_ref<const std::string&>().empty())
            Add(result.diagnostics, path, "PIP1006", "renderPath must be a non-empty string", "/renderPath");
        else
            result.value.render_path = root["renderPath"].get<std::string>();
        if (!root.contains("targets") || !Only(root["targets"], {"color", "depth", "samples"})) {
            Add(result.diagnostics, path, "PIP1007", "targets must contain only color, depth, and samples", "/targets");
        } else {
            const auto& targets = root["targets"];
            if (!targets.contains("color") || !targets["color"].is_array() || targets["color"].empty() || !std::ranges::all_of(targets["color"], [](const Json& item) { return item.is_string(); }))
                Add(result.diagnostics, path, "PIP1008", "targets.color must be a non-empty string array", "/targets/color");
            else
                for (std::size_t index = 0; index < targets["color"].size(); ++index) {
                    const auto name = targets["color"][index].get<std::string>();
                    const auto format = ParsePipelineTargetFormat(name);
                    if (!format || *format == PipelineTargetFormat::Depth24Plus || *format == PipelineTargetFormat::Depth24PlusStencil8 || *format == PipelineTargetFormat::Depth32Float)
                        Add(result.diagnostics, path, "PIP1026", "unsupported color target format: " + name, "/targets/color/" + std::to_string(index));
                    else
                        result.value.targets.color.push_back(*format);
                }
            if (targets.contains("depth")) {
                if (!targets["depth"].is_string())
                    Add(result.diagnostics, path, "PIP1009", "targets.depth must be a string", "/targets/depth");
                else {
                    const auto name = targets["depth"].get<std::string>();
                    const auto format = ParsePipelineTargetFormat(name);
                    if (!format || (*format != PipelineTargetFormat::Depth24Plus && *format != PipelineTargetFormat::Depth24PlusStencil8 && *format != PipelineTargetFormat::Depth32Float))
                        Add(result.diagnostics, path, "PIP1027", "unsupported depth target format: " + name, "/targets/depth");
                    else
                        result.value.targets.depth = *format;
                }
            }
            if (targets.contains("samples")) {
                if (!targets["samples"].is_number_unsigned() || (targets["samples"].get<u32>() != 1 && targets["samples"].get<u32>() != 4))
                    Add(result.diagnostics, path, "PIP1012", "targets.samples must be 1 or 4", "/targets/samples");
                else
                    result.value.targets.samples = targets["samples"].get<u32>();
            }
        }
        if (!root.contains("features") || !root["features"].is_array())
            Add(result.diagnostics, path, "PIP1013", "features must be an array", "/features");
        else
            for (std::size_t index = 0; index < root["features"].size(); ++index) {
                const auto& item = root["features"][index];
                const std::string pointer = "/features/" + std::to_string(index);
                if (!Only(item, {"id", "instance", "config", "settings"}) || !item.contains("id") || !item["id"].is_string()) {
                    Add(result.diagnostics, path, "PIP1014", "feature requires a string id and supported keys", pointer);
                    continue;
                }
                FeatureSource feature;
                feature.id = item["id"].get<std::string>();
                if (item.contains("instance")) {
                    if (!item["instance"].is_string() || item["instance"].get_ref<const std::string&>().empty())
                        Add(result.diagnostics, path, "PIP1028", "feature instance must be a non-empty stable string", pointer + "/instance");
                    else
                        feature.instance = item["instance"].get<std::string>();
                }
                if (item.contains("config")) {
                    if (!item["config"].is_string())
                        Add(result.diagnostics, path, "PIP1015", "feature config must be a relative path", pointer + "/config");
                    else {
                        auto parsed = Relative(path, item["config"].get_ref<const std::string&>());
                        if (!parsed)
                            Add(result.diagnostics, path, "PIP1016", parsed.error().Message().data(), pointer + "/config");
                        else
                            feature.config = std::move(*parsed);
                    }
                }
                if (item.contains("settings"))
                    ParseSettings(path, item["settings"], pointer + "/settings", feature.settings, result.diagnostics);
                result.value.features.push_back(std::move(feature));
            }
        if (root.contains("extensionPoints")) {
            if (!root["extensionPoints"].is_object())
                Add(result.diagnostics, path, "PIP1017", "extensionPoints must be an object", "/extensionPoints");
            else
                for (const auto& [point, list] : root["extensionPoints"].items()) {
                    if (!list.is_array()) {
                        Add(result.diagnostics, path, "PIP1018", "extension point values must be arrays", "/extensionPoints/" + point);
                        continue;
                    }
                    for (std::size_t index = 0; index < list.size(); ++index) {
                        const auto& reference = list[index];
                        if (reference.is_string())
                            result.value.extension_points[point].push_back({reference.get<std::string>(), std::nullopt});
                        else if (Only(reference, {"id", "instance"}) && reference.contains("id") && reference["id"].is_string() && reference.contains("instance") && reference["instance"].is_string()
                                 && !reference["instance"].get_ref<const std::string&>().empty())
                            result.value.extension_points[point].push_back({reference["id"].get<std::string>(), reference["instance"].get<std::string>()});
                        else
                            Add(result.diagnostics, path, "PIP1018", "extension feature must be an id string or an {id, instance} object", "/extensionPoints/" + point + '/' + std::to_string(index));
                    }
                }
        }
        if (root.contains("qualityProfile")) {
            if (!root["qualityProfile"].is_string())
                Add(result.diagnostics, path, "PIP1019", "qualityProfile must be a relative path", "/qualityProfile");
            else {
                auto parsed = Relative(path, root["qualityProfile"].get_ref<const std::string&>());
                if (!parsed)
                    Add(result.diagnostics, path, "PIP1020", parsed.error().Message().data(), "/qualityProfile");
                else
                    result.value.quality_profile = std::move(*parsed);
            }
        }
        if (root.contains("fallbacks")) {
            if (!root["fallbacks"].is_array())
                Add(result.diagnostics, path, "PIP1021", "fallbacks must be an array", "/fallbacks");
            else
                for (std::size_t index = 0; index < root["fallbacks"].size(); ++index) {
                    const auto& item = root["fallbacks"][index];
                    const std::string pointer = "/fallbacks/" + std::to_string(index);
                    if (!Only(item, {"pipeline", "missingCapabilities"}) || !item.contains("pipeline") || !item["pipeline"].is_string() || !item.contains("missingCapabilities")
                        || !item["missingCapabilities"].is_array()) {
                        Add(result.diagnostics, path, "PIP1022", "fallback requires pipeline and missingCapabilities", pointer);
                        continue;
                    }
                    auto target = Relative(path, item["pipeline"].get_ref<const std::string&>());
                    if (!target) {
                        Add(result.diagnostics, path, "PIP1023", target.error().Message().data(), pointer + "/pipeline");
                        continue;
                    }
                    FallbackSource fallback{std::move(*target), {}};
                    for (const auto& capability : item["missingCapabilities"])
                        if (!capability.is_string())
                            Add(result.diagnostics, path, "PIP1024", "fallback capabilities must be strings", pointer + "/missingCapabilities");
                        else
                            fallback.missing_capabilities.push_back(capability.get<std::string>());
                    result.value.fallbacks.push_back(std::move(fallback));
                }
        }
    }
    return result;
}

PipelineParseResult<FeatureConfigSource> ParseFeatureConfig(const asset::AssetPath& path, const std::string_view jsonc) {
    PipelineParseResult<FeatureConfigSource> result;
    Json root;
    if (!ParseRoot(path, jsonc, result, root, {"$schema", "schema", "feature", "settings"}))
        return result;
    {
        if (!root.contains("schema") || !root["schema"].is_number_unsigned() || root["schema"] != kFeatureConfigSchema)
            Add(result.diagnostics, path, "PIP1101", "feature config schema must be 1", "/schema");
        if (!root.contains("feature") || !root["feature"].is_string())
            Add(result.diagnostics, path, "PIP1102", "feature must be a string", "/feature");
        else
            result.value.feature = root["feature"].get<std::string>();
        if (!root.contains("settings"))
            Add(result.diagnostics, path, "PIP1103", "settings is required", "/settings");
        else
            ParseSettings(path, root["settings"], "/settings", result.value.settings, result.diagnostics);
    }
    return result;
}

PipelineParseResult<QualitySource> ParseQuality(const asset::AssetPath& path, const std::string_view jsonc) {
    PipelineParseResult<QualitySource> result;
    Json root;
    if (!ParseRoot(path, jsonc, result, root, {"$schema", "schema", "name", "features"}))
        return result;
    {
        ReadSchemaName(path, root, kQualitySchema, result.value.schema, result.value.name, result.diagnostics);
        if (!root.contains("features") || !root["features"].is_object())
            Add(result.diagnostics, path, "PIP1201", "quality features must be an object", "/features");
        else
            for (const auto& [id, item] : root["features"].items()) {
                if (!Only(item, {"enabled", "settings"})) {
                    Add(result.diagnostics, path, "PIP1202", "quality feature contains an unknown key", "/features/" + id);
                    continue;
                }
                QualityFeatureOverride override;
                if (item.contains("enabled")) {
                    if (!item["enabled"].is_boolean())
                        Add(result.diagnostics, path, "PIP1203", "enabled must be boolean", "/features/" + id + "/enabled");
                    else
                        override.enabled = item["enabled"].get<bool>();
                }
                if (item.contains("settings"))
                    ParseSettings(path, item["settings"], "/features/" + id + "/settings", override.settings, result.diagnostics);
                result.value.features.emplace(id, std::move(override));
            }
    }
    return result;
}

namespace {

using FeatureKey = std::pair<std::string, std::string>;

bool ConfigMatches(const ConfigValue& value, ConfigType type) {
    return (type == ConfigType::Boolean && std::holds_alternative<bool>(value)) || (type == ConfigType::Integer && std::holds_alternative<i64>(value)) || (type == ConfigType::Number && std::holds_alternative<f64>(value))
           || (type == ConfigType::String && std::holds_alternative<std::string>(value));
}

class CompilationContext {
public:
    CompilationContext(const asset::Vfs& vfs, const FeatureRegistry& registry, PipelineCompileResult& result)
        : vfs_(vfs),
          registry_(registry),
          result_(result) {}

    std::optional<RenderPipelineIR> Build(const asset::AssetPath& path, u32 depth = 0) {
        if (depth > 32) {
            Add(result_.diagnostics, path, "PIP2020", "fallback depth limit exceeded");
            return std::nullopt;
        }
        if (active_.contains(path)) {
            Add(result_.diagnostics, path, "PIP2017", "fallback cycle detected: " + path.String());
            return std::nullopt;
        }
        if (const auto found = cache_.find(path); found != cache_.end())
            return found->second;
        if (++nodes_ > 256) {
            Add(result_.diagnostics, path, "PIP2021", "fallback total-node limit exceeded");
            return std::nullopt;
        }
        active_.insert(path);
        auto built = BuildOne(path, depth);
        active_.erase(path);
        if (built)
            cache_.emplace(path, *built);
        return built;
    }

    void CanonicalDependencies(RenderPipelineIR& root) const {
        root.dependencies.clear();
        for (const auto& [path, hash] : dependencies_)
            root.dependencies.push_back({path, hash});
    }

private:
    void Dependency(const asset::AssetPath& path, const ContentHash& hash) {
        dependencies_.insert_or_assign(path, hash);
    }

    std::optional<RenderPipelineIR> BuildOne(const asset::AssetPath& path, u32 depth) {
        auto source_text = vfs_.ReadText(path);
        if (!source_text) {
            Add(result_.diagnostics, path, "PIP2001", source_text.error().Message().data());
            return std::nullopt;
        }
        const ContentHash source_hash = ConfigHash(*source_text, "pipeline", kPipelineSchema);
        Dependency(path, source_hash);
        auto parsed = ParsePipeline(path, *source_text);
        result_.diagnostics.insert(result_.diagnostics.end(), parsed.diagnostics.begin(), parsed.diagnostics.end());
        if (!parsed.Valid())
            return std::nullopt;
        PipelineSource source = std::move(parsed.value);
        RenderPipelineIR ir;
        ir.asset_id = asset::AssetId::FromName("engine://" + path.String());
        ir.name_id = StringId(source.name);
        ir.debug_name = source.name;
        ir.render_path = StringId(source.render_path);
        ir.render_path_name = source.render_path;
        ir.targets = source.targets;
        ir.root_source_hash = source_hash;
        const auto known_features = registry_.Features();
        if (!std::ranges::any_of(known_features, [&](const auto& metadata) { return std::ranges::find(metadata.compatible_render_paths, ir.render_path) != metadata.compatible_render_paths.end(); }))
            Add(result_.diagnostics, path, "PIP2019", "unknown render path: " + source.render_path, "/renderPath");

        QualitySource quality;
        if (source.quality_profile) {
            auto text = vfs_.ReadText(*source.quality_profile);
            if (!text)
                Add(result_.diagnostics, path, "PIP2002", "quality profile cannot be read", "/qualityProfile");
            else {
                Dependency(*source.quality_profile, ConfigHash(*text, "quality", kQualitySchema));
                auto parsed_quality = ParseQuality(*source.quality_profile, *text);
                result_.diagnostics.insert(result_.diagnostics.end(), parsed_quality.diagnostics.begin(), parsed_quality.diagnostics.end());
                quality = std::move(parsed_quality.value);
                ir.quality_path = *source.quality_profile;
                ir.quality_id = asset::AssetId::FromName("engine://" + source.quality_profile->String());
            }
        }
        for (const auto& [name, override] : quality.features) {
            const auto* metadata = registry_.Metadata(StringId(name));
            if (!metadata) {
                Add(result_.diagnostics, path, "PIP2022", "quality profile names an unknown feature: " + name, "/qualityProfile");
                continue;
            }
            for (const auto& [key, value] : override.settings) {
                const auto field = std::ranges::find(metadata->config.fields, key, &FeatureConfigField::name);
                if (field == metadata->config.fields.end() || !ConfigMatches(value, field->type))
                    Add(result_.diagnostics, path, "PIP2023", "quality profile setting is unknown or has the wrong type: " + name + '.' + key, "/qualityProfile");
            }
        }

        std::map<FeatureKey, FeatureSource> enabled;
        for (auto& feature : source.features) {
            const auto* metadata = registry_.Metadata(StringId(feature.id));
            if (!metadata) {
                Add(result_.diagnostics, path, "PIP2004", "unknown feature: " + feature.id, "/features");
                continue;
            }
            if ((metadata->multiplicity == FeatureMultiplicity::Once && feature.instance) || (metadata->multiplicity == FeatureMultiplicity::Multiple && !feature.instance)) {
                Add(result_.diagnostics, path, "PIP2003",
                    metadata->multiplicity == FeatureMultiplicity::Once ? "single feature cannot have an instance: " + feature.id : "multiple feature requires an instance: " + feature.id, "/features");
                continue;
            }
            FeatureKey key{feature.id, feature.instance.value_or("")};
            if (!enabled.emplace(key, std::move(feature)).second)
                Add(result_.diagnostics, path, "PIP2003", "duplicate feature instance: " + key.first, "/features");
        }
        for (const auto& [id, override] : quality.features) {
            const auto* metadata = registry_.Metadata(StringId(id));
            if (!metadata)
                continue;
            if (override.enabled == false)
                std::erase_if(enabled, [&](const auto& item) { return item.first.first == id; });
            else if (override.enabled == true && std::ranges::none_of(enabled, [&](const auto& item) { return item.first.first == id; })) {
                if (metadata->multiplicity == FeatureMultiplicity::Multiple)
                    Add(result_.diagnostics, path, "PIP2024", "quality profile cannot create a multiple feature without an instance: " + id);
                else {
                    FeatureSource feature;
                    feature.id = id;
                    enabled.emplace(FeatureKey{id, ""}, std::move(feature));
                }
            }
        }

        std::map<FeatureKey, std::set<FeatureKey>> edges;
        std::map<FeatureKey, u32> indegree;
        const auto keys_for = [&](StringId id) {
            std::vector<FeatureKey> keys;
            for (const auto& [key, feature] : enabled)
                if (StringId(feature.id) == id)
                    keys.push_back(key);
            return keys;
        };
        for (const auto& [key, feature] : enabled) {
            const auto* metadata = registry_.Metadata(StringId(feature.id));
            indegree.try_emplace(key, 0);
            if (std::ranges::find(metadata->compatible_render_paths, ir.render_path) == metadata->compatible_render_paths.end())
                Add(result_.diagnostics, path, "PIP2005", "feature is incompatible with render path: " + feature.id);
            for (StringId capability : metadata->required_capabilities)
                ir.required_capabilities.push_back(capability);
            for (StringId required : metadata->required_features)
                if (keys_for(required).empty())
                    Add(result_.diagnostics, path, "PIP2007", "feature is missing a required feature: " + feature.id);
            for (StringId conflict : metadata->conflicting_features)
                if (!keys_for(conflict).empty())
                    Add(result_.diagnostics, path, "PIP2008", "conflicting features are enabled: " + feature.id);
        }
        for (const auto& [key, feature] : enabled) {
            const auto* metadata = registry_.Metadata(StringId(feature.id));
            auto add_dependencies = [&](const std::vector<StringId>& ids) {
                for (StringId id : ids)
                    for (const auto& dependency : keys_for(id))
                        if (edges[dependency].insert(key).second)
                            ++indegree[key];
            };
            add_dependencies(metadata->required_features);
            add_dependencies(metadata->optional_features);
        }
        for (const auto& [point, references] : source.extension_points) {
            const StringId point_id(point);
            if (!registry_.KnowsExtensionPoint(point_id)) {
                Add(result_.diagnostics, path, "PIP2009", "unknown extension point: " + point);
                continue;
            }
            ExtensionBinding binding{point_id, point, {}};
            std::optional<FeatureKey> previous;
            for (const auto& reference : references) {
                std::vector<FeatureKey> matches;
                for (const auto& [key, feature] : enabled)
                    if (feature.id == reference.id && (!reference.instance || feature.instance == reference.instance))
                        matches.push_back(key);
                if (matches.size() != 1) {
                    Add(result_.diagnostics, path, "PIP2010", "extension feature reference is disabled or ambiguous: " + reference.id);
                    continue;
                }
                const auto& key = matches.front();
                const auto* metadata = registry_.Metadata(StringId(reference.id));
                if (std::ranges::find(metadata->insertion_points, point_id) == metadata->insertion_points.end()) {
                    Add(result_.diagnostics, path, "PIP2011", "feature does not support extension point: " + reference.id);
                    continue;
                }
                binding.features.push_back({metadata->id, metadata->debug_name, enabled.at(key).instance});
                if (previous && edges[*previous].insert(key).second)
                    ++indegree[key];
                previous = key;
            }
            ir.extensions.push_back(std::move(binding));
        }

        std::set<FeatureKey> ready;
        for (const auto& [key, degree] : indegree)
            if (degree == 0)
                ready.insert(key);
        std::vector<FeatureKey> order;
        while (!ready.empty()) {
            FeatureKey key = *ready.begin();
            ready.erase(ready.begin());
            order.push_back(key);
            for (const auto& next : edges[key])
                if (--indegree[next] == 0)
                    ready.insert(next);
        }
        if (order.size() != indegree.size())
            Add(result_.diagnostics, path, "PIP2012", "feature dependency or extension-point cycle detected");
        for (const auto& key : order) {
            auto& feature = enabled.find(key)->second;
            const auto* metadata = registry_.Metadata(StringId(feature.id));
            std::map<std::string, ConfigValue, std::less<>> settings;
            if (feature.config) {
                auto text = vfs_.ReadText(*feature.config);
                if (!text)
                    Add(result_.diagnostics, path, "PIP2013", "feature config cannot be read: " + feature.config->String());
                else {
                    Dependency(*feature.config, ConfigHash(*text, "feature", kFeatureConfigSchema));
                    auto config = ParseFeatureConfig(*feature.config, *text);
                    result_.diagnostics.insert(result_.diagnostics.end(), config.diagnostics.begin(), config.diagnostics.end());
                    if (config.value.feature != feature.id)
                        Add(result_.diagnostics, path, "PIP2014", "feature config identity does not match request");
                    settings = std::move(config.value.settings);
                }
            }
            for (const auto& [name, value] : feature.settings)
                settings.insert_or_assign(name, value);
            if (const auto found = quality.features.find(feature.id); found != quality.features.end())
                for (const auto& [name, value] : found->second.settings)
                    settings.insert_or_assign(name, value);
            auto compiled = registry_.Compile(metadata->id, settings, feature.instance);
            if (!compiled)
                Add(result_.diagnostics, path, "PIP2015", compiled.error().Message().data());
            else {
                ir.features.push_back({metadata->id, metadata->debug_name, feature.instance, metadata->scope, metadata->multiplicity, metadata->version, std::move(*compiled)});
            }
        }

        if (std::ranges::find(ir.targets.color, PipelineTargetFormat::RGBA16Float) != ir.targets.color.end())
            ir.required_capabilities.emplace_back("hdr");
        std::ranges::sort(ir.required_capabilities);
        ir.required_capabilities.erase(std::ranges::unique(ir.required_capabilities).begin(), ir.required_capabilities.end());
        for (StringId capability : ir.required_capabilities) {
            if (!registry_.KnowsCapability(capability))
                Add(result_.diagnostics, path, "PIP2006", "pipeline requires an unknown capability");
            else
                ir.required_capability_names.emplace_back(registry_.CapabilityName(capability));
        }
        std::set<asset::AssetPath> fallback_paths;
        for (const auto& fallback : source.fallbacks) {
            FallbackRule rule{asset::AssetId::FromName("engine://" + fallback.pipeline.String()), fallback.pipeline, {}, fallback.missing_capabilities};
            if (!fallback_paths.insert(fallback.pipeline).second)
                Add(result_.diagnostics, path, "PIP2025", "duplicate fallback target: " + fallback.pipeline.String());
            for (const auto& name : fallback.missing_capabilities) {
                StringId capability(name);
                if (!registry_.KnowsCapability(capability))
                    Add(result_.diagnostics, path, "PIP2018", "unknown fallback capability: " + name);
                else
                    rule.missing_capabilities.push_back(capability);
            }
            std::ranges::sort(rule.missing_capabilities);
            rule.missing_capabilities.erase(std::ranges::unique(rule.missing_capabilities).begin(), rule.missing_capabilities.end());
            rule.capability_names.clear();
            for (StringId capability : rule.missing_capabilities)
                rule.capability_names.emplace_back(registry_.CapabilityName(capability));
            ir.fallbacks.push_back(std::move(rule));
            Build(fallback.pipeline, depth + 1);
        }
        std::ranges::sort(ir.extensions, {}, &ExtensionBinding::debug_name);
        ir.content_hash = HashPipeline(ir);
        return ir;
    }

    const asset::Vfs& vfs_;
    const FeatureRegistry& registry_;
    PipelineCompileResult& result_;
    std::set<asset::AssetPath> active_;
    std::map<asset::AssetPath, RenderPipelineIR> cache_;
    std::map<asset::AssetPath, ContentHash> dependencies_;
    u32 nodes_{};
};

} // namespace

PipelineCompileResult RenderPipelineCompiler::Compile(const asset::AssetPath& path) const {
    PipelineCompileResult result;
    CompilationContext context(vfs_, registry_, result);
    auto pipeline = context.Build(path);
    if (!pipeline || !result.diagnostics.empty())
        return result;
    context.CanonicalDependencies(*pipeline);
    pipeline->content_hash = HashPipeline(*pipeline);
    result.pipeline = std::move(*pipeline);
    return result;
}

ContentHash HashPipeline(const RenderPipelineIR& pipeline) {
    return Canonical(pipeline);
}

Result<asset::AssetPath> SelectSupportedPipeline(const asset::Vfs& vfs, const FeatureRegistry& registry, const asset::AssetPath& root, CapabilitySet capabilities) {
    std::ranges::sort(capabilities);
    capabilities.erase(std::ranges::unique(capabilities).begin(), capabilities.end());
    std::set<asset::AssetPath> visited;
    asset::AssetPath current = root;
    for (u32 depth = 0; depth <= 32; ++depth) {
        if (!visited.insert(current).second)
            return Err(ErrorCode::ValidationInvalidState, "pipeline fallback selection cycle detected");
        auto compiled = RenderPipelineCompiler(vfs, registry).Compile(current);
        if (!compiled.pipeline)
            return Err(ErrorCode::ValidationInvalidState, compiled.diagnostics.empty() ? "pipeline compilation failed" : compiled.diagnostics.front().message);
        const auto missing = [&](StringId id) { return !std::ranges::binary_search(capabilities, id); };
        const auto fallback = std::ranges::find_if(compiled.pipeline->fallbacks, [&](const FallbackRule& rule) { return std::ranges::any_of(rule.missing_capabilities, missing); });
        if (fallback != compiled.pipeline->fallbacks.end()) {
            current = fallback->path;
            continue;
        }
        if (std::ranges::any_of(compiled.pipeline->required_capabilities, missing))
            return Err(ErrorCode::ValidationInvalidState, "pipeline requires unsupported capabilities and has no applicable fallback");
        return Ok(std::move(current));
    }
    return Err(ErrorCode::ValidationInvalidState, "pipeline fallback selection depth limit exceeded");
}

} // namespace woki::gfx
