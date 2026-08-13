#include <bit>
#include <limits>
#include <woki/config.hpp>

#include <woki/gfx/advanced/material.hpp>

namespace woki::gfx {
namespace {
using Json = config::Json;
using namespace std::string_view_literals;

bool Only(const Json& object, std::initializer_list<std::string_view> keys) {
    return object.is_object() && std::ranges::all_of(object.items(), [&](const auto& item) {
        return std::ranges::find(keys, item.first) != keys.end();
    });
}

template <typename E, size_t N>
Result<E> Enum(const Json& value, const std::array<std::pair<std::string_view, E>, N>& values, std::string_view field) {
    if (!value.is_string())
        return Err(ErrorCode::ParseInvalidFormat, std::string(field) + " must be a string");
    const auto found = std::ranges::find(
        values,
        value.get_ref<const std::string&>(),
        &std::pair<std::string_view, E>::first
    );
    if (found == values.end())
        return Err(ErrorCode::ParseInvalidFormat, std::string(field) + " has an unsupported value");
    return Ok(found->second);
}

constexpr std::array kTypes{std::pair{"bool"sv, MaterialValueType::Bool},
    std::pair{"i32"sv, MaterialValueType::I32},
    std::pair{"u32"sv, MaterialValueType::U32},
    std::pair{"f32"sv, MaterialValueType::F32},
    std::pair{"vec2"sv, MaterialValueType::Vec2},
    std::pair{"vec3"sv, MaterialValueType::Vec3},
    std::pair{"vec4"sv, MaterialValueType::Vec4},
    std::pair{"mat4"sv, MaterialValueType::Mat4},
    std::pair{"texture2d"sv, MaterialValueType::Texture2D},
    std::pair{"textureCube"sv, MaterialValueType::TextureCube},
    std::pair{"sampler"sv, MaterialValueType::Sampler}};

Result<MaterialValue> Value(const Json& value, MaterialValueType type) {
    if (type == MaterialValueType::Bool && value.is_boolean())
        return Ok(MaterialValue{value.get<bool>()});
    if (type == MaterialValueType::I32 && value.is_number_integer()) {
        const auto number = value.get<i64>();
        if (number >= std::numeric_limits<i32>::min() && number <= std::numeric_limits<i32>::max())
            return Ok(MaterialValue{static_cast<i32>(number)});
    }
    if (type == MaterialValueType::U32 && value.is_number_unsigned()
        && value.get<u64>() <= std::numeric_limits<u32>::max())
        return Ok(MaterialValue{value.get<u32>()});
    if (type == MaterialValueType::F32 && value.is_number())
        return Ok(MaterialValue{value.get<f32>()});
    const auto vector = [&](size_t count) {
        return value.is_array() && value.size() == count
               && std::ranges::all_of(value, [](const Json& item) { return item.is_number(); });
    };
    if (type == MaterialValueType::Vec2 && vector(2))
        return Ok(MaterialValue{std::array{value[0].get<f32>(), value[1].get<f32>()}});
    if (type == MaterialValueType::Vec3 && vector(3))
        return Ok(MaterialValue{std::array{value[0].get<f32>(), value[1].get<f32>(), value[2].get<f32>()}});
    if (type == MaterialValueType::Vec4 && vector(4))
        return Ok(
            MaterialValue{
                std::array{value[0].get<f32>(), value[1].get<f32>(), value[2].get<f32>(), value[3].get<f32>()}}
        );
    if (type == MaterialValueType::Mat4 && vector(16)) {
        std::array<f32, 16> result{};
        for (size_t i = 0; i < result.size(); ++i)
            result[i] = value[i].get<f32>();
        return Ok(MaterialValue{result});
    }
    if ((type == MaterialValueType::Texture2D || type == MaterialValueType::TextureCube
            || type == MaterialValueType::Sampler)
        && value.is_string()) {
        auto id = asset::AssetId::Parse(value.get_ref<const std::string&>());
        if (id)
            return Ok(MaterialValue{*id});
    }
    return Err(ErrorCode::ParseInvalidFormat, "material value does not match its declared type");
}

Result<asset::AssetId> Id(const Json& object, const char* field) {
    if (!object.contains(field) || !object[field].is_string())
        return Err(ErrorCode::ParseInvalidFormat, std::string(field) + " must be an asset ID string");
    return asset::AssetId::Parse(object[field].get_ref<const std::string&>());
}

Result<TextureSamplerDefaults> Sampler(const Json& root) {
    if (!Only(
            root,
            {"address_u", "address_v", "address_w", "mag_filter", "min_filter", "mip_filter", "max_anisotropy"}
        ))
        return Err(ErrorCode::ParseInvalidFormat, "sampler contains an unknown field");
    TextureSamplerDefaults result;
    constexpr std::array addresses{std::pair{"clamp"sv, TextureAddressMode::Clamp},
        std::pair{"repeat"sv, TextureAddressMode::Repeat},
        std::pair{"mirror"sv, TextureAddressMode::Mirror}};
    constexpr std::array filters{std::pair{"nearest"sv, TextureFilter::Nearest},
        std::pair{"linear"sv, TextureFilter::Linear}};
    for (const auto& [name, output] :
        {std::pair{"address_u", &result.address_u}, {"address_v", &result.address_v}, {"address_w", &result.address_w}})
        if (root.contains(name))
            TRY_ASSIGN(*output, Enum(root[name], addresses, name));
    for (const auto& [name, output] : {std::pair{"mag_filter", &result.mag_filter},
             {"min_filter", &result.min_filter},
             {"mip_filter", &result.mip_filter}})
        if (root.contains(name))
            TRY_ASSIGN(*output, Enum(root[name], filters, name));
    if (root.contains("max_anisotropy")) {
        if (!root["max_anisotropy"].is_number_unsigned() || root["max_anisotropy"].get<u32>() < 1
            || root["max_anisotropy"].get<u32>() > 16)
            return Err(ErrorCode::ValidationOutOfRange, "sampler anisotropy must be in [1,16]");
        result.max_anisotropy = root["max_anisotropy"].get<u16>();
    }
    return Ok(result);
}
} // namespace

MaterialPropertyId MaterialPropertyId::FromName(const std::string_view name) noexcept {
    u32 hash = 2166136261U;
    for (const char value : name) {
        hash ^= static_cast<u8>(value);
        hash *= 16777619U;
    }
    return MaterialPropertyId(hash == 0 ? 1 : hash);
}

bool MaterialValueMatches(const MaterialValueType type, const MaterialValue& value) noexcept {
    switch (type) {
        case MaterialValueType::Bool:
            return std::holds_alternative<bool>(value);
        case MaterialValueType::I32:
            return std::holds_alternative<i32>(value);
        case MaterialValueType::U32:
            return std::holds_alternative<u32>(value);
        case MaterialValueType::F32:
            return std::holds_alternative<f32>(value);
        case MaterialValueType::Vec2:
            return std::holds_alternative<std::array<f32, 2>>(value);
        case MaterialValueType::Vec3:
            return std::holds_alternative<std::array<f32, 3>>(value);
        case MaterialValueType::Vec4:
            return std::holds_alternative<std::array<f32, 4>>(value);
        case MaterialValueType::Mat4:
            return std::holds_alternative<std::array<f32, 16>>(value);
        case MaterialValueType::Texture2D:
        case MaterialValueType::TextureCube:
        case MaterialValueType::Sampler:
            return std::holds_alternative<asset::AssetId>(value);
    }
    return false;
}

Result<MaterialTypeSource> ParseMaterialType(const std::string_view jsonc) {
    auto document = Json::Parse(jsonc, "material-type");
    if (!document)
        return Err(ErrorCode::ParseInvalidFormat, config::FormatDiagnostics(document.error()));
    Json root = std::move(*document);
    if (!Only(
            root,
            {"$schema",
                "schema",
                "id",
                "name",
                "shader",
                "product_family",
                "passes",
                "entry_points",
                "render_state",
                "properties",
                "textures",
                "samplers",
                "overrides"}
        ))
        return Err(ErrorCode::ParseInvalidFormat, "material type root contains an unknown field");
    if (!root.contains("schema") || !root["schema"].is_number_unsigned() || root["schema"] != 1
        || !root.contains("name") || !root["name"].is_string() || root["name"].get_ref<const std::string&>().empty())
        return Err(ErrorCode::ParseInvalidFormat, "material type requires schema 1 and a name");
    MaterialTypeSource result;
    TRY_ASSIGN(result.id, Id(root, "id"));
    TRY_ASSIGN(result.shader, Id(root, "shader"));
    result.name = root["name"].get<std::string>();
    if (!root.contains("product_family") || !root["product_family"].is_string()
        || root["product_family"].get_ref<const std::string&>().empty())
        return Err(ErrorCode::ParseInvalidFormat, "product_family must be a non-empty string");
    result.product_family = root["product_family"].get<std::string>();
    constexpr std::array passes{std::pair{"forward"sv, MaterialPass::Forward},
        std::pair{"depth"sv, MaterialPass::Depth},
        std::pair{"shadow"sv, MaterialPass::Shadow},
        std::pair{"velocity"sv, MaterialPass::Velocity}};
    if (!root.contains("passes") || !root["passes"].is_array() || root["passes"].empty())
        return Err(ErrorCode::ParseInvalidFormat, "passes must be a non-empty array");
    for (const auto& pass : root["passes"]) {
        MaterialPass parsed;
        TRY_ASSIGN(parsed, Enum(pass, passes, "pass"));
        result.passes.push_back(parsed);
    }
    std::ranges::sort(result.passes);
    if (std::ranges::adjacent_find(result.passes) != result.passes.end())
        return Err(ErrorCode::ParseInvalidFormat, "passes contains a duplicate");
    if (!root.contains("entry_points") || !root["entry_points"].is_object())
        return Err(ErrorCode::ParseInvalidFormat, "entry_points must be an object");
    for (const auto& [name, item] : root["entry_points"].items()) {
        auto pass = std::ranges::find(passes, name, &std::pair<std::string_view, MaterialPass>::first);
        if (pass == passes.end() || !Only(item, {"vertex", "fragment"}) || !item.contains("vertex")
            || !item["vertex"].is_string() || !item.contains("fragment") || !item["fragment"].is_string())
            return Err(ErrorCode::ParseInvalidFormat, "entry point must contain vertex and fragment strings");
        result.entry_points
            .emplace(pass->second, std::pair{item["vertex"].get<std::string>(), item["fragment"].get<std::string>()});
    }
    if (root.contains("render_state")) {
        const auto& state = root["render_state"];
        if (!Only(state, {"blend", "cull", "depth_compare", "depth_write", "alpha_test"}))
            return Err(ErrorCode::ParseInvalidFormat, "render_state contains an unknown field");
        constexpr std::array blends{std::pair{"opaque"sv, MaterialBlendMode::Opaque},
            std::pair{"alpha"sv, MaterialBlendMode::Alpha},
            std::pair{"additive"sv, MaterialBlendMode::Additive}};
        constexpr std::array culls{std::pair{"none"sv, rhi::CullMode::None},
            std::pair{"front"sv, rhi::CullMode::Front},
            std::pair{"back"sv, rhi::CullMode::Back}};
        constexpr std::array compares{std::pair{"less"sv, rhi::CompareFunction::Less},
            std::pair{"less_equal"sv, rhi::CompareFunction::LessEqual},
            std::pair{"always"sv, rhi::CompareFunction::Always}};
        if (state.contains("blend"))
            TRY_ASSIGN(result.render_state.blend, Enum(state["blend"], blends, "blend"));
        if (state.contains("cull"))
            TRY_ASSIGN(result.render_state.cull, Enum(state["cull"], culls, "cull"));
        if (state.contains("depth_compare"))
            TRY_ASSIGN(result.render_state.depth_compare, Enum(state["depth_compare"], compares, "depth_compare"));
        if (state.contains("depth_write")) {
            if (!state["depth_write"].is_boolean())
                return Err(ErrorCode::ParseInvalidFormat, "depth_write must be boolean");
            result.render_state.depth_write = state["depth_write"].get<bool>();
        }
        if (state.contains("alpha_test")) {
            if (!state["alpha_test"].is_boolean())
                return Err(ErrorCode::ParseInvalidFormat, "alpha_test must be boolean");
            result.render_state.alpha_test = state["alpha_test"].get<bool>();
        }
    }
    if (!root.contains("properties") || !root["properties"].is_array()
        || root["properties"].size() > kMaxMaterialProperties)
        return Err(ErrorCode::ParseInvalidFormat, "properties must be a bounded array");
    std::map<MaterialPropertyId, std::string> names;
    for (const auto& item : root["properties"]) {
        if (!Only(item, {"name", "type", "default", "offset"}) || !item.contains("name") || !item["name"].is_string()
            || !item.contains("type"))
            return Err(ErrorCode::ParseInvalidFormat, "property is invalid or contains an unknown field");
        MaterialPropertySource property;
        property.name = item["name"].get<std::string>();
        property.id = MaterialPropertyId::FromName(property.name);
        TRY_ASSIGN(property.type, Enum(item["type"], kTypes, "property type"));
        if (property.type >= MaterialValueType::Texture2D || !item.contains("default"))
            return Err(ErrorCode::ParseInvalidFormat, "numeric property requires a default and cannot be a resource");
        TRY_ASSIGN(property.default_value, Value(item["default"], property.type));
        if (item.contains("offset")) {
            if (!item["offset"].is_number_unsigned())
                return Err(ErrorCode::ParseInvalidFormat, "offset must be unsigned");
            property.offset = item["offset"].get<u32>();
        }
        if (!names.emplace(property.id, property.name).second)
            return Err(ErrorCode::ParseInvalidFormat, "duplicate or colliding material property name");
        result.properties.push_back(std::move(property));
    }
    constexpr std::array semantics{std::pair{"color"sv, TextureSemantic::Color},
        std::pair{"normal"sv, TextureSemantic::Normal},
        std::pair{"data"sv, TextureSemantic::Data},
        std::pair{"emissive"sv, TextureSemantic::Emissive},
        std::pair{"occlusion"sv, TextureSemantic::Occlusion},
        std::pair{"depth"sv, TextureSemantic::Depth},
        std::pair{"environment"sv, TextureSemantic::Environment}};
    if (root.contains("textures"))
        for (const auto& item : root["textures"]) {
            if (!Only(item, {"name", "type", "binding", "semantic", "default"}) || !item.contains("name")
                || !item["name"].is_string() || !item.contains("type") || !item.contains("binding")
                || !item["binding"].is_number_unsigned() || !item.contains("semantic"))
                return Err(ErrorCode::ParseInvalidFormat, "texture declaration is invalid");
            MaterialTextureSource texture;
            texture.name = item["name"].get<std::string>();
            texture.id = MaterialPropertyId::FromName(texture.name);
            texture.binding = item["binding"].get<u32>();
            TRY_ASSIGN(texture.type, Enum(item["type"], kTypes, "texture type"));
            TRY_ASSIGN(texture.semantic, Enum(item["semantic"], semantics, "texture semantic"));
            if (texture.type != MaterialValueType::Texture2D && texture.type != MaterialValueType::TextureCube)
                return Err(ErrorCode::ParseInvalidFormat, "texture type must be texture2d or textureCube");
            if (item.contains("default")) {
                auto value = Value(item["default"], texture.type);
                if (!value)
                    return Err(std::move(value).error());
                texture.default_asset = std::get<asset::AssetId>(*value);
            }
            if (!names.emplace(texture.id, texture.name).second)
                return Err(ErrorCode::ParseInvalidFormat, "duplicate or colliding material property name");
            result.textures.push_back(std::move(texture));
        }
    if (root.contains("samplers"))
        for (const auto& item : root["samplers"]) {
            if (!Only(item, {"name", "binding", "default"}) || !item.contains("name") || !item["name"].is_string()
                || !item.contains("binding") || !item["binding"].is_number_unsigned())
                return Err(ErrorCode::ParseInvalidFormat, "sampler declaration is invalid");
            MaterialSamplerSource sampler;
            sampler.name = item["name"].get<std::string>();
            sampler.id = MaterialPropertyId::FromName(sampler.name);
            sampler.binding = item["binding"].get<u32>();
            if (item.contains("default"))
                TRY_ASSIGN(sampler.defaults, Sampler(item["default"]));
            if (!names.emplace(sampler.id, sampler.name).second)
                return Err(ErrorCode::ParseInvalidFormat, "duplicate or colliding material property name");
            result.samplers.push_back(std::move(sampler));
        }
    if (root.contains("overrides")) {
        if (!root["overrides"].is_array() || root["overrides"].size() > kMaxMaterialOverrides)
            return Err(ErrorCode::ValidationOutOfRange, "overrides exceeds the bounded policy");
        u64 permutations = 1;
        for (const auto& item : root["overrides"]) {
            if (!Only(item, {"property", "override_id", "values"}) || !item.contains("property")
                || !item["property"].is_string() || !item.contains("override_id")
                || !item["override_id"].is_number_unsigned() || !item.contains("values") || !item["values"].is_array()
                || item["values"].empty())
                return Err(ErrorCode::ParseInvalidFormat, "override declaration is invalid");
            MaterialOverrideSource override;
            override.name = item["property"].get<std::string>();
            override.property = MaterialPropertyId::FromName(override.name);
            override.override_id = item["override_id"].get<u32>();
            const auto property = std::ranges::find(result.properties, override.property, &MaterialPropertySource::id);
            if (property == result.properties.end())
                return Err(ErrorCode::ParseInvalidFormat, "override property is unknown");
            for (const auto& allowed : item["values"]) {
                MaterialValue parsed;
                TRY_ASSIGN(parsed, Value(allowed, property->type));
                override.values.push_back(std::move(parsed));
            }
            permutations *= override.values.size();
            if (permutations > kMaxMaterialPermutations)
                return Err(ErrorCode::ValidationOutOfRange, "material permutation policy exceeds 256 variants");
            result.overrides.push_back(std::move(override));
        }
    }
    return Ok(std::move(result));
}

Result<MaterialInstanceSource> ParseMaterialInstance(const std::string_view jsonc, const MaterialTypeSource& type) {
    auto document = Json::Parse(jsonc, "material");
    if (!document)
        return Err(ErrorCode::ParseInvalidFormat, config::FormatDiagnostics(document.error()));
    Json root = std::move(*document);
    if (!Only(root, {"$schema", "schema", "id", "type", "overrides"}) || !root.contains("schema")
        || !root["schema"].is_number_unsigned() || root["schema"] != 1 || !root.contains("overrides")
        || !root["overrides"].is_object())
        return Err(ErrorCode::ParseInvalidFormat, "material instance root is invalid or contains an unknown field");
    MaterialInstanceSource result;
    TRY_ASSIGN(result.id, Id(root, "id"));
    TRY_ASSIGN(result.type, Id(root, "type"));
    if (result.type != type.id)
        return Err(ErrorCode::ValidationInvalidState, "material instance type does not match the supplied definition");
    for (const auto& [name, item] : root["overrides"].items()) {
        const auto id = MaterialPropertyId::FromName(name);
        if (const auto property = std::ranges::find(type.properties, id, &MaterialPropertySource::id);
            property != type.properties.end()) {
            MaterialValue parsed;
            TRY_ASSIGN(parsed, Value(item, property->type));
            result.overrides.emplace(id, std::move(parsed));
        } else if (const auto texture = std::ranges::find(type.textures, id, &MaterialTextureSource::id);
            texture != type.textures.end()) {
            MaterialValue parsed;
            TRY_ASSIGN(parsed, Value(item, texture->type));
            result.overrides.emplace(id, std::move(parsed));
        } else
            return Err(ErrorCode::ParseInvalidFormat, "material instance override names an unknown property: " + name);
    }
    return Ok(std::move(result));
}

} // namespace woki::gfx
