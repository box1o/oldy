#include <algorithm>
#include <array>
#include <cstring>
#include <unordered_set>

#include <woki/gfx/advanced/texture_library.hpp>
#include <woki/rhi/queue.hpp>
#include "gfx_util.hpp"

namespace woki::gfx {
namespace {

constexpr u64 kUploadPieceBytes = 4U * 1024U * 1024U;

rhi::TextureFormat Format(const PortableTextureFormat format) {
    return format == PortableTextureFormat::Rgba8Srgb ? rhi::TextureFormat::RGBA8UnormSrgb : rhi::TextureFormat::RGBA8Unorm;
}

} // namespace

struct TextureLibrary::Impl final {
    struct PhysicalVersionTag;

    struct Slot final {
        TextureRecord record;
        ref<TexturePhysical> physical;
        ref<TexturePhysical> candidate;
        std::optional<task::Future<asset::AssetLease>> pending;
        std::unordered_set<u64> upload_work;
        rhi::SubmissionTicket upload_submission;
        u64 candidate_generation{};
        TextureMetadata candidate_metadata;
        TextureProductLocator candidate_product;
        asset::AssetVersion candidate_version;
        u64 candidate_estimated_bytes{};
        u64 pending_bytes{};
        bool builtin{};
    };

    asset::AssetManager& assets;
    ref<rhi::Device> device;
    UploadScheduler* uploads;
    ref<DeferredReleaseQueue> releases;
    TextureBudget budget;
    scope<TextureViewCache> views;
    scope<SamplerCache> samplers;
    SlotMap<Slot, TextureHandle> slots;
    std::unordered_map<asset::AssetId, TextureHandle> by_asset;
    std::array<ref<TexturePhysical>, 7> fallbacks;
    ::woki::Version<PhysicalVersionTag> next_physical{1};
    u64 evictions{};
    u64 fallback_resolves{};
    std::unordered_set<asset::AssetId> fallback_logged;
    bool device_lost{};

    Impl(asset::AssetManager& asset_manager, ref<rhi::Device> physical_device, UploadScheduler& scheduler, ref<DeferredReleaseQueue> release_queue, const TextureBudget texture_budget)
        : assets(asset_manager),
          device(std::move(physical_device)),
          uploads(&scheduler),
          releases(std::move(release_queue)),
          budget(texture_budget),
          views(createScope<TextureViewCache>(device)),
          samplers(createScope<SamplerCache>(device)) {}

    [[nodiscard]] bool Valid(const TextureHandle handle) const noexcept {
        return slots.Contains(handle);
    }

    [[nodiscard]] Result<void> RequestLoad(Slot& slot) {
        if (slot.pending || device_lost)
            return Ok();
        auto future = assets.Request(slot.record.asset_id);
        if (!future) {
            slot.record.state = TextureState::Failed;
            slot.record.diagnostic = std::string(future.error().Message());
            slog::Warn("Texture {} request failed: {}", slot.record.asset_id.String(), slot.record.diagnostic);
            return Err(std::move(future).error());
        }
        slot.pending = std::move(*future);
        slot.record.state = TextureState::Loading;
        return Ok();
    }

    [[nodiscard]] Result<ref<TexturePhysical>> CreatePhysical(const TextureMetadata& metadata, const std::string_view label) {
        if (device == nullptr)
            return Err(ErrorCode::GraphicsDeviceLost, "texture library has no device");
        const auto format = Format(metadata.format);
        const u32 array_layers = metadata.layers * metadata.faces;
        auto texture = device->CreateTexture({.size = {metadata.width, metadata.height, array_layers},
            .mip_level_count = metadata.mip_count,
            .sample_count = 1,
            .dimension = rhi::TextureDimension::e2D,
            .format = format,
            .usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::CopyDst,
            .label = std::string(label)});
        if (!texture)
            return Err(std::move(texture).error());
        auto physical = createRef<TexturePhysical>();
        physical->id = next_physical.Value();
        if (!next_physical.Increment())
            return Err(ErrorCode::FailedToAcquireResource, "texture physical version space exhausted");
        physical->texture = ref<rhi::Texture>(std::move(*texture));
        physical->view_key = {.physical_id = physical->id,
            .format = format,
            .dimension = metadata.dimension == TextureShape::Cube ? (metadata.layers == 1 ? rhi::TextureViewDimension::Cube : rhi::TextureViewDimension::CubeArray)
                                                                  : (metadata.layers == 1 ? rhi::TextureViewDimension::e2D : rhi::TextureViewDimension::e2DArray),
            .base_mip = 0,
            .mip_count = metadata.mip_count,
            .base_layer = 0,
            .layer_count = array_layers,
            .aspect = rhi::TextureAspect::All};
        TRY_ASSIGN(physical->default_view, views->GetOrCreate(physical->view_key, *physical->texture));
        physical->sampler_key = MakeSamplerKey(metadata.sampler);
        TRY_ASSIGN(physical->sampler, samplers->GetOrCreate(physical->sampler_key));
        physical->estimated_bytes = static_cast<u64>(metadata.width) * metadata.height * array_layers * 4U;
        return Ok(std::move(physical));
    }

