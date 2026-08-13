#pragma once

#include <optional>

#include <woki/asset.hpp>

namespace woki::gfx {

enum class TextureSemantic : u8 { Color, Normal, Data, Emissive, Occlusion, Depth, Environment };
enum class TextureColorSpace : u8 { Linear, Srgb };
enum class TextureShape : u8 { e2D, Cube };
enum class TextureMipPolicy : u8 { None, Generate, Preserve };
enum class TextureStreamingPolicy : u8 { Full, StreamMips };
enum class TextureTargetPolicy : u8 { Auto, Rgba8 };
enum class TextureAddressMode : u8 { Clamp, Repeat, Mirror };
enum class TextureFilter : u8 { Nearest, Linear };

// Logical products supplied by TextureLibrary itself. They never enter AssetManager.
[[nodiscard]] asset::AssetId BuiltinFallbackTextureId(TextureSemantic semantic) noexcept;
[[nodiscard]] ContentHash BuiltinFallbackTextureProductHash(TextureSemantic semantic) noexcept;

struct TextureSamplerDefaults final {
    TextureAddressMode address_u{TextureAddressMode::Repeat};
    TextureAddressMode address_v{TextureAddressMode::Repeat};
    TextureAddressMode address_w{TextureAddressMode::Repeat};
    TextureFilter mag_filter{TextureFilter::Linear};
    TextureFilter min_filter{TextureFilter::Linear};
    TextureFilter mip_filter{TextureFilter::Linear};
    u16 max_anisotropy{1};
};

struct TextureSource final {
    TextureSource(asset::AssetId id, asset::AssetUri uri)
        : asset_id(std::move(id)),
          source_uri(std::move(uri)) {}

    u32 schema{1};
    asset::AssetId asset_id;
    asset::AssetUri source_uri;
    TextureSemantic semantic{TextureSemantic::Color};
    TextureColorSpace color_space{TextureColorSpace::Srgb};
    TextureShape dimension{TextureShape::e2D};
    TextureMipPolicy mip_policy{TextureMipPolicy::Generate};
    TextureStreamingPolicy streaming{TextureStreamingPolicy::Full};
    u32 max_size{8192};
    TextureTargetPolicy target{TextureTargetPolicy::Auto};
    TextureSamplerDefaults sampler;
};

[[nodiscard]] Result<TextureSource> ParseTextureSource(std::string_view jsonc);

#ifndef __EMSCRIPTEN__
class TextureBuilder final : public asset::AssetBuilder {
public:
    explicit TextureBuilder(ref<const asset::Vfs> vfs);
    [[nodiscard]] const asset::BuilderDescriptor& Descriptor() const noexcept override;
    [[nodiscard]] Result<asset::Product> Build(const asset::BuildRequest& request, asset::BuildContext& context, std::span<const std::byte> source) const override;

private:
    ref<const asset::Vfs> vfs_;
    asset::BuilderDescriptor descriptor_;
};
#endif

} // namespace woki::gfx
