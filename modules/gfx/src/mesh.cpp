#include <array>
#include <woki/config.hpp>

#include <woki/gfx/advanced/mesh.hpp>

namespace woki::gfx {
namespace {
using Json = config::Json;

bool Only(const Json& object, const std::initializer_list<std::string_view> fields) {
    return object.is_object() && std::ranges::all_of(object.items(), [&](const auto& item) { return std::ranges::find(fields, item.first) != fields.end(); });
}

template <typename T, size_t N>
Result<T> ParseEnum(const Json& value, const std::array<std::pair<std::string_view, T>, N>& choices, const std::string_view field) {
    if (!value.is_string())
        return Err(ErrorCode::ParseInvalidFormat, std::string(field) + " must be a string");
    const auto found = std::ranges::find(choices, value.get_ref<const std::string&>(), &std::pair<std::string_view, T>::first);
    return found == choices.end() ? Result<T>(Err(ErrorCode::ParseInvalidFormat, std::string(field) + " has an unsupported value")) : Ok(found->second);
}

ValueType ShaderType(const VertexFormat format) {
    switch (format) {
        case VertexFormat::Float32x2:
            return ValueType::Vec2F;
        case VertexFormat::Float32x3:
            return ValueType::Vec3F;
        case VertexFormat::Float32x4:
        case VertexFormat::Unorm16x4:
            return ValueType::Vec4F;
        case VertexFormat::Uint16x4:
            return ValueType::Vec4U;
    }
    return ValueType::Unknown;
}
} // namespace

Result<MeshSource> ParseMeshSource(const std::string_view jsonc) {
    auto parsed = Json::Parse(jsonc, "mesh-import");
    if (!parsed)
        return Err(ErrorCode::ParseInvalidFormat, config::FormatDiagnostics(parsed.error()));
    Json root = std::move(*parsed);
    if (!Only(root, {"$schema", "schema", "id", "source", "importer", "coordinates", "unit_scale", "tangents", "optimize", "lod_ratios", "lod_errors", "meshlets", "quantize", "compress", "animations"}))
        return Err(ErrorCode::ParseInvalidFormat, "mesh import root contains an unknown field");
    if (!root.contains("schema") || root["schema"] != 1 || !root.contains("id") || !root["id"].is_string() || !root.contains("source") || !root["source"].is_string())
        return Err(ErrorCode::ParseInvalidFormat, "mesh import requires schema 1, id, and source");
    auto id = asset::AssetId::Parse(root["id"].get_ref<const std::string&>());
    if (!id)
        return Err(std::move(id).error());
    auto uri = asset::AssetUri::Parse(root["source"].get_ref<const std::string&>());
    if (!uri)
        return Err(std::move(uri).error());
    MeshSource result(*id, std::move(*uri));
    if (root.contains("coordinates")
        && (!root["coordinates"].is_string() || root["coordinates"].get_ref<const std::string&>() != "canonical"))
        return Err(ErrorCode::ParseInvalidFormat, "coordinates currently supports only canonical");
    using namespace std::string_view_literals;
    constexpr std::array importers{std::pair{"auto"sv, MeshImporterKind::Auto}, std::pair{"fastgltf"sv, MeshImporterKind::FastGltf}, std::pair{"assimp"sv, MeshImporterKind::Assimp}};
    constexpr std::array tangents{std::pair{"preserve"sv, TangentPolicy::Preserve}, std::pair{"generate_if_missing"sv, TangentPolicy::GenerateIfMissing}, std::pair{"generate"sv, TangentPolicy::Generate},
        std::pair{"none"sv, TangentPolicy::None}};
    if (root.contains("importer"))
        TRY_ASSIGN(result.importer, ParseEnum(root["importer"], importers, "importer"));
    if (root.contains("tangents"))
        TRY_ASSIGN(result.tangents, ParseEnum(root["tangents"], tangents, "tangents"));
    if (root.contains("unit_scale")) {
        if (!root["unit_scale"].is_number() || root["unit_scale"].get<f32>() <= 0.0F)
            return Err(ErrorCode::ValidationOutOfRange, "unit_scale must be positive");
        result.unit_scale = root["unit_scale"].get<f32>();
    }
    if (root.contains("optimize"))
        result.optimize = root["optimize"].get<bool>();
    if (root.contains("quantize"))
        result.quantize = root["quantize"].get<bool>();
    if (root.contains("compress"))
        result.compress = root["compress"].get<bool>();
    if (result.quantize || result.compress)
        return Err(ErrorCode::GraphicsUnsupportedApi, "mesh quantization/compression is not enabled because no runtime decode path is implemented");
    if (root.contains("lod_ratios"))
        result.lod_ratios = root["lod_ratios"].get<std::vector<f32>>();
    if (result.lod_ratios.empty())
        result.lod_ratios = {1.0F};
    for (size_t index = 0; index < result.lod_ratios.size(); ++index)
        if (result.lod_ratios[index] <= 0.0F || result.lod_ratios[index] > 1.0F || (index > 0 && result.lod_ratios[index] >= result.lod_ratios[index - 1]))
            return Err(ErrorCode::ValidationOutOfRange, "lod_ratios must be strictly decreasing values in (0,1]");
    if (root.contains("lod_errors"))
        result.lod_errors = root["lod_errors"].get<std::vector<f32>>();
    if (!result.lod_errors.empty() && result.lod_errors.size() != result.lod_ratios.size())
        return Err(ErrorCode::ValidationInvalidState, "lod_errors must match lod_ratios");
    if (root.contains("meshlets")) {
        const auto& meshlets = root["meshlets"];
        if (!Only(meshlets, {"enabled", "max_vertices", "max_triangles"}))
            return Err(ErrorCode::ParseInvalidFormat, "meshlets contains an unknown field");
        if (meshlets.contains("enabled"))
            result.meshlets.enabled = meshlets["enabled"].get<bool>();
        if (meshlets.contains("max_vertices"))
            result.meshlets.max_vertices = meshlets["max_vertices"].get<u32>();
        if (meshlets.contains("max_triangles"))
            result.meshlets.max_triangles = meshlets["max_triangles"].get<u32>();
        if (result.meshlets.max_vertices < 3 || result.meshlets.max_vertices > 255 || result.meshlets.max_triangles < 1 || result.meshlets.max_triangles > 512)
            return Err(ErrorCode::ValidationOutOfRange, "meshlet limits are outside supported ranges");
    }
    if (root.contains("animations")) {
        const auto& animations = root["animations"];
        if (animations.is_boolean())
            result.import_animations = animations.get<bool>();
        else if (animations.is_array())
            result.animations = animations.get<std::vector<std::string>>();
        else
            return Err(ErrorCode::ParseInvalidFormat, "animations must be boolean or a name array");
    }
    return Ok(std::move(result));
}

u64 HashVertexSchema(const VertexSchema& schema) noexcept {
    u64 hash = 1469598103934665603ULL;
    const auto add = [&](const u64 value) {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    add(schema.streams.size());
    for (const auto& stream : schema.streams) {
        add(stream.stride);
        add(static_cast<u8>(stream.step_mode));
        add(stream.attributes.size());
        for (const auto& attribute : stream.attributes) {
            add(static_cast<u8>(attribute.semantic));
            add(attribute.location);
            add(static_cast<u8>(attribute.format));
            add(attribute.offset);
        }
    }
    return hash == 0 ? 1 : hash;
}

Result<void> ValidateVertexSchema(const VertexSchema& schema, const ShaderInterface& shader, const std::string_view entry_point) {
    const auto entry = std::ranges::find_if(shader.entry_points, [&](const auto& value) { return value.stage == ShaderStage::Vertex && value.name == entry_point; });
    if (entry == shader.entry_points.end())
        return Err(ErrorCode::ValidationInvalidState, "vertex shader entry point is absent");
    for (const auto input : entry->inputs) {
        const VertexAttributeDesc* found{};
        for (const auto& stream : schema.streams) {
            const auto attribute = std::ranges::find(stream.attributes, input.location, &VertexAttributeDesc::location);
            if (attribute != stream.attributes.end()) {
                found = &*attribute;
                break;
            }
        }
        if (found == nullptr || ShaderType(found->format) != input.type)
            return Err(ErrorCode::ValidationInvalidState, "vertex schema does not satisfy reflected shader input location " + std::to_string(input.location));
    }
    return Ok();
}

} // namespace woki::gfx
