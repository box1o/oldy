#pragma once

#include "texture.hpp"

namespace woki::gfx {

inline constexpr u32 kTextureSourceType = 0x43525854U;  // TXRC
inline constexpr u32 kTextureProductType = 0x52505854U; // TXPR
inline constexpr u32 kTextureProductVersion = 2;

enum class TextureFormatClass : u8 { Uncompressed, BlockCompressed, Basis };
enum class PortableTextureFormat : u8 { Rgba8Unorm, Rgba8Srgb };

struct TextureMipChunk final {
    u32 level{};
    u32 width{};
    u32 height{};
    u64 offset{};
    u64 size{};
    ContentHash checksum;
    [[nodiscard]] friend auto operator<=>(const TextureMipChunk&, const TextureMipChunk&) noexcept = default;
};

struct TextureMetadata final {
    TextureFormatClass format_class{TextureFormatClass::Uncompressed};
    PortableTextureFormat format{PortableTextureFormat::Rgba8Unorm};
    TextureSemantic semantic{TextureSemantic::Color};
    TextureColorSpace color_space{TextureColorSpace::Linear};
    TextureShape dimension{TextureShape::e2D};
    TextureStreamingPolicy streaming{TextureStreamingPolicy::Full};
    u32 width{};
    u32 height{};
    u32 layers{1};
    u32 faces{1};
    u32 mip_count{};
    TextureSamplerDefaults sampler;
};

struct TextureProduct final {
    TextureMetadata metadata;
    std::vector<TextureMipChunk> mips;
    std::vector<asset::ProductDependency> dependencies;
    std::span<const std::byte> bytes;

    [[nodiscard]] std::span<const std::byte> Mip(u32 level) const noexcept;
};

struct TextureProductLimits final {
    u64 max_bytes{512U * 1024U * 1024U};
    u32 max_mips{32};
    u32 max_dependencies{4096};
    u32 max_dimension{32768};
};

[[nodiscard]] Result<std::vector<std::byte>> SerializeTextureProduct(const TextureMetadata& metadata,
    std::span<const std::vector<std::byte>> mips,
    std::span<const asset::ProductDependency> dependencies,
    TextureProductLimits limits = {});
[[nodiscard]] Result<TextureProduct> ParseTextureProduct(std::span<const std::byte> bytes, TextureProductLimits limits = {});
[[nodiscard]] Result<TextureProduct> ParseTextureProductHeader(std::span<const std::byte> bytes, TextureProductLimits limits = {});
[[nodiscard]] Result<TextureProduct> ReadTextureProductHeader(const asset::ProductReader& reader, TextureProductLimits limits = {});
[[nodiscard]] Result<std::vector<std::byte>> ReadTextureMip(const asset::ProductReader& reader, u32 level);
[[nodiscard]] Result<asset::Product> MakeTextureProduct(asset::AssetId id,
    const TextureMetadata& metadata,
    std::span<const std::vector<std::byte>> mips,
    ContentHash source_hash,
    std::vector<asset::ProductDependency> dependencies = {});

} // namespace woki::gfx
