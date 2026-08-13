#include <bit>
#include <array>

#include <woki/gfx/advanced/pipeline_product.hpp>
#include "binary_codec.hpp"

namespace woki::gfx {
namespace {

constexpr std::array<std::byte, 4> kMagic{std::byte{'W'}, std::byte{'P'}, std::byte{'I'}, std::byte{'P'}};

class Writer : public detail::BinaryWriter {
public:
    void Optional(const std::optional<std::string>& value) {
        Int(static_cast<u8>(value.has_value()));
        if (value)
            String(*value);
    }
};

class Reader : public detail::BinaryReader {
public:
    Reader(std::span<const std::byte> bytes, PipelineProductLimits limits)
        : BinaryReader(bytes),
          limits_(limits) {}

    bool String(std::string& value) {
        size_t size{};
        if (!BinaryReader::String(value, limits_.max_string_bytes, &size) || total_strings_ > limits_.max_total_string_bytes - std::min<u32>(static_cast<u32>(size), limits_.max_total_string_bytes))
            return false;
        total_strings_ += static_cast<u32>(size);
        return true;
    }

    bool Optional(std::optional<std::string>& value) {
        u8 present{};
        std::string text;
        if (!Int(present) || present > 1 || (present && !String(text)))
            return false;
        if (present)
            value = std::move(text);
        return true;
    }

