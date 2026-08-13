#include <array>
#include <woki/config.hpp>

#include <woki/gfx/advanced/texture.hpp>

namespace woki::gfx {
namespace {

using Json = config::Json;
using namespace std::string_view_literals;

bool Only(const Json& object, const std::initializer_list<std::string_view> keys) {
    return object.is_object() && std::ranges::all_of(object.items(), [&](const auto& item) { return std::ranges::find(keys, item.first) != keys.end(); });
}

template <typename E, std::size_t N>
Result<E> Enum(const Json& value, const std::array<std::pair<std::string_view, E>, N>& names, const std::string_view field) {
    if (!value.is_string())
        return Err(ErrorCode::ParseInvalidFormat, std::string(field) + " must be a string");
    const auto text = value.get<std::string>();
    const auto found = std::ranges::find(names, text, &std::pair<std::string_view, E>::first);
    if (found == names.end())
        return Err(ErrorCode::ParseInvalidFormat, std::string(field) + " has an unsupported value");
    return Ok(found->second);
}

template <typename E, std::size_t N>
Result<void> OptionalEnum(const Json& root, const char* name, E& output, const std::array<std::pair<std::string_view, E>, N>& names) {
    if (!root.contains(name))
        return Ok();
    TRY_ASSIGN(output, Enum(root[name], names, name));
    return Ok();
}

} // namespace

asset::AssetId BuiltinFallbackTextureId(const TextureSemantic semantic) noexcept {
    constexpr std::array names{"color", "normal", "data", "emissive", "occlusion", "depth", "environment"};
    return asset::AssetId::FromName("engine://textures/builtin/material-" + std::string(names[static_cast<u8>(semantic)]));
}

ContentHash BuiltinFallbackTextureProductHash(const TextureSemantic semantic) noexcept {
    return Sha256("woki.gfx.builtin-material-fallback.v1:" + std::to_string(static_cast<u8>(semantic)));
}

Result<TextureSource> ParseTextureSource(const std::string_view jsonc) {
    auto parsed = Json::Parse(jsonc, "texture");
    if (!parsed)
        return Err(ErrorCode::ParseInvalidFormat, config::FormatDiagnostics(parsed.error()));
    Json root = std::move(*parsed);
    if (!Only(root, {"$schema", "schema", "asset_id", "source", "semantic", "color_space", "dimension", "mips", "streaming", "max_size", "target", "sampler"}))
        return Err(ErrorCode::ParseInvalidFormat, "texture source root is not an object or contains an unknown key");
    if (!root.contains("schema") || !root["schema"].is_number_unsigned() || root["schema"] != 1)
        return Err(ErrorCode::ParseInvalidFormat, "texture schema must be unsigned integer 1");
    if (!root.contains("asset_id") || !root["asset_id"].is_string() || !root.contains("source") || !root["source"].is_string())
        return Err(ErrorCode::ParseInvalidFormat, "texture asset_id and source must be strings");
    auto id = asset::AssetId::Parse(root["asset_id"].get_ref<const std::string&>());
    auto uri = asset::AssetUri::Parse(root["source"].get_ref<const std::string&>());
    if (!id || !uri)
        return Err(ErrorCode::ParseInvalidFormat, "texture asset_id or source URI is invalid");
    TextureSource result(std::move(*id), std::move(*uri));

    constexpr std::array semantics{std::pair{"color"sv, TextureSemantic::Color}, std::pair{"normal"sv, TextureSemantic::Normal}, std::pair{"data"sv, TextureSemantic::Data},
        std::pair{"emissive"sv, TextureSemantic::Emissive}, std::pair{"occlusion"sv, TextureSemantic::Occlusion}, std::pair{"depth"sv, TextureSemantic::Depth}, std::pair{"environment"sv, TextureSemantic::Environment}};
    constexpr std::array spaces{std::pair{"linear"sv, TextureColorSpace::Linear}, std::pair{"srgb"sv, TextureColorSpace::Srgb}};
    constexpr std::array dimensions{std::pair{"2d"sv, TextureShape::e2D}, std::pair{"cube"sv, TextureShape::Cube}};
    constexpr std::array mips{std::pair{"none"sv, TextureMipPolicy::None}, std::pair{"generate"sv, TextureMipPolicy::Generate}, std::pair{"preserve"sv, TextureMipPolicy::Preserve}};
    constexpr std::array streaming{std::pair{"full"sv, TextureStreamingPolicy::Full}, std::pair{"mips"sv, TextureStreamingPolicy::StreamMips}};
    constexpr std::array targets{std::pair{"auto"sv, TextureTargetPolicy::Auto}, std::pair{"rgba8"sv, TextureTargetPolicy::Rgba8}};
    TRY_VOID(OptionalEnum(root, "semantic", result.semantic, semantics));
    TRY_VOID(OptionalEnum(root, "color_space", result.color_space, spaces));
    TRY_VOID(OptionalEnum(root, "dimension", result.dimension, dimensions));
    TRY_VOID(OptionalEnum(root, "mips", result.mip_policy, mips));
    TRY_VOID(OptionalEnum(root, "streaming", result.streaming, streaming));
    TRY_VOID(OptionalEnum(root, "target", result.target, targets));
    if (root.contains("max_size")) {
        if (!root["max_size"].is_number_unsigned())
            return Err(ErrorCode::ParseInvalidFormat, "max_size must be an unsigned integer");
        result.max_size = root["max_size"].get<u32>();
    }
    if (result.max_size == 0 || result.max_size > 32768)
        return Err(ErrorCode::ValidationOutOfRange, "max_size must be between 1 and 32768");

    if (root.contains("sampler")) {
        const auto& sampler = root["sampler"];
        if (!Only(sampler, {"address_u", "address_v", "address_w", "mag_filter", "min_filter", "mip_filter", "max_anisotropy"}))
            return Err(ErrorCode::ParseInvalidFormat, "sampler is not an object or contains an unknown key");
        constexpr std::array addresses{std::pair{"clamp"sv, TextureAddressMode::Clamp}, std::pair{"repeat"sv, TextureAddressMode::Repeat}, std::pair{"mirror"sv, TextureAddressMode::Mirror}};
        constexpr std::array filters{std::pair{"nearest"sv, TextureFilter::Nearest}, std::pair{"linear"sv, TextureFilter::Linear}};
        TRY_VOID(OptionalEnum(sampler, "address_u", result.sampler.address_u, addresses));
        TRY_VOID(OptionalEnum(sampler, "address_v", result.sampler.address_v, addresses));
        TRY_VOID(OptionalEnum(sampler, "address_w", result.sampler.address_w, addresses));
        TRY_VOID(OptionalEnum(sampler, "mag_filter", result.sampler.mag_filter, filters));
        TRY_VOID(OptionalEnum(sampler, "min_filter", result.sampler.min_filter, filters));
        TRY_VOID(OptionalEnum(sampler, "mip_filter", result.sampler.mip_filter, filters));
        if (sampler.contains("max_anisotropy")) {
            if (!sampler["max_anisotropy"].is_number_unsigned())
                return Err(ErrorCode::ParseInvalidFormat, "sampler.max_anisotropy must be an unsigned integer");
            result.sampler.max_anisotropy = sampler["max_anisotropy"].get<u16>();
        }
        if (result.sampler.max_anisotropy == 0 || result.sampler.max_anisotropy > 16)
            return Err(ErrorCode::ValidationOutOfRange, "sampler.max_anisotropy must be between 1 and 16");
    }
    if (result.semantic == TextureSemantic::Depth && result.color_space != TextureColorSpace::Linear)
        return Err(ErrorCode::ValidationInvalidState, "depth textures must use linear color space");
    return Ok(std::move(result));
}

} // namespace woki::gfx
