#pragma once

#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "texture_cache.hpp"
#include "texture_product.hpp"
#include "upload.hpp"
#include "../handles.hpp"

namespace woki::gfx {

enum class TextureState : u8 { Unloaded, Queued, Loading, Transcoding, QueuedUpload, Uploading, Partial, Resident, Failed, Evicting, Evicted, Lost };
enum class TextureFallback : u8 { None, White2D, Black2D, FlatNormal, Error2D, WhiteCube, BlackCube, Depth };

struct TextureProductLocator final {
    ContentHash product_hash;
    std::vector<TextureMipChunk> mips;
};

struct TextureRecord final {
    asset::AssetId asset_id;
    TextureState state{TextureState::Unloaded};
    TextureMetadata metadata;
    TextureProductLocator product;
    asset::AssetVersion version;
    u32 resident_base_mip{};
    u32 resident_mip_count{};
    u64 estimated_bytes{};
    rhi::SubmissionTicket last_used;
    std::string diagnostic;
};

struct TexturePhysical final {
    u64 id{};
    ref<rhi::Texture> texture;
    ref<rhi::TextureView> default_view;
    ref<rhi::Sampler> sampler;
    TextureViewKey view_key;
    SamplerKey sampler_key;
    rhi::SubmissionTicket last_used;
    u64 estimated_bytes{};
};

struct ResolvedTexture final {
    ref<TexturePhysical> physical;
    TextureFallback fallback{TextureFallback::None};
    TextureSemantic expected_semantic{TextureSemantic::Color};
    std::string diagnostic;

    [[nodiscard]] bool UsedFallback() const noexcept {
        return fallback != TextureFallback::None;
    }
};

struct TextureBudget final {
    u64 resident_bytes{512U * 1024U * 1024U};
};

struct TextureStats final {
    u64 budget_bytes{};
    u64 resident_bytes{};
    u64 pending_upload_bytes{};
    u64 evictable_bytes{};
    u32 records{};
    u32 resident{};
    u32 failed{};
    u64 evictions{};
    u64 fallback_resolves{};
};

class TextureLibrary final {
public:
    [[nodiscard]] static Result<scope<TextureLibrary>> Create(asset::AssetManager& assets, ref<rhi::Device> device, UploadScheduler& uploads, ref<DeferredReleaseQueue> releases, TextureBudget budget = {});
    ~TextureLibrary();
    TextureLibrary(const TextureLibrary&) = delete;
    TextureLibrary& operator=(const TextureLibrary&) = delete;

    [[nodiscard]] Result<TextureHandle> Request(asset::AssetId id);
    [[nodiscard]] TextureState State(TextureHandle handle) const noexcept;
    [[nodiscard]] Result<ResolvedTexture> Resolve(TextureHandle handle, TextureSemantic expected_semantic);
    [[nodiscard]] std::optional<TextureMetadata> Metadata(TextureHandle handle) const;
    [[nodiscard]] asset::AssetVersion Version(TextureHandle handle) const noexcept;
    void MarkUsed(TextureHandle handle, rhi::SubmissionTicket submission);
    [[nodiscard]] Result<void> Pump();
    [[nodiscard]] Result<void> Evict(TextureHandle handle);
    [[nodiscard]] size_t EvictToBudget();
    void MarkDeviceLost() noexcept;
    [[nodiscard]] Result<scope<TextureLibrary>> PrepareReplacement(ref<rhi::Device> device, UploadScheduler& uploads, ref<DeferredReleaseQueue> releases) const;
    [[nodiscard]] TextureStats Stats() const noexcept;

private:
    struct Impl;
    explicit TextureLibrary(scope<Impl> impl);
    scope<Impl> impl_;
};

} // namespace woki::gfx