    bool Count(u32& value) {
        return Int(value) && value <= limits_.max_records && records_ <= limits_.max_records - value && (records_ += value, true);
    }

private:
    PipelineProductLimits limits_;
    u32 records_{};
    u32 total_strings_{};
};

void Value(Writer& writer, const ConfigValue& value) {
    writer.Int(static_cast<u8>(value.index()));
    std::visit(
        [&](const auto& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::same_as<T, bool>)
                writer.Int(static_cast<u8>(item));
            else if constexpr (std::same_as<T, i64>)
                writer.Int(item);
            else if constexpr (std::same_as<T, f64>)
                writer.Int(std::bit_cast<u64>(item));
            else
                writer.String(item);
        },
        value
    );
}

bool Value(Reader& reader, ConfigValue& value) {
    u8 type{};
    if (!reader.Int(type) || type > 3)
        return false;
    if (type == 0) {
        u8 item{};
        if (!reader.Int(item) || item > 1)
            return false;
        value = item != 0;
    } else if (type == 1) {
        i64 item{};
        if (!reader.Int(item))
            return false;
        value = item;
    } else if (type == 2) {
        u64 item{};
        if (!reader.Int(item))
            return false;
        value = std::bit_cast<f64>(item);
    } else {
        std::string item;
        if (!reader.String(item))
            return false;
        value = std::move(item);
    }
    return true;
}

bool ValidFormat(PipelineTargetFormat format) {
    return format <= PipelineTargetFormat::Depth32Float;
}

bool ColorFormat(PipelineTargetFormat format) {
    return ValidFormat(format) && format <= PipelineTargetFormat::RGBA16Float;
}

bool DepthFormat(PipelineTargetFormat format) {
    return format >= PipelineTargetFormat::Depth24Plus && ValidFormat(format);
}

bool Valid(const RenderPipelineIR& pipeline, PipelineProductLimits limits) {
    u64 records = pipeline.targets.color.size() + pipeline.features.size() + pipeline.extensions.size() + pipeline.fallbacks.size() + pipeline.required_capabilities.size() + pipeline.dependencies.size();
    u64 strings = pipeline.debug_name.size() + pipeline.render_path_name.size();
    const auto add_string = [&](std::string_view text) {
        strings += text.size();
        return text.size() <= limits.max_string_bytes && strings <= limits.max_total_string_bytes;
    };
    if (pipeline.name_id != StringId(pipeline.debug_name) || pipeline.render_path != StringId(pipeline.render_path_name) || !pipeline.asset_id || pipeline.root_source_hash == ContentHash{}
        || pipeline.content_hash != HashPipeline(pipeline) || records > limits.max_records || !add_string(pipeline.debug_name) || !add_string(pipeline.render_path_name))
        return false;
    if ((pipeline.targets.samples != 1 && pipeline.targets.samples != 4) || pipeline.targets.color.empty() || !std::ranges::all_of(pipeline.targets.color, ColorFormat)
        || (pipeline.targets.depth && !DepthFormat(*pipeline.targets.depth)))
        return false;
    std::set<std::pair<StringId, std::optional<std::string>>> identities;
    for (const auto& feature : pipeline.features) {
        records += feature.config.values.size();
        if (feature.id != StringId(feature.debug_name) || feature.scope > FeatureScope::View || feature.multiplicity > FeatureMultiplicity::Multiple || feature.factory_version.major == 0
            || feature.config.feature != feature.id || feature.config.instance != feature.instance || feature.config.factory_version != feature.factory_version || feature.config.hash != HashFeatureConfig(feature.config)
            || (feature.multiplicity == FeatureMultiplicity::Once && feature.instance) || (feature.multiplicity == FeatureMultiplicity::Multiple && (!feature.instance || feature.instance->empty()))
            || !identities.emplace(feature.id, feature.instance).second || !add_string(feature.debug_name) || (feature.instance && !add_string(*feature.instance))
            || !std::ranges::is_sorted(feature.config.values, {}, &decltype(feature.config.values)::value_type::first)
            || std::ranges::adjacent_find(feature.config.values, {}, &decltype(feature.config.values)::value_type::first) != feature.config.values.end())
            return false;
        for (const auto& [name, value] : feature.config.values)
            if (name.empty() || !add_string(name) || (std::holds_alternative<std::string>(value) && !add_string(std::get<std::string>(value))))
                return false;
    }
    if (records > limits.max_records || !std::ranges::is_sorted(pipeline.extensions, {}, &ExtensionBinding::debug_name)
        || std::ranges::adjacent_find(pipeline.extensions, {}, &ExtensionBinding::debug_name) != pipeline.extensions.end())
        return false;
    for (const auto& extension : pipeline.extensions) {
        records += extension.features.size();
        std::set<std::pair<StringId, std::optional<std::string>>> seen;
        if (extension.point != StringId(extension.debug_name) || !add_string(extension.debug_name))
            return false;
        for (const auto& feature : extension.features)
            if (feature.id != StringId(feature.debug_name) || !identities.contains({feature.id, feature.instance}) || !seen.emplace(feature.id, feature.instance).second || !add_string(feature.debug_name)
                || (feature.instance && !add_string(*feature.instance)))
                return false;
    }
    if (pipeline.quality_path && (!pipeline.quality_id || *pipeline.quality_id != asset::AssetId::FromName("engine://" + pipeline.quality_path->String()) || !add_string(pipeline.quality_path->String())))
        return false;
    if (pipeline.quality_id.has_value() != pipeline.quality_path.has_value())
        return false;
    std::set<asset::AssetPath> fallback_paths;
    for (const auto& fallback : pipeline.fallbacks) {
        records += fallback.missing_capabilities.size() + fallback.capability_names.size();
        if (fallback.target != asset::AssetId::FromName("engine://" + fallback.path.String()) || !fallback_paths.insert(fallback.path).second || !add_string(fallback.path.String())
            || fallback.missing_capabilities.size() != fallback.capability_names.size() || !std::ranges::is_sorted(fallback.missing_capabilities)
            || std::ranges::adjacent_find(fallback.missing_capabilities) != fallback.missing_capabilities.end())
            return false;
        for (std::size_t i = 0; i < fallback.capability_names.size(); ++i)
            if (fallback.missing_capabilities[i] != StringId(fallback.capability_names[i]) || !add_string(fallback.capability_names[i]))
                return false;
    }
    if (pipeline.required_capabilities.size() != pipeline.required_capability_names.size() || !std::ranges::is_sorted(pipeline.required_capabilities)
        || std::ranges::adjacent_find(pipeline.required_capabilities) != pipeline.required_capabilities.end())
        return false;
    for (std::size_t i = 0; i < pipeline.required_capability_names.size(); ++i)
        if (pipeline.required_capabilities[i] != StringId(pipeline.required_capability_names[i]) || !add_string(pipeline.required_capability_names[i]))
            return false;
    if (!std::ranges::is_sorted(pipeline.dependencies) || std::ranges::adjacent_find(pipeline.dependencies, {}, &PipelineDependency::path) != pipeline.dependencies.end())
        return false;
    u32 root_dependencies{};
    for (const auto& dependency : pipeline.dependencies) {
        if (dependency.hash == ContentHash{} || !add_string(dependency.path.String()))
            return false;
        if (asset::AssetId::FromName("engine://" + dependency.path.String()) == pipeline.asset_id) {
            ++root_dependencies;
            if (dependency.hash != pipeline.root_source_hash)
                return false;
        }
    }
    return records <= limits.max_records && strings <= limits.max_total_string_bytes && root_dependencies == 1;
}

void WriteReference(Writer& out, const FeatureReference& feature) {
    out.String(feature.debug_name);
    out.Optional(feature.instance);
}

bool ReadReference(Reader& in, FeatureReference& feature) {
    if (!in.String(feature.debug_name) || !in.Optional(feature.instance))
        return false;
    feature.id = StringId(feature.debug_name);
    return true;
}

} // namespace

