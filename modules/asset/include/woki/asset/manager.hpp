#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

#include <functional>
#include <optional>
#include <span>
#include <string>

#include <woki/task.hpp>

#include "id.hpp"
#include "product.hpp"

namespace woki::asset {

template <typename T>
class TypedAssetLease;

enum class AssetState : u8 { Unloaded, Queued, Loading, Ready, Failed };
enum class AssetPriority : u8 { Prefetch, Normal, High };

struct AssetRequestOptions final {
    task::CancellationToken cancellation;
    AssetPriority priority{AssetPriority::Normal};
    bool prefetch{};
};

struct AssetVersion {
    u64 revision{};
    u64 generation{};
    ContentHash product_hash;
};

struct AssetError {
    ErrorCode code{ErrorCode::Success};
    std::string message;
};

struct AssetGeneration {
    AssetKey key;
    u32 type{};
    AssetVersion version;
    std::vector<std::byte> bytes;
};

class AssetLease {
public:
    AssetLease() = default;

    explicit AssetLease(ref<const AssetGeneration> generation)
        : generation_(std::move(generation)) {}

    [[nodiscard]] explicit operator bool() const noexcept {
        return generation_ != nullptr;
    }

    [[nodiscard]] const AssetGeneration& Get() const noexcept {
        return *generation_;
    }

    [[nodiscard]] std::span<const std::byte> Bytes() const noexcept {
        return generation_->bytes;
    }

    template <typename T, typename Decoder>
    [[nodiscard]] Result<TypedAssetLease<T>> Decode(Decoder&& decode) const {
        if (!generation_)
            return Err(ErrorCode::InvalidState, "cannot decode an empty asset lease");
        auto value = std::invoke(std::forward<Decode>(decode), Bytes());
        if (!value)
            return Err(std::move(value).error());
        return Ok(TypedAssetLease<T>(*this, createRef<const T>(std::move(*value))));
    }

private:
    ref<const AssetGeneration> generation_;
};

template <typename T>
class TypedAssetLease {
public:
    [[nodiscard]] const T& Get() const noexcept {
        return *value_;
    }

    [[nodiscard]] const AssetLease& Raw() const noexcept {
        return raw_;
    }

private:
    friend class AssetLease;

    TypedAssetLease(AssetLease raw, ref<const T> value)
        : raw_(std::move(raw)),
          value_(std::move(value)) {}

    AssetLease raw_;
    ref<const T> value_;
};

struct AssetStatus {
    AssetState state{AssetState::Unloaded};
    AssetVersion version;
    std::optional<AssetError> error;
    bool in_flight{};
};

struct AssetManagerOptions {
    std::size_t worker_count{};
    std::size_t io_workers{1};
    std::size_t queue_capacity{256};
    std::size_t publication_capacity{256};
    std::size_t max_waiters_per_asset{256};
    std::size_t max_prefetch_requests{64};
    std::size_t max_decoded_bytes{512U * 1024U * 1024U};
};

class AssetManager {
public:
    using Load = std::function<Result<Product>(AssetId, task::CancellationToken)>;
    explicit AssetManager(Load load, AssetManagerOptions options = {});
    ~AssetManager();
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    [[nodiscard]] Result<task::Future<AssetLease>> Request(AssetId id, task::CancellationToken cancellation = {});
    [[nodiscard]] Result<task::Future<AssetLease>> Request(AssetId id, AssetRequestOptions options);
    [[nodiscard]] Result<void> Prefetch(AssetId id, AssetPriority priority = AssetPriority::Prefetch);
    [[nodiscard]] std::optional<AssetLease> Borrow(AssetId id) const;
    [[nodiscard]] AssetStatus Status(AssetId id) const;
    void Invalidate(AssetId id);
    void Invalidate(std::span<const AssetId> ids);
    [[nodiscard]] std::size_t ReleaseUnused();
    [[nodiscard]] std::size_t PumpPublications(std::size_t limit = static_cast<std::size_t>(-1));
    [[nodiscard]] Result<std::vector<AssetLease>> PublishAtomic(std::span<const Product> products);

private:
    class Impl;
    scope<Impl> impl_;
};

} // namespace woki::asset