    [[nodiscard]] Result<void> EnqueueMip(Slot& slot, const TextureMipChunk& mip, const std::span<const std::byte> bytes) {
        const u32 images = slot.candidate_metadata.layers * slot.candidate_metadata.faces;
        const u32 row_bytes = mip.width * 4U;
        const u32 padded_row = detail::AlignUp(row_bytes, 256U);
        const u32 rows_per_piece = std::max(1U, static_cast<u32>(kUploadPieceBytes / padded_row));
        const size_t image_bytes = static_cast<size_t>(row_bytes) * mip.height;
        if (bytes.size() != image_bytes * images)
            return Err(ErrorCode::ParseInvalidFormat, "texture mip byte size does not match RGBA8 metadata");
        for (u32 image = 0; image < images; ++image) {
            for (u32 y = 0; y < mip.height; y += rows_per_piece) {
                const u32 rows = std::min(rows_per_piece, mip.height - y);
                std::vector<std::byte> upload(static_cast<size_t>(padded_row) * rows);
                for (u32 row = 0; row < rows; ++row) {
                    const size_t source = static_cast<size_t>(image) * image_bytes + static_cast<size_t>(y + row) * row_bytes;
                    const size_t destination = static_cast<size_t>(row) * padded_row;
                    std::memcpy(upload.data() + destination, bytes.data() + source, row_bytes);
                }
                WorkVersion work;
                TRY_ASSIGN(work, uploads->Enqueue(TextureUploadRequest{.target = slot.candidate->texture,
                                     .mip_level = mip.level,
                                     .origin = {0, y, image},
                                     .aspect = rhi::TextureAspect::All,
                                     .layout = {.offset = 0, .bytes_per_row = padded_row, .rows_per_image = rows},
                                     .extent = {mip.width, rows, 1},
                                     .bytes = std::move(upload),
                                     .publication = {}}));
                slot.upload_work.insert(work.Value());
            }
        }
        return Ok();
    }

    [[nodiscard]] Result<void> Accept(Slot& slot, const asset::AssetLease& lease) {
        if (lease.Get().type != kTextureProductType)
            return Err(ErrorCode::ParseInvalidFormat, "asset is not a texture product");
        TextureProduct product;
        TRY_ASSIGN(product, ParseTextureProduct(lease.Bytes()));
        slot.candidate_metadata = product.metadata;
        slot.candidate_product = {lease.Get().version.product_hash, product.mips};
        slot.candidate_version = lease.Get().version;
        slot.candidate_estimated_bytes = 0;
        for (const auto& mip : product.mips)
            slot.candidate_estimated_bytes += mip.size;
        slot.candidate_generation = lease.Get().version.generation;
        slot.record.state = TextureState::QueuedUpload;
        TRY_ASSIGN(slot.candidate, CreatePhysical(product.metadata, "Texture:" + slot.record.asset_id.String()));
        slot.upload_work.clear();
        for (const auto& mip : product.mips)
            TRY_VOID(EnqueueMip(slot, mip, product.Mip(mip.level)));
        slot.pending_bytes = slot.candidate_estimated_bytes;
        return Ok();
    }