Result<std::vector<std::byte>> SerializePipeline(const RenderPipelineIR& pipeline, PipelineProductLimits limits) {
    if (!Valid(pipeline, limits))
        return Err(ErrorCode::ValidationInvalidState, "pipeline IR is noncanonical or has invalid semantic data");
    Writer out;
    out.bytes.insert(out.bytes.end(), kMagic.begin(), kMagic.end());
    out.Int(kPipelineProductVersion);
    out.Int(u32{});
    out.Hash(pipeline.content_hash);
    out.Id(pipeline.asset_id);
    out.Hash(pipeline.root_source_hash);
    out.String(pipeline.debug_name);
    out.String(pipeline.render_path_name);
    out.Int(static_cast<u32>(pipeline.targets.color.size()));
    for (auto format : pipeline.targets.color)
        out.Int(static_cast<u8>(format));
    out.Int(static_cast<u8>(pipeline.targets.depth.has_value()));
    if (pipeline.targets.depth)
        out.Int(static_cast<u8>(*pipeline.targets.depth));
    out.Int(pipeline.targets.samples);
    out.Int(static_cast<u32>(pipeline.features.size()));
    for (const auto& feature : pipeline.features) {
        out.String(feature.debug_name);
        out.Optional(feature.instance);
        out.Int(static_cast<u8>(feature.scope));
        out.Int(static_cast<u8>(feature.multiplicity));
        out.Int(feature.factory_version.major);
        out.Int(feature.factory_version.minor);
        out.Int(feature.factory_version.patch);
        out.Hash(feature.config.hash);
        out.Int(static_cast<u32>(feature.config.values.size()));
        for (const auto& [name, value] : feature.config.values) {
            out.String(name);
            Value(out, value);
        }
    }
    out.Int(static_cast<u32>(pipeline.extensions.size()));
    for (const auto& extension : pipeline.extensions) {
        out.String(extension.debug_name);
        out.Int(static_cast<u32>(extension.features.size()));
        for (const auto& feature : extension.features)
            WriteReference(out, feature);
    }
    out.Int(static_cast<u8>(pipeline.quality_path.has_value()));
    if (pipeline.quality_path)
        out.String(pipeline.quality_path->String());
    out.Int(static_cast<u32>(pipeline.fallbacks.size()));
    for (const auto& fallback : pipeline.fallbacks) {
        out.String(fallback.path.String());
        out.Int(static_cast<u32>(fallback.capability_names.size()));
        for (const auto& name : fallback.capability_names)
            out.String(name);
    }
    out.Int(static_cast<u32>(pipeline.required_capability_names.size()));
    for (const auto& name : pipeline.required_capability_names)
        out.String(name);
    out.Int(static_cast<u32>(pipeline.dependencies.size()));
    for (const auto& dependency : pipeline.dependencies) {
        out.String(dependency.path.String());
        out.Hash(dependency.hash);
    }
    if (out.bytes.size() > limits.max_payload_bytes)
        return Err(ErrorCode::InvalidArgument, "pipeline payload exceeds byte limit");
    return Ok(std::move(out.bytes));
}

