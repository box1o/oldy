#pragma once

#include <map>

#include <woki/rhi/device.hpp>

#include "deferred_release.hpp"
#include "texture.hpp"

namespace woki::gfx {

struct TextureViewKey final {
    u64 physical_id{};
    rhi::TextureFormat format{rhi::TextureFormat::Undefined};
    rhi::TextureViewDimension dimension{rhi::TextureViewDimension::Undefined};
    u32 base_mip{};
    u32 mip_count{1};
    u32 base_layer{};
    u32 layer_count{1};
    rhi::TextureAspect aspect{rhi::TextureAspect::All};
    [[nodiscard]] friend auto operator<=>(const TextureViewKey&, const TextureViewKey&) noexcept = default;
};

struct SamplerKey final {
    rhi::AddressMode address_u{rhi::AddressMode::ClampToEdge};
    rhi::AddressMode address_v{rhi::AddressMode::ClampToEdge};
    rhi::AddressMode address_w{rhi::AddressMode::ClampToEdge};
    rhi::FilterMode mag_filter{rhi::FilterMode::Nearest};
    rhi::FilterMode min_filter{rhi::FilterMode::Nearest};
    rhi::MipmapFilterMode mip_filter{rhi::MipmapFilterMode::Nearest};
    u32 lod_min_bits{};
    u32 lod_max_bits{0x42000000U};
    rhi::CompareFunction compare{rhi::CompareFunction::Undefined};
    u16 max_anisotropy{1};
    [[nodiscard]] friend auto operator<=>(const SamplerKey&, const SamplerKey&) noexcept = default;
};

[[nodiscard]] SamplerKey MakeSamplerKey(const TextureSamplerDefaults& defaults) noexcept;
[[nodiscard]] ContentHash HashTextureViewKey(const TextureViewKey& key) noexcept;
[[nodiscard]] ContentHash HashSamplerKey(const SamplerKey& key) noexcept;

class TextureViewCache final {
public:
    explicit TextureViewCache(ref<rhi::Device> device);
    [[nodiscard]] Result<ref<rhi::TextureView>> GetOrCreate(const TextureViewKey& key, const rhi::Texture& texture);
    void MarkUsed(const TextureViewKey& key, rhi::SubmissionTicket submission);
    [[nodiscard]] size_t Prune(DeferredReleaseQueue& releases);
    void Clear() noexcept;

private:
    struct Entry {
        ref<rhi::TextureView> view;
        rhi::SubmissionTicket last_used;
    };

    ref<rhi::Device> device_;
    std::map<TextureViewKey, Entry> entries_;
};

class SamplerCache final {
public:
    explicit SamplerCache(ref<rhi::Device> device);
    [[nodiscard]] Result<ref<rhi::Sampler>> GetOrCreate(const SamplerKey& key);
    void MarkUsed(const SamplerKey& key, rhi::SubmissionTicket submission);
    [[nodiscard]] size_t Prune(DeferredReleaseQueue& releases);
    void Clear() noexcept;

private:
    struct Entry {
        ref<rhi::Sampler> sampler;
        rhi::SubmissionTicket last_used;
    };

    ref<rhi::Device> device_;
    std::map<SamplerKey, Entry> entries_;
};

} // namespace woki::gfx