    [[nodiscard]] Result<ref<TexturePhysical>> CreateFallback(const TextureFallback fallback) {
        const bool cube = fallback == TextureFallback::WhiteCube || fallback == TextureFallback::BlackCube;
        TextureMetadata metadata{.format_class = TextureFormatClass::Uncompressed,
            .format = PortableTextureFormat::Rgba8Unorm,
            .semantic = fallback == TextureFallback::FlatNormal ? TextureSemantic::Normal
                        : fallback == TextureFallback::Depth    ? TextureSemantic::Depth
                                                                : TextureSemantic::Color,
            .color_space = TextureColorSpace::Linear,
            .dimension = cube ? TextureShape::Cube : TextureShape::e2D,
            .streaming = TextureStreamingPolicy::Full,
            .width = 1,
            .height = 1,
            .layers = 1,
            .faces = cube ? 6U : 1U,
            .mip_count = 1,
            .sampler = {}};
        if (fallback == TextureFallback::Depth) {
            auto texture = device->CreateTexture({.size = {1, 1, 1},
                .mip_level_count = 1,
                .sample_count = 1,
                .dimension = rhi::TextureDimension::e2D,
                .format = rhi::TextureFormat::Depth32Float,
                .usage = rhi::TextureUsage::TextureBinding | rhi::TextureUsage::RenderAttachment,
                .label = "TextureFallbackDepth"});
            if (!texture)
                return Err(std::move(texture).error());
            auto physical = createRef<TexturePhysical>();
            physical->id = next_physical.Value();
            if (!next_physical.Increment())
                return Err(ErrorCode::FailedToAcquireResource, "texture physical version space exhausted");
            physical->texture = ref<rhi::Texture>(std::move(*texture));
            physical->view_key = {.physical_id = physical->id,
                .format = rhi::TextureFormat::Depth32Float,
                .dimension = rhi::TextureViewDimension::e2D,
                .base_mip = 0,
                .mip_count = 1,
                .base_layer = 0,
                .layer_count = 1,
                .aspect = rhi::TextureAspect::DepthOnly};
            TRY_ASSIGN(physical->default_view, views->GetOrCreate(physical->view_key, *physical->texture));
            TextureSamplerDefaults sampler;
            sampler.mag_filter = TextureFilter::Nearest;
            sampler.min_filter = TextureFilter::Nearest;
            sampler.mip_filter = TextureFilter::Nearest;
            physical->sampler_key = MakeSamplerKey(sampler);
            TRY_ASSIGN(physical->sampler, samplers->GetOrCreate(physical->sampler_key));
            physical->estimated_bytes = 4;
            return Ok(std::move(physical));
        }
        ref<TexturePhysical> physical;
        TRY_ASSIGN(physical, CreatePhysical(metadata, "TextureFallback"));
        std::array<u8, 4> color{255, 255, 255, 255};
        if (fallback == TextureFallback::Black2D || fallback == TextureFallback::BlackCube)
            color = {0, 0, 0, 255};
        if (fallback == TextureFallback::FlatNormal)
            color = {128, 128, 255, 255};
        if (fallback == TextureFallback::Error2D)
            color = {255, 0, 255, 255};
        std::vector<std::byte> bytes(256U);
        for (u32 channel = 0; channel < 4; ++channel)
            bytes[channel] = static_cast<std::byte>(color[channel]);
        for (u32 face = 0; face < metadata.faces; ++face)
            TRY_VOID(uploads->Enqueue(TextureUploadRequest{.target = physical->texture,
                .mip_level = 0,
                .origin = {0, 0, face},
                .aspect = rhi::TextureAspect::All,
                .layout = {.offset = 0, .bytes_per_row = 256, .rows_per_image = 1},
                .extent = {1, 1, 1},
                .bytes = bytes,
                .publication = {}}));
        return Ok(std::move(physical));
    }

    [[nodiscard]] Result<void> BuildFallbacks() {
        for (u8 value = 1; value <= static_cast<u8>(TextureFallback::Depth); ++value)
            TRY_ASSIGN(fallbacks[value - 1], CreateFallback(static_cast<TextureFallback>(value)));
        return Ok();
    }