Result<RenderPipelineIR> ParsePipelineProduct(std::span<const std::byte> bytes, PipelineProductLimits limits) {
    if (bytes.size() > limits.max_payload_bytes || bytes.size() < kMagic.size() || !std::ranges::equal(kMagic, bytes.first(kMagic.size())))
        return Err(ErrorCode::ParseInvalidFormat, "pipeline payload size or magic is invalid");
    Reader in(bytes.subspan(kMagic.size()), limits);
    RenderPipelineIR value;
    u32 version{}, flags{};
    u32 count{};
    u8 present{}, raw{};
    std::string text;
    if (!in.Int(version) || !in.Int(flags) || version != kPipelineProductVersion || flags != 0 || !in.Hash(value.content_hash) || !in.Id(value.asset_id) || !in.Hash(value.root_source_hash) || !in.String(value.debug_name)
        || !in.String(value.render_path_name))
        return Err(ErrorCode::ParseInvalidFormat, "pipeline payload header is invalid");
    value.name_id = StringId(value.debug_name);
    value.render_path = StringId(value.render_path_name);
    if (!in.Count(count))
        return Err(ErrorCode::ParseInvalidFormat, "pipeline target count is invalid");
    for (u32 i = 0; i < count; ++i) {
        if (!in.Int(raw) || !ColorFormat(static_cast<PipelineTargetFormat>(raw)))
            return Err(ErrorCode::ParseInvalidFormat, "pipeline color target is invalid");
        value.targets.color.push_back(static_cast<PipelineTargetFormat>(raw));
    }
    if (!in.Int(present) || present > 1 || (present && (!in.Int(raw) || !DepthFormat(static_cast<PipelineTargetFormat>(raw)))))
        return Err(ErrorCode::ParseInvalidFormat, "pipeline depth target is invalid");
    if (present)
        value.targets.depth = static_cast<PipelineTargetFormat>(raw);
    if (!in.Int(value.targets.samples))
        return Err(ErrorCode::ParseInvalidFormat, "pipeline sample count is truncated");
    if (!in.Count(count))
        return Err(ErrorCode::ParseInvalidFormat, "pipeline feature count is invalid");
    for (u32 i = 0; i < count; ++i) {
        CompiledFeatureRequest feature;
        u8 scope{}, multiplicity{};
        if (!in.String(feature.debug_name) || !in.Optional(feature.instance) || !in.Int(scope) || scope > static_cast<u8>(FeatureScope::View) || !in.Int(multiplicity)
            || multiplicity > static_cast<u8>(FeatureMultiplicity::Multiple) || !in.Int(feature.factory_version.major) || !in.Int(feature.factory_version.minor) || !in.Int(feature.factory_version.patch)
            || !in.Hash(feature.config.hash))
            return Err(ErrorCode::ParseInvalidFormat, "pipeline feature is truncated");
        feature.id = StringId(feature.debug_name);
        feature.scope = static_cast<FeatureScope>(scope);
        feature.multiplicity = static_cast<FeatureMultiplicity>(multiplicity);
        feature.config.feature = feature.id;
        feature.config.instance = feature.instance;
        feature.config.factory_version = feature.factory_version;
        u32 values{};
        if (!in.Count(values))
            return Err(ErrorCode::ParseInvalidFormat, "pipeline feature config count is invalid");
        for (u32 j = 0; j < values; ++j) {
            std::string name;
            ConfigValue item;
            if (!in.String(name) || !Value(in, item))
                return Err(ErrorCode::ParseInvalidFormat, "pipeline feature config is truncated");
            feature.config.values.emplace_back(std::move(name), std::move(item));
        }
        value.features.push_back(std::move(feature));
    }
    if (!in.Count(count))
        return Err(ErrorCode::ParseInvalidFormat, "pipeline extension count is invalid");
    for (u32 i = 0; i < count; ++i) {
        ExtensionBinding extension;
        if (!in.String(extension.debug_name))
            return Err(ErrorCode::ParseInvalidFormat, "pipeline extension is truncated");
        extension.point = StringId(extension.debug_name);
        u32 references{};
        if (!in.Count(references))
            return Err(ErrorCode::ParseInvalidFormat, "pipeline extension references are invalid");
        for (u32 j = 0; j < references; ++j) {
            FeatureReference feature;
            if (!ReadReference(in, feature))
                return Err(ErrorCode::ParseInvalidFormat, "pipeline extension reference is truncated");
            extension.features.push_back(std::move(feature));
        }
        value.extensions.push_back(std::move(extension));
    }
    if (!in.Int(present) || present > 1)
        return Err(ErrorCode::ParseInvalidFormat, "pipeline quality identity is invalid");
    if (present) {
        if (!in.String(text))
            return Err(ErrorCode::ParseInvalidFormat, "pipeline quality path is truncated");
        auto path = asset::AssetPath::Parse(text);
        if (!path)
            return Err(ErrorCode::ParseInvalidFormat, "pipeline quality path is invalid");
        value.quality_path = std::move(*path);
        value.quality_id = asset::AssetId::FromName("engine://" + value.quality_path->String());
    }
    if (!in.Count(count))
        return Err(ErrorCode::ParseInvalidFormat, "pipeline fallback count is invalid");
    for (u32 i = 0; i < count; ++i) {
        if (!in.String(text))
            return Err(ErrorCode::ParseInvalidFormat, "pipeline fallback is truncated");
        auto path = asset::AssetPath::Parse(text);
        if (!path)
            return Err(ErrorCode::ParseInvalidFormat, "pipeline fallback path is invalid");
        FallbackRule fallback{asset::AssetId::FromName("engine://" + path->String()), std::move(*path), {}, {}};
        u32 capabilities{};
        if (!in.Count(capabilities))
            return Err(ErrorCode::ParseInvalidFormat, "pipeline fallback capabilities are invalid");
        for (u32 j = 0; j < capabilities; ++j) {
            if (!in.String(text))
                return Err(ErrorCode::ParseInvalidFormat, "pipeline fallback capability is truncated");
            fallback.capability_names.push_back(text);
            fallback.missing_capabilities.emplace_back(text);
        }
        value.fallbacks.push_back(std::move(fallback));
    }
    if (!in.Count(count))
        return Err(ErrorCode::ParseInvalidFormat, "pipeline required capability count is invalid");
    for (u32 i = 0; i < count; ++i) {
        if (!in.String(text))
            return Err(ErrorCode::ParseInvalidFormat, "pipeline required capability is truncated");
        value.required_capability_names.push_back(text);
        value.required_capabilities.emplace_back(text);
    }
    if (!in.Count(count))
        return Err(ErrorCode::ParseInvalidFormat, "pipeline dependency count is invalid");
    for (u32 i = 0; i < count; ++i) {
        ContentHash hash;
        if (!in.String(text) || !in.Hash(hash))
            return Err(ErrorCode::ParseInvalidFormat, "pipeline dependency is truncated");
        auto path = asset::AssetPath::Parse(text);
        if (!path)
            return Err(ErrorCode::ParseInvalidFormat, "pipeline dependency path is invalid");
        value.dependencies.push_back({std::move(*path), hash});
    }
    if (!in.End() || !Valid(value, limits))
        return Err(ErrorCode::ParseInvalidFormat, "pipeline payload is noncanonical, inconsistent, or has trailing bytes");
    return Ok(std::move(value));
}

Result<asset::Product> MakePipelineProduct(const RenderPipelineIR& pipeline) {
    auto bytes = SerializePipeline(pipeline);
    if (!bytes)
        return Err(std::move(bytes).error());
    return Ok(asset::MakeProduct(
        pipeline.asset_id,
        kPipelineProductType,
        kPipelineProductVersion,
        1,
        pipeline.root_source_hash,
        {},
        Sha256("woki.gfx.generic-target"),
        std::move(*bytes)
    ));
}

Result<asset::Product> CookPipeline(const asset::Vfs& vfs, const FeatureRegistry& registry, const asset::AssetPath& path, std::vector<PipelineDiagnostic>* diagnostics) {
    auto compiled = RenderPipelineCompiler(vfs, registry).Compile(path);
    if (diagnostics)
        *diagnostics = compiled.diagnostics;
    if (!compiled.pipeline)
        return Err(ErrorCode::ValidationInvalidState, compiled.diagnostics.empty() ? "pipeline compilation failed" : compiled.diagnostics.front().message);
    return MakePipelineProduct(*compiled.pipeline);
}

} // namespace woki::gfx
