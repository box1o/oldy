#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <sstream>

#include <ktx.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_PIC
#define STBI_NO_PNM
#include <stb_image.h>

#include <woki/gfx/advanced/texture_product.hpp>

namespace woki::gfx {
namespace {

struct Decoded final {
    u32 width{};
    u32 height{};
    u32 layers{1};
    u32 faces{1};
    std::vector<std::vector<std::byte>> mips;
};

using Ktx = std::unique_ptr<ktxTexture2, decltype(&ktxTexture2_Destroy)>;

f32 ToLinear(const u8 value) {
    const f32 normalized = static_cast<f32>(value) / 255.0F;
    return normalized <= 0.04045F ? normalized / 12.92F : std::pow((normalized + 0.055F) / 1.055F, 2.4F);
}

u8 ToSrgb(const f32 value) {
    const f32 encoded = value <= 0.0031308F ? value * 12.92F : 1.055F * std::pow(value, 1.0F / 2.4F) - 0.055F;
    return static_cast<u8>(std::clamp(std::lround(encoded * 255.0F), 0L, 255L));
}

std::vector<std::byte> Downsample(const std::span<const std::byte> source, const u32 width, const u32 height, const u32 images, const TextureSemantic semantic, const TextureColorSpace color_space) {
    const u32 output_width = std::max(1U, width / 2U);
    const u32 output_height = std::max(1U, height / 2U);
    std::vector<std::byte> output(static_cast<size_t>(output_width) * output_height * images * 4U);
    const auto sample = [&](const u32 image, const u32 x, const u32 y, const u32 channel) {
        const size_t offset = ((static_cast<size_t>(image) * height + std::min(y, height - 1U)) * width + std::min(x, width - 1U)) * 4U + channel;
        return std::to_integer<u8>(source[offset]);
    };
    for (u32 image = 0; image < images; ++image) {
        for (u32 y = 0; y < output_height; ++y) {
            for (u32 x = 0; x < output_width; ++x) {
                const size_t destination = ((static_cast<size_t>(image) * output_height + y) * output_width + x) * 4U;
                if (semantic == TextureSemantic::Normal) {
                    f32 nx{}, ny{}, nz{};
                    for (u32 yy = 0; yy < 2; ++yy)
                        for (u32 xx = 0; xx < 2; ++xx) {
                            nx += static_cast<f32>(sample(image, x * 2U + xx, y * 2U + yy, 0)) / 127.5F - 1.0F;
                            ny += static_cast<f32>(sample(image, x * 2U + xx, y * 2U + yy, 1)) / 127.5F - 1.0F;
                            nz += static_cast<f32>(sample(image, x * 2U + xx, y * 2U + yy, 2)) / 127.5F - 1.0F;
                        }
                    const f32 length = std::sqrt(nx * nx + ny * ny + nz * nz);
                    if (length > 0.00001F) {
                        nx /= length;
                        ny /= length;
                        nz /= length;
                    }
                    output[destination] = static_cast<std::byte>(static_cast<u8>(std::clamp(std::lround((nx * 0.5F + 0.5F) * 255.0F), 0L, 255L)));
                    output[destination + 1] = static_cast<std::byte>(static_cast<u8>(std::clamp(std::lround((ny * 0.5F + 0.5F) * 255.0F), 0L, 255L)));
                    output[destination + 2] = static_cast<std::byte>(static_cast<u8>(std::clamp(std::lround((nz * 0.5F + 0.5F) * 255.0F), 0L, 255L)));
                } else {
                    for (u32 channel = 0; channel < 3; ++channel) {
                        f32 sum{};
                        for (u32 yy = 0; yy < 2; ++yy)
                            for (u32 xx = 0; xx < 2; ++xx) {
                                const u8 value = sample(image, x * 2U + xx, y * 2U + yy, channel);
                                sum += color_space == TextureColorSpace::Srgb ? ToLinear(value) : static_cast<f32>(value) / 255.0F;
                            }
                        output[destination + channel] = static_cast<std::byte>(color_space == TextureColorSpace::Srgb ? ToSrgb(sum * 0.25F) : static_cast<u8>(std::clamp(std::lround(sum * 63.75F), 0L, 255L)));
                    }
                }
                u32 alpha{};
                for (u32 yy = 0; yy < 2; ++yy)
                    for (u32 xx = 0; xx < 2; ++xx)
                        alpha += sample(image, x * 2U + xx, y * 2U + yy, 3);
                output[destination + 3] = static_cast<std::byte>(static_cast<u8>((alpha + 2U) / 4U));
            }
        }
    }
    return output;
}

void GenerateMips(Decoded& image, const TextureSource& source) {
    u32 width = image.width;
    u32 height = image.height;
    while (width > 1 || height > 1) {
        image.mips.push_back(Downsample(image.mips.back(), width, height, image.layers * image.faces, source.semantic, source.color_space));
        width = std::max(1U, width / 2U);
        height = std::max(1U, height / 2U);
    }
}

Result<Decoded> DecodeImage(const std::span<const std::byte> bytes) {
    if (bytes.size() >= 2 && bytes[0] == std::byte{'P'} && bytes[1] == std::byte{'3'}) {
        std::istringstream input(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
        std::string magic;
        u32 width{}, height{}, maximum{};
        if (!(input >> magic >> width >> height >> maximum) || magic != "P3" || width == 0 || height == 0 || maximum != 255)
            return Err(ErrorCode::ParseInvalidFormat, "ASCII PPM header is invalid");
        Decoded result{width, height, 1, 1, {std::vector<std::byte>(static_cast<size_t>(width) * height * 4U)}};
        for (size_t pixel = 0; pixel < static_cast<size_t>(width) * height; ++pixel) {
            u32 red{}, green{}, blue{};
            if (!(input >> red >> green >> blue) || red > 255 || green > 255 || blue > 255)
                return Err(ErrorCode::ParseInvalidFormat, "ASCII PPM pixel data is invalid");
            result.mips.front()[pixel * 4U] = static_cast<std::byte>(red);
            result.mips.front()[pixel * 4U + 1U] = static_cast<std::byte>(green);
            result.mips.front()[pixel * 4U + 2U] = static_cast<std::byte>(blue);
            result.mips.front()[pixel * 4U + 3U] = std::byte{255};
        }
        return Ok(std::move(result));
    }
    int width{}, height{}, components{};
    stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size()), &width, &height, &components, 4);
    if (pixels == nullptr || width <= 0 || height <= 0)
        return Err(ErrorCode::ParseInvalidFormat, std::string("PNG/JPEG decode failed: ") + stbi_failure_reason());
    const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4U;
    Decoded result{static_cast<u32>(width), static_cast<u32>(height), 1, 1, {}};
    result.mips.emplace_back(size);
    std::memcpy(result.mips.front().data(), pixels, size);
    stbi_image_free(pixels);
    return Ok(std::move(result));
}

Result<Decoded> DecodeKtx(const std::span<const std::byte> bytes, const TextureSource& source) {
    ktxTexture2* raw{};
    const auto created = ktxTexture2_CreateFromMemory(reinterpret_cast<const ktx_uint8_t*>(bytes.data()), bytes.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &raw);
    if (created != KTX_SUCCESS)
        return Err(ErrorCode::ParseInvalidFormat, "KTX2 memory decode failed");
    Ktx texture(raw, &ktxTexture2_Destroy);
    if (raw->baseDepth != 1 || raw->numDimensions != 2 || raw->baseWidth == 0 || raw->baseHeight == 0 || raw->numLevels == 0)
        return Err(ErrorCode::ValidationInvalidState, "only non-empty 2D and cube KTX2 textures are supported");
    if (ktxTexture2_NeedsTranscoding(raw) && ktxTexture2_TranscodeBasis(raw, KTX_TTF_RGBA32, 0) != KTX_SUCCESS)
        return Err(ErrorCode::ParseInvalidFormat, "Basis Universal transcoding to RGBA8 failed");
    constexpr ktx_uint32_t kVkRgba8Unorm = 37;
    constexpr ktx_uint32_t kVkRgba8Srgb = 43;
    if (raw->vkFormat != kVkRgba8Unorm && raw->vkFormat != kVkRgba8Srgb)
        return Err(ErrorCode::GraphicsUnsupportedApi, "KTX2 format is not portable RGBA8 and does not require Basis transcoding");
    Decoded result{raw->baseWidth, raw->baseHeight, std::max(1U, raw->numLayers), raw->numFaces, {}};
    if (result.faces != 1 && result.faces != 6)
        return Err(ErrorCode::ValidationInvalidState, "KTX2 face count must be one or six");
    u32 width = result.width, height = result.height;
    for (u32 level = 0; level < raw->numLevels; ++level) {
        const size_t image_size = static_cast<size_t>(width) * height * 4U;
        std::vector<std::byte> mip(image_size * result.layers * result.faces);
        size_t destination{};
        for (u32 layer = 0; layer < result.layers; ++layer)
            for (u32 face = 0; face < result.faces; ++face) {
                ktx_size_t offset{};
                if (ktxTexture_GetImageOffset(reinterpret_cast<ktxTexture*>(texture.get()), level, layer, face, &offset) != KTX_SUCCESS || offset > raw->dataSize || image_size > raw->dataSize - offset)
                    return Err(ErrorCode::ParseInvalidFormat, "KTX2 mip image range is invalid");
                std::memcpy(mip.data() + destination, raw->pData + offset, image_size);
                destination += image_size;
            }
        result.mips.push_back(std::move(mip));
        width = std::max(1U, width / 2U);
        height = std::max(1U, height / 2U);
    }
    if ((source.dimension == TextureShape::Cube) != (result.faces == 6))
        return Err(ErrorCode::ValidationInvalidState, "KTX2 face count does not match the authored dimension");
    return Ok(std::move(result));
}

bool IsKtx2(const std::span<const std::byte> bytes) {
    constexpr std::array signature{std::byte{0xAB}, std::byte{'K'}, std::byte{'T'}, std::byte{'X'}, std::byte{' '}, std::byte{'2'}, std::byte{'0'}, std::byte{0xBB}, std::byte{0x0D}, std::byte{0x0A}, std::byte{0x1A},
        std::byte{0x0A}};
    return bytes.size() >= signature.size() && std::ranges::equal(signature, bytes.first(signature.size()));
}

} // namespace