    void Retire(ref<TexturePhysical> physical) {
        if (!physical)
            return;
        if (physical->last_used.IsValid())
            releases->Retire(std::move(physical), physical->last_used);
    }
};

TextureLibrary::TextureLibrary(scope<Impl> impl)
    : impl_(std::move(impl)) {}

TextureLibrary::~TextureLibrary() = default;

Result<scope<TextureLibrary>> TextureLibrary::Create(asset::AssetManager& assets, ref<rhi::Device> device, UploadScheduler& uploads, ref<DeferredReleaseQueue> releases, const TextureBudget budget) {
    if (device == nullptr || releases == nullptr || budget.resident_bytes == 0)
        return Err(ErrorCode::ValidationInvalidState, "texture library descriptor is invalid");
    auto impl = createScope<Impl>(assets, std::move(device), uploads, std::move(releases), budget);
    TRY_VOID(impl->BuildFallbacks());
    return Ok(scope<TextureLibrary>(new TextureLibrary(std::move(impl))));
}

Result<TextureHandle> TextureLibrary::Request(const asset::AssetId id) {
    if (!id)
        return Err(ErrorCode::InvalidArgument, "cannot request a nil texture asset ID");
    if (const auto found = impl_->by_asset.find(id); found != impl_->by_asset.end()) {
        auto& slot = impl_->slots.Get(found->second);
        TRY_VOID(impl_->RequestLoad(slot));
        return Ok(found->second);
    }
    const auto handle = impl_->slots.Emplace();
    auto& slot = impl_->slots.Get(handle);
    slot.record.asset_id = id;
    for (u8 semantic = 0; semantic <= static_cast<u8>(TextureSemantic::Environment); ++semantic)
        if (id == BuiltinFallbackTextureId(static_cast<TextureSemantic>(semantic))) {
            slot.builtin = true;
            slot.record.state = TextureState::Resident;
            slot.record.metadata.semantic = static_cast<TextureSemantic>(semantic);
            slot.record.version = {.revision = 1, .generation = 1, .product_hash = BuiltinFallbackTextureProductHash(static_cast<TextureSemantic>(semantic))};
            break;
        }
    if (!slot.builtin)
        slot.record.state = TextureState::Queued;
    impl_->by_asset.emplace(id, handle);
    if (!slot.builtin)
        TRY_VOID(impl_->RequestLoad(slot));
    return Ok(handle);
}

TextureState TextureLibrary::State(const TextureHandle handle) const noexcept {
    return impl_->Valid(handle) ? impl_->slots.Get(handle).record.state : TextureState::Unloaded;
}

Result<ResolvedTexture> TextureLibrary::Resolve(const TextureHandle handle, const TextureSemantic expected) {
    if (!impl_->Valid(handle))
        return Err(ErrorCode::InvalidArgument, "texture handle is stale or invalid");
    auto& slot = impl_->slots.Get(handle);
    if (slot.physical && slot.record.state == TextureState::Resident && slot.record.metadata.semantic == expected)
        return Ok(ResolvedTexture{slot.physical, TextureFallback::None, expected, {}});
    TextureFallback fallback = TextureFallback::Error2D;
    switch (expected) {
        case TextureSemantic::Normal:
            fallback = TextureFallback::FlatNormal;
            break;
        case TextureSemantic::Emissive:
            fallback = TextureFallback::Black2D;
            break;
        case TextureSemantic::Occlusion:
            fallback = TextureFallback::White2D;
            break;
        case TextureSemantic::Depth:
            fallback = TextureFallback::Depth;
            break;
        case TextureSemantic::Environment:
            fallback = TextureFallback::WhiteCube;
            break;
        case TextureSemantic::Color:
        case TextureSemantic::Data:
            fallback = slot.builtin ? TextureFallback::White2D : TextureFallback::Error2D;
            break;
    }
    ++impl_->fallback_resolves;
    if (impl_->fallback_logged.insert(slot.record.asset_id).second)
        slog::Warn("Texture fallback for {} state={} metadata_semantic={} expected={} diagnostic={}",
            slot.record.asset_id.String(),
            static_cast<u32>(slot.record.state),
            static_cast<u32>(slot.record.metadata.semantic),
            static_cast<u32>(expected),
            slot.record.diagnostic);
    std::string diagnostic = slot.record.diagnostic;
    if (slot.physical && slot.record.metadata.semantic != expected)
        diagnostic = "texture semantic mismatch";
    return Ok(ResolvedTexture{impl_->fallbacks[static_cast<u8>(fallback) - 1], fallback, expected, std::move(diagnostic)});
}

std::optional<TextureMetadata> TextureLibrary::Metadata(const TextureHandle handle) const {
    if (!impl_->Valid(handle) || impl_->slots.Get(handle).record.version.generation == 0)
        return std::nullopt;
    return impl_->slots.Get(handle).record.metadata;
}

asset::AssetVersion TextureLibrary::Version(const TextureHandle handle) const noexcept {
    return impl_->Valid(handle) ? impl_->slots.Get(handle).record.version : asset::AssetVersion{};
}

void TextureLibrary::MarkUsed(const TextureHandle handle, const rhi::SubmissionTicket submission) {
    if (!impl_->Valid(handle) || !submission.IsValid())
        return;
    auto& slot = impl_->slots.Get(handle);
    slot.record.last_used = std::max(slot.record.last_used, submission);
    if (slot.physical) {
        slot.physical->last_used = std::max(slot.physical->last_used, submission);
        impl_->views->MarkUsed(slot.physical->view_key, submission);
        impl_->samplers->MarkUsed(slot.physical->sampler_key, submission);
    }
}

Result<void> TextureLibrary::Pump() {
    if (impl_->device_lost)
        return Err(ErrorCode::GraphicsDeviceLost, "texture library device is lost");
    static_cast<void>(impl_->assets.PumpPublications());
    for (const auto handle : impl_->slots.Handles()) {
        auto& slot = impl_->slots.Get(handle);
        if (slot.builtin)
            continue;
        if (slot.pending && slot.pending->IsReady()) {
            auto loaded = slot.pending->Wait();
            slot.pending.reset();
            if (!loaded) {
                slot.record.state = slot.physical ? TextureState::Resident : TextureState::Failed;
                slot.record.diagnostic = std::string(loaded.error().Message());
                slog::Warn("Texture {} failed to load: {}", slot.record.asset_id.String(), slot.record.diagnostic);
            } else {
                auto accepted = impl_->Accept(slot, *loaded);
                if (!accepted) {
                    slot.record.state = slot.physical ? TextureState::Resident : TextureState::Failed;
                    slot.record.diagnostic = std::string(accepted.error().Message());
                    slot.candidate.reset();
                    slog::Warn("Texture {} rejected: {}", slot.record.asset_id.String(), slot.record.diagnostic);
                }
            }
        }
        const auto status = impl_->assets.Status(slot.record.asset_id);
        if (!slot.pending && !slot.candidate && (status.state == asset::AssetState::Unloaded || status.version.generation > slot.record.version.generation))
            static_cast<void>(impl_->RequestLoad(slot));
    }
    auto tickets = impl_->uploads->PrepareAndSubmit();
    if (!tickets)
        return Err(std::move(tickets).error());
    for (const auto& ticket : *tickets)
        for (const auto handle : impl_->slots.Handles()) {
            auto& slot = impl_->slots.Get(handle);
            if (slot.upload_work.erase(ticket.work.Value()) != 0 && slot.upload_submission < ticket.submission)
                slot.upload_submission = ticket.submission;
        }
    const auto completed = impl_->device->GetQueue().CompletedSubmission();
    static_cast<void>(impl_->uploads->PublishCompleted(completed));
    for (const auto handle : impl_->slots.Handles()) {
        auto& slot = impl_->slots.Get(handle);
        if (!slot.candidate)
            continue;
        slot.record.state = slot.upload_work.empty() ? TextureState::Uploading : TextureState::QueuedUpload;
        if (!slot.upload_work.empty() || !completed.HasReached(slot.upload_submission))
            continue;
        const auto current = impl_->assets.Status(slot.record.asset_id);
        if (current.version.generation != slot.candidate_generation) {
            impl_->Retire(std::move(slot.candidate));
            slot.pending_bytes = 0;
            continue;
        }
        impl_->Retire(std::move(slot.physical));
        slot.physical = std::move(slot.candidate);
        slot.pending_bytes = 0;
        slot.record.metadata = std::move(slot.candidate_metadata);
        slot.record.product = std::move(slot.candidate_product);
        slot.record.version = slot.candidate_version;
        slot.record.estimated_bytes = slot.candidate_estimated_bytes;
        slot.record.resident_base_mip = 0;
        slot.record.resident_mip_count = slot.record.metadata.mip_count;
        slot.record.state = TextureState::Resident;
        slot.record.diagnostic.clear();
    }
    static_cast<void>(EvictToBudget());
    return Ok();
}

Result<void> TextureLibrary::Evict(const TextureHandle handle) {
    if (!impl_->Valid(handle))
        return Err(ErrorCode::InvalidArgument, "texture handle is stale or invalid");
    auto& slot = impl_->slots.Get(handle);
    if (!slot.physical || slot.physical.use_count() != 1)
        return Err(ErrorCode::InvalidState, "texture is not resident or is externally referenced");
    slot.record.state = TextureState::Evicting;
    impl_->Retire(std::move(slot.physical));
    slot.record.resident_mip_count = 0;
    slot.record.state = TextureState::Evicted;
    ++impl_->evictions;
    return Ok();
}

size_t TextureLibrary::EvictToBudget() {
    u64 resident{};
    for (const auto handle : impl_->slots.Handles()) {
        const auto& slot = impl_->slots.Get(handle);
        if (slot.physical)
            resident += slot.record.estimated_bytes;
    }
    size_t count{};
    while (resident > impl_->budget.resident_bytes) {
        Impl::Slot* candidate{};
        for (const auto handle : impl_->slots.Handles()) {
            auto& slot = impl_->slots.Get(handle);
            if (slot.physical && slot.physical.use_count() == 1 && (candidate == nullptr || slot.record.last_used < candidate->record.last_used))
                candidate = &slot;
        }
        if (candidate == nullptr)
            break;
        resident -= candidate->record.estimated_bytes;
        impl_->Retire(std::move(candidate->physical));
        candidate->record.resident_mip_count = 0;
        candidate->record.state = TextureState::Evicted;
        ++impl_->evictions;
        ++count;
    }
    return count;
}

void TextureLibrary::MarkDeviceLost() noexcept {
    impl_->device_lost = true;
    impl_->slots.ForEach([](const TextureHandle, Impl::Slot& slot) {
        slot.physical.reset();
        slot.candidate.reset();
        slot.upload_work.clear();
        slot.pending_bytes = 0;
        slot.record.resident_mip_count = 0;
        slot.record.state = TextureState::Lost;
    });
    impl_->fallbacks = {};
    impl_->views->Clear();
    impl_->samplers->Clear();
    impl_->device.reset();
}

Result<scope<TextureLibrary>> TextureLibrary::PrepareReplacement(ref<rhi::Device> device, UploadScheduler& uploads, ref<DeferredReleaseQueue> releases) const {
    scope<TextureLibrary> replacement;
    TRY_ASSIGN(replacement, Create(impl_->assets, std::move(device), uploads, std::move(releases), impl_->budget));
    for (const auto handle : impl_->slots.Handles()) {
        TextureHandle cloned;
        TRY_ASSIGN(cloned, replacement->Request(impl_->slots.Get(handle).record.asset_id));
        if (cloned != handle)
            return Err(ErrorCode::InvalidState, "texture replacement changed a logical handle");
    }
    return Ok(std::move(replacement));
}

TextureStats TextureLibrary::Stats() const noexcept {
    TextureStats result{.budget_bytes = impl_->budget.resident_bytes, .records = static_cast<u32>(impl_->by_asset.size()), .evictions = impl_->evictions, .fallback_resolves = impl_->fallback_resolves};
    impl_->slots.ForEach([&result](const TextureHandle, const Impl::Slot& slot) {
        result.pending_upload_bytes += slot.pending_bytes;
        if (slot.physical) {
            result.resident_bytes += slot.record.estimated_bytes;
            ++result.resident;
            if (slot.physical.use_count() == 1)
                result.evictable_bytes += slot.record.estimated_bytes;
        }
        if (slot.record.state == TextureState::Failed)
            ++result.failed;
    });
    return result;
}

} // namespace woki::gfx
