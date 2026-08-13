#include <array>
#include <limits>

#include <woki/gfx/advanced/texture_product.hpp>
#include "binary_codec.hpp"

namespace woki::gfx {
namespace {

constexpr std::array kMagic{std::byte{'W'}, std::byte{'T'}, std::byte{'E'}, std::byte{'X'}};
constexpr u64 kHeaderSize = 62;
constexpr u64 kDependencySize = asset::AssetId::kSize + ContentHash::kSize;
constexpr u64 kMipSize = 64;

using Writer = detail::BinaryWriter;
using Reader = detail::BinaryReader;

bool MetadataValid(const TextureMetadata& value, const TextureProductLimits limits) {
    return value.format_class == TextureFormatClass::Uncompressed && value.format <= PortableTextureFormat::Rgba8Srgb && value.semantic <= TextureSemantic::Environment && value.color_space <= TextureColorSpace::Srgb
           && value.dimension <= TextureShape::Cube && value.streaming <= TextureStreamingPolicy::StreamMips && value.width > 0 && value.height > 0 && value.width <= limits.max_dimension
           && value.height <= limits.max_dimension && value.layers > 0 && value.faces > 0 && value.faces == (value.dimension == TextureShape::Cube ? 6U : 1U) && value.mip_count > 0 && value.mip_count <= limits.max_mips
           && value.sampler.address_u <= TextureAddressMode::Mirror && value.sampler.address_v <= TextureAddressMode::Mirror && value.sampler.address_w <= TextureAddressMode::Mirror
           && value.sampler.mag_filter <= TextureFilter::Linear && value.sampler.min_filter <= TextureFilter::Linear && value.sampler.mip_filter <= TextureFilter::Linear && value.sampler.max_anisotropy > 0
           && value.sampler.max_anisotropy <= 16;
}

void WriteMetadata(Writer& out, const TextureMetadata& value) {
    out.Int(static_cast<u8>(value.format_class));
    out.Int(static_cast<u8>(value.format));
    out.Int(static_cast<u8>(value.semantic));
    out.Int(static_cast<u8>(value.color_space));
    out.Int(static_cast<u8>(value.dimension));
    out.Int(static_cast<u8>(value.streaming));
    out.Int(static_cast<u8>(value.sampler.address_u));
    out.Int(static_cast<u8>(value.sampler.address_v));
    out.Int(static_cast<u8>(value.sampler.address_w));
    out.Int(static_cast<u8>(value.sampler.mag_filter));
    out.Int(static_cast<u8>(value.sampler.min_filter));
    out.Int(static_cast<u8>(value.sampler.mip_filter));
    out.Int(value.sampler.max_anisotropy);
    out.Int(value.width);
    out.Int(value.height);
    out.Int(value.layers);
    out.Int(value.faces);
    out.Int(value.mip_count);
}

bool ReadMetadata(Reader& in, TextureMetadata& value) {
    u8 format_class{}, format{}, semantic{}, color_space{}, dimension{}, streaming{}, address_u{}, address_v{}, address_w{}, mag{}, min{}, mip{};
    if (!in.Int(format_class) || !in.Int(format) || !in.Int(semantic) || !in.Int(color_space) || !in.Int(dimension) || !in.Int(streaming) || !in.Int(address_u) || !in.Int(address_v) || !in.Int(address_w) || !in.Int(mag)
        || !in.Int(min) || !in.Int(mip) || !in.Int(value.sampler.max_anisotropy) || !in.Int(value.width) || !in.Int(value.height) || !in.Int(value.layers) || !in.Int(value.faces) || !in.Int(value.mip_count))
        return false;
    value.format_class = static_cast<TextureFormatClass>(format_class);
    value.format = static_cast<PortableTextureFormat>(format);
    value.semantic = static_cast<TextureSemantic>(semantic);
    value.color_space = static_cast<TextureColorSpace>(color_space);
    value.dimension = static_cast<TextureShape>(dimension);
    value.streaming = static_cast<TextureStreamingPolicy>(streaming);
    value.sampler.address_u = static_cast<TextureAddressMode>(address_u);
    value.sampler.address_v = static_cast<TextureAddressMode>(address_v);
    value.sampler.address_w = static_cast<TextureAddressMode>(address_w);
    value.sampler.mag_filter = static_cast<TextureFilter>(mag);
    value.sampler.min_filter = static_cast<TextureFilter>(min);
    value.sampler.mip_filter = static_cast<TextureFilter>(mip);
    return true;
}

} // namespace

std::span<const std::byte> TextureProduct::Mip(const u32 level) const noexcept {
    const auto found = std::ranges::find(mips, level, &TextureMipChunk::level);
    if (found == mips.end() || found->offset > bytes.size() || found->size > bytes.size() - found->offset)
        return {};
    return bytes.subspan(static_cast<size_t>(found->offset), static_cast<size_t>(found->size));
}

Result<std::vector<std::byte>> SerializeTextureProduct(const TextureMetadata& metadata,
    const std::span<const std::vector<std::byte>> mip_bytes,
    const std::span<const asset::ProductDependency> dependencies,
    const TextureProductLimits limits) {
    if (!MetadataValid(metadata, limits) || mip_bytes.size() != metadata.mip_count || dependencies.size() > limits.max_dependencies)
        return Err(ErrorCode::ValidationInvalidState, "texture metadata or record counts are invalid");
    if (!std::ranges::is_sorted(dependencies) || std::ranges::adjacent_find(dependencies, {}, &asset::ProductDependency::asset_id) != dependencies.end())
        return Err(ErrorCode::ValidationInvalidState, "texture dependencies must be canonical");
    const u64 data_offset = kHeaderSize + dependencies.size() * kDependencySize + mip_bytes.size() * kMipSize;
    u64 total = data_offset;
    for (const auto& mip : mip_bytes) {
        if (mip.empty() || mip.size() > limits.max_bytes - std::min<u64>(total, limits.max_bytes))
            return Err(ErrorCode::OutOfRange, "texture mip data exceeds configured limits");
        total += mip.size();
    }
    Writer out;
    out.bytes.insert(out.bytes.end(), kMagic.begin(), kMagic.end());
    out.Int(u16{kTextureProductVersion});
    out.Int(u16{});
    WriteMetadata(out, metadata);
    out.Int(static_cast<u32>(dependencies.size()));
    out.Int(kHeaderSize);
    out.Int(data_offset);
    for (const auto& dependency : dependencies) {
        out.Id(dependency.asset_id);
        out.Hash(dependency.product_hash);
    }
    u64 offset = data_offset;
    u32 width = metadata.width, height = metadata.height;
    for (u32 level = 0; level < metadata.mip_count; ++level) {
        const auto& mip = mip_bytes[level];
        out.Int(level);
        out.Int(width);
        out.Int(height);
        out.Int(u32{});
        out.Int(offset);
        out.Int(static_cast<u64>(mip.size()));
        out.Hash(Sha256(mip));
        offset += mip.size();
        width = std::max(1U, width / 2U);
        height = std::max(1U, height / 2U);
    }
    for (const auto& mip : mip_bytes)
        out.bytes.insert(out.bytes.end(), mip.begin(), mip.end());
    return Ok(std::move(out.bytes));
}

static Result<TextureProduct> ParseTextureProductImpl(const std::span<const std::byte> bytes, const TextureProductLimits limits, const bool header_only) {
    if (bytes.size() < kHeaderSize || bytes.size() > limits.max_bytes || !std::ranges::equal(kMagic, bytes.first(kMagic.size())))
        return Err(ErrorCode::ParseInvalidFormat, "texture product magic, size, or header is invalid");
    Reader in(bytes.subspan(kMagic.size()));
    TextureProduct result;
    u16 version{}, flags{};
    u32 dependency_count{};
    u64 dependency_offset{}, data_offset{};
    if (!in.Int(version) || !in.Int(flags) || !ReadMetadata(in, result.metadata) || !in.Int(dependency_count) || !in.Int(dependency_offset) || !in.Int(data_offset) || version != kTextureProductVersion || flags != 0
        || !MetadataValid(result.metadata, limits) || dependency_count > limits.max_dependencies || dependency_offset != kHeaderSize)
        return Err(ErrorCode::ParseInvalidFormat, "texture product header is invalid");
    const u64 expected_data = kHeaderSize + static_cast<u64>(dependency_count) * kDependencySize + static_cast<u64>(result.metadata.mip_count) * kMipSize;
    if (data_offset != expected_data || (header_only ? data_offset != bytes.size() : data_offset > bytes.size()))
        return Err(ErrorCode::ParseInvalidFormat, "texture product table offsets are invalid");
    result.dependencies.resize(dependency_count);
    for (auto& dependency : result.dependencies)
        if (!in.Id(dependency.asset_id) || !in.Hash(dependency.product_hash))
            return Err(ErrorCode::ParseInvalidFormat, "texture dependency table is truncated");
    if (!std::ranges::is_sorted(result.dependencies) || std::ranges::adjacent_find(result.dependencies, {}, &asset::ProductDependency::asset_id) != result.dependencies.end())
        return Err(ErrorCode::ParseInvalidFormat, "texture dependencies are not canonical");
    result.mips.resize(result.metadata.mip_count);
    u64 expected_offset = data_offset;
    u32 expected_width = result.metadata.width, expected_height = result.metadata.height;
    for (u32 index = 0; index < result.metadata.mip_count; ++index) {
        auto& mip = result.mips[index];
        u32 reserved{};
        if (!in.Int(mip.level) || !in.Int(mip.width) || !in.Int(mip.height) || !in.Int(reserved) || !in.Int(mip.offset) || !in.Int(mip.size) || !in.Hash(mip.checksum) || reserved != 0 || mip.level != index
            || mip.width != expected_width || mip.height != expected_height || mip.offset != expected_offset || mip.size == 0 || (!header_only && (mip.offset > bytes.size() || mip.size > bytes.size() - mip.offset)))
            return Err(ErrorCode::ParseInvalidFormat, "texture mip table is invalid");
        const auto data = header_only ? std::span<const std::byte>{} : bytes.subspan(static_cast<size_t>(mip.offset), static_cast<size_t>(mip.size));
        if (!header_only && mip.checksum != Sha256(data))
            return Err(ErrorCode::ParseInvalidFormat, "texture mip checksum is invalid");
        expected_offset += mip.size;
        expected_width = std::max(1U, expected_width / 2U);
        expected_height = std::max(1U, expected_height / 2U);
    }
    if ((!header_only && expected_offset != bytes.size()) || in.Offset() + kMagic.size() != data_offset)
        return Err(ErrorCode::ParseInvalidFormat, "texture product is truncated or contains trailing bytes");
    result.bytes = bytes;
    return Ok(std::move(result));
}

Result<TextureProduct> ParseTextureProduct(const std::span<const std::byte> bytes, const TextureProductLimits limits) {
    return ParseTextureProductImpl(bytes, limits, false);
}

Result<TextureProduct> ParseTextureProductHeader(const std::span<const std::byte> bytes, const TextureProductLimits limits) {
    return ParseTextureProductImpl(bytes, limits, true);
}

Result<TextureProduct> ReadTextureProductHeader(const asset::ProductReader& reader, const TextureProductLimits limits) {
    if (reader.Header().type != kTextureProductType || reader.Chunks().empty() || reader.Chunks().front().semantic != asset::ProductChunkSemantic::Header)
        return Err(ErrorCode::ParseInvalidFormat, "asset product does not expose a texture header chunk");
    auto bytes = reader.ReadChunk(0);
    return bytes ? ParseTextureProductHeader(*bytes, limits) : Result<TextureProduct>(Err(std::move(bytes).error()));
}

Result<std::vector<std::byte>> ReadTextureMip(const asset::ProductReader& reader, const u32 level) {
    u32 current{};
    for (std::size_t index = 0; index < reader.Chunks().size(); ++index)
        if (reader.Chunks()[index].semantic == asset::ProductChunkSemantic::TextureMip) {
            if (current++ == level)
                return reader.ReadChunk(index);
        }
    return Err(ErrorCode::OutOfRange, "texture mip is unavailable");
}

Result<asset::Product> MakeTextureProduct(const asset::AssetId id,
    const TextureMetadata& metadata,
    const std::span<const std::vector<std::byte>> mips,
    ContentHash source_hash,
    std::vector<asset::ProductDependency> dependencies) {
    std::ranges::sort(dependencies);
    std::vector<std::byte> payload;
    TRY_ASSIGN(payload, SerializeTextureProduct(metadata, mips, dependencies));
    auto product = asset::MakeProduct(id, kTextureProductType, kTextureProductVersion, 1, std::move(source_hash), std::move(dependencies), Sha256("woki.gfx.texture.rgba8.v1"), std::move(payload));
    auto parsed = ParseTextureProduct(product.payload);
    if (!parsed)
        return Err(std::move(parsed).error());
    product.chunks.clear();
    const u64 first_mip = parsed->mips.front().offset;
    product.chunks.push_back({asset::ProductChunkSemantic::Header, asset::ProductCompression::None, 1, 0, first_mip, first_mip, Sha256(std::span(product.payload).first(static_cast<size_t>(first_mip)))});
    for (const auto& mip : parsed->mips)
        product.chunks.push_back({asset::ProductChunkSemantic::TextureMip, asset::ProductCompression::None, 1, mip.offset, mip.size, mip.size, mip.checksum});
    product.product_hash = asset::HashProduct(product);
    return Ok(std::move(product));
}

} // namespace woki::gfx
