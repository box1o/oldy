#include <bit>

#include <woki/gfx/advanced/texture_cache.hpp>
#include <woki/gfx/advanced/texture.hpp>
#include <woki/rhi/objects.hpp>

namespace woki::gfx {
namespace {

template <typename T>
void Append(std::vector<std::byte>& bytes, const T value) {
    using U = std::make_unsigned_t<T>;
    const U bits = static_cast<U>(value);
    for (size_t index = 0; index < sizeof(T); ++index)
        bytes.push_back(static_cast<std::byte>((bits >> (index * 8U)) & 0xffU));
}

rhi::AddressMode Address(const TextureAddressMode value) {
    switch (value) {
        case TextureAddressMode::Clamp:
            return rhi::AddressMode::ClampToEdge;
        case TextureAddressMode::Repeat:
            return rhi::AddressMode::Repeat;
        case TextureAddressMode::Mirror:
            return rhi::AddressMode::MirrorRepeat;
    }
    return rhi::AddressMode::ClampToEdge;
}

rhi::FilterMode Filter(const TextureFilter value) {
    return value == TextureFilter::Linear ? rhi::FilterMode::Linear : rhi::FilterMode::Nearest;
}

rhi::MipmapFilterMode MipFilter(const TextureFilter value) {
    return value == TextureFilter::Linear ? rhi::MipmapFilterMode::Linear : rhi::MipmapFilterMode::Nearest;
}

} // namespace

SamplerKey MakeSamplerKey(const TextureSamplerDefaults& defaults) noexcept {
    return {.address_u = Address(defaults.address_u),
        .address_v = Address(defaults.address_v),
        .address_w = Address(defaults.address_w),
        .mag_filter = Filter(defaults.mag_filter),
        .min_filter = Filter(defaults.min_filter),
        .mip_filter = MipFilter(defaults.mip_filter),
        .lod_min_bits = std::bit_cast<u32>(0.0F),
        .lod_max_bits = std::bit_cast<u32>(32.0F),
        .compare = rhi::CompareFunction::Undefined,
        .max_anisotropy = defaults.max_anisotropy};
}

ContentHash HashTextureViewKey(const TextureViewKey& key) noexcept {
    std::vector<std::byte> bytes;
    bytes.reserve(40);
    Append(bytes, key.physical_id);
    Append(bytes, static_cast<u32>(key.format));
    Append(bytes, static_cast<u32>(key.dimension));
    Append(bytes, key.base_mip);
    Append(bytes, key.mip_count);
    Append(bytes, key.base_layer);
    Append(bytes, key.layer_count);
    Append(bytes, static_cast<u32>(key.aspect));
    return Sha256(bytes);
}

ContentHash HashSamplerKey(const SamplerKey& key) noexcept {
    std::vector<std::byte> bytes;
    bytes.reserve(40);
    Append(bytes, static_cast<u32>(key.address_u));
    Append(bytes, static_cast<u32>(key.address_v));
    Append(bytes, static_cast<u32>(key.address_w));
    Append(bytes, static_cast<u32>(key.mag_filter));
    Append(bytes, static_cast<u32>(key.min_filter));
    Append(bytes, static_cast<u32>(key.mip_filter));
    Append(bytes, key.lod_min_bits);
    Append(bytes, key.lod_max_bits);
    Append(bytes, static_cast<u32>(key.compare));
    Append(bytes, key.max_anisotropy);
    return Sha256(bytes);
}

TextureViewCache::TextureViewCache(ref<rhi::Device> device)
    : device_(std::move(device)) {}

Result<ref<rhi::TextureView>> TextureViewCache::GetOrCreate(const TextureViewKey& key, const rhi::Texture& texture) {
    if (key.physical_id == 0 || key.mip_count == 0 || key.layer_count == 0)
        return Err(ErrorCode::ValidationInvalidState, "texture view key is invalid");
    if (const auto found = entries_.find(key); found != entries_.end())
        return Ok(found->second.view);
    auto view = texture.CreateView({.format = key.format,
        .dimension = key.dimension,
        .base_mip_level = key.base_mip,
        .mip_level_count = key.mip_count,
        .base_array_layer = key.base_layer,
        .array_layer_count = key.layer_count,
        .aspect = key.aspect,
        .usage = rhi::TextureUsage::TextureBinding,
        .label = "TextureView"});
    if (view == nullptr)
        return Err(ErrorCode::GraphicsResourceCreationFailed, "RHI texture view creation failed");
    ref<rhi::TextureView> retained(std::move(view));
    entries_.emplace(key, Entry{retained, {}});
    return Ok(std::move(retained));
}

void TextureViewCache::MarkUsed(const TextureViewKey& key, const rhi::SubmissionTicket submission) {
    const auto found = entries_.find(key);
    if (found != entries_.end() && found->second.last_used < submission)
        found->second.last_used = submission;
}

size_t TextureViewCache::Prune(DeferredReleaseQueue& releases) {
    const size_t before = entries_.size();
    std::erase_if(entries_, [&](auto& item) {
        auto& entry = item.second;
        if (entry.view.use_count() != 1 || !entry.last_used.IsValid())
            return false;
        releases.Retire(std::move(entry.view), entry.last_used);
        return true;
    });
    return before - entries_.size();
}

void TextureViewCache::Clear() noexcept {
    entries_.clear();
    device_.reset();
}

SamplerCache::SamplerCache(ref<rhi::Device> device)
    : device_(std::move(device)) {}

Result<ref<rhi::Sampler>> SamplerCache::GetOrCreate(const SamplerKey& key) {
    if (key.max_anisotropy == 0)
        return Err(ErrorCode::ValidationInvalidState, "sampler key is invalid");
    if (const auto found = entries_.find(key); found != entries_.end())
        return Ok(found->second.sampler);
    if (device_ == nullptr)
        return Err(ErrorCode::GraphicsDeviceLost, "sampler cache has no device");
    auto sampler = device_->CreateSampler({.address_mode_u = key.address_u,
        .address_mode_v = key.address_v,
        .address_mode_w = key.address_w,
        .mag_filter = key.mag_filter,
        .min_filter = key.min_filter,
        .mipmap_filter = key.mip_filter,
        .lod_min_clamp = std::bit_cast<f32>(key.lod_min_bits),
        .lod_max_clamp = std::bit_cast<f32>(key.lod_max_bits),
        .compare = key.compare,
        .max_anisotropy = key.max_anisotropy,
        .label = "TextureSampler"});
    if (!sampler)
        return Err(std::move(sampler).error());
    ref<rhi::Sampler> retained(std::move(*sampler));
    entries_.emplace(key, Entry{retained, {}});
    return Ok(std::move(retained));
}

void SamplerCache::MarkUsed(const SamplerKey& key, const rhi::SubmissionTicket submission) {
    const auto found = entries_.find(key);
    if (found != entries_.end() && found->second.last_used < submission)
        found->second.last_used = submission;
}

size_t SamplerCache::Prune(DeferredReleaseQueue& releases) {
    const size_t before = entries_.size();
    std::erase_if(entries_, [&](auto& item) {
        auto& entry = item.second;
        if (entry.sampler.use_count() != 1 || !entry.last_used.IsValid())
            return false;
        releases.Retire(std::move(entry.sampler), entry.last_used);
        return true;
    });
    return before - entries_.size();
}

void SamplerCache::Clear() noexcept {
    entries_.clear();
    device_.reset();
}

} // namespace woki::gfx