TextureBuilder::TextureBuilder(ref<const asset::Vfs> vfs)
    : vfs_(std::move(vfs)),
      descriptor_{asset::AssetId::FromName("woki.gfx.texture-builder"), 1, kTextureSourceType, kTextureProductType, "TextureBuilder"} {}

const asset::BuilderDescriptor& TextureBuilder::Descriptor() const noexcept {
    return descriptor_;
}

Result<asset::Product> TextureBuilder::Build(const asset::BuildRequest& request, asset::BuildContext& context, const std::span<const std::byte> descriptor_source) const {
    if (vfs_ == nullptr)
        return Err(ErrorCode::InvalidState, "texture builder has no VFS");
    auto source = ParseTextureSource(std::string_view(reinterpret_cast<const char*>(descriptor_source.data()), descriptor_source.size()));
    if (!source)
        return Err(std::move(source).error());
    if (source->asset_id != request.asset_id)
        return Err(ErrorCode::ValidationInvalidState, "texture descriptor asset_id does not match the build request");
    std::vector<std::byte> source_bytes;
    TRY_ASSIGN(source_bytes, vfs_->ReadBinary(source->source_uri, 512U * 1024U * 1024U));
    const ContentHash image_hash = Sha256(source_bytes);
    const asset::AssetId source_identity = asset::AssetId::FromName(source->source_uri.String());
    TRY_VOID(context.AddSourceDependency(source_identity, image_hash));
    Decoded decoded;
    if (IsKtx2(source_bytes))
        TRY_ASSIGN(decoded, DecodeKtx(source_bytes, *source));
    else
        TRY_ASSIGN(decoded, DecodeImage(source_bytes));
    if (source->dimension == TextureShape::Cube && decoded.faces == 1 && decoded.width == 1 && decoded.height == 1) {
        std::vector<std::byte> faces;
        faces.reserve(decoded.mips.front().size() * 6U);
        for (u32 face = 0; face < 6; ++face)
            faces.insert(faces.end(), decoded.mips.front().begin(), decoded.mips.front().end());
        decoded.mips.front() = std::move(faces);
        decoded.faces = 6;
    }
    if (source->dimension == TextureShape::Cube && decoded.faces != 6)
        return Err(ErrorCode::ValidationInvalidState, "cube textures require KTX2 or a 1x1 source replicated across six faces");
    while (decoded.width > source->max_size || decoded.height > source->max_size) {
        decoded.mips = {Downsample(decoded.mips.front(), decoded.width, decoded.height, decoded.layers * decoded.faces, source->semantic, source->color_space)};
        decoded.width = std::max(1U, decoded.width / 2U);
        decoded.height = std::max(1U, decoded.height / 2U);
    }
    if (source->mip_policy == TextureMipPolicy::None)
        decoded.mips.resize(1);
    else if (source->mip_policy == TextureMipPolicy::Generate) {
        decoded.mips.resize(1);
        GenerateMips(decoded, *source);
    }
    TextureMetadata metadata{.format_class = TextureFormatClass::Uncompressed,
        .format = source->color_space == TextureColorSpace::Srgb ? PortableTextureFormat::Rgba8Srgb : PortableTextureFormat::Rgba8Unorm,
        .semantic = source->semantic,
        .color_space = source->color_space,
        .dimension = source->dimension,
        .streaming = source->streaming,
        .width = decoded.width,
        .height = decoded.height,
        .layers = decoded.layers,
        .faces = decoded.faces,
        .mip_count = static_cast<u32>(decoded.mips.size()),
        .sampler = source->sampler};
    return MakeTextureProduct(request.asset_id, metadata, decoded.mips, request.source_hash, {});
}

} // namespace woki::gfx
