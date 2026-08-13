#pragma once

#include <memory>
#include <mutex>
#include <thread>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

#include <woki/core.hpp>
#include <woki/asset.hpp>
#include <woki/rhi/submission.hpp>

#include "epoch.hpp"

namespace woki::gfx {

enum class ResourceState : u8 {
    Empty,
    Loading,
    Ready,
    Failed,
    RebuildNeeded,
};

enum class ResidencyState : u8 {
    NonResident,
    UploadPending,
    Resident,
    Evicted,
    Lost,
};

struct ResidencyRecord final {
    ResourceState resource{ResourceState::Empty};
    ResidencyState residency{ResidencyState::NonResident};
    ContentVersion content_version;
    ResidencyVersion residency_version;
    rhi::SubmissionTicket last_used;
    u64 estimated_bytes{};
    bool fallback{};
};

struct ResourceBudgetStats final {
    u64 budget_bytes{};
    u64 resident_bytes{};
    u64 pending_bytes{};
    u64 fallback_resources{};
    u64 evictions{};
};

void MarkPhysicalResidencyLost(ResidencyRecord& record) noexcept;

template <typename Tag>
using ResourceHandle = Handle<Tag>;

template <typename Record, typename Tag>
class ResourceRegistry final {
public:
    using Handle = ResourceHandle<Tag>;

    ResourceRegistry()
        : owner_(std::this_thread::get_id()) {}

    [[nodiscard]] Result<Handle> Create(const asset::AssetId asset, Record record) {
        if (!IsOwner())
            return Err(ErrorCode::InvalidState, "resource registry mutation is owner-thread only");
        std::unique_lock lock(mutex_);
        if (!asset || assets_.contains(asset))
            return Err(ErrorCode::ValidationInvalidState, "resource asset id is invalid or already registered");
        const Handle handle = slots_.Emplace(Slot{asset, std::make_shared<const Record>(std::move(record))});
        assets_.emplace(asset, handle);
        return Ok(handle);
    }

    [[nodiscard]] std::optional<Handle> Find(const asset::AssetId asset) const noexcept {
        std::shared_lock lock(mutex_);
        const auto found = assets_.find(asset);
        return found == assets_.end() ? std::nullopt : std::optional<Handle>(found->second);
    }

    [[nodiscard]] std::shared_ptr<const Record> Borrow(const Handle handle) const noexcept {
        std::shared_lock lock(mutex_);
        const auto* slot = slots_.TryGet(handle);
        return slot == nullptr ? nullptr : slot->record;
    }

    [[nodiscard]] Result<void> Publish(const Handle handle, Record record) {
        if (!IsOwner())
            return Err(ErrorCode::InvalidState, "resource publication requires an owner-thread live handle");
        std::unique_lock lock(mutex_);
        if (!Valid(handle))
            return Err(ErrorCode::InvalidState, "resource publication requires an owner-thread live handle");
        slots_.Get(handle).record = std::make_shared<const Record>(std::move(record));
        return Ok();
    }

    [[nodiscard]] Result<void> Remove(const Handle handle) {
        if (!IsOwner())
            return Err(ErrorCode::InvalidState, "resource removal requires an owner-thread live handle");
        std::unique_lock lock(mutex_);
        if (!Valid(handle))
            return Err(ErrorCode::InvalidState, "resource removal requires an owner-thread live handle");
        auto& slot = slots_.Get(handle);
        assets_.erase(slot.asset);
        static_cast<void>(slots_.Remove(handle));
        return Ok();
    }

    [[nodiscard]] size_t Size() const noexcept {
        std::shared_lock lock(mutex_);
        return slots_.Size();
    }

    [[nodiscard]] bool IsOwner() const noexcept {
        return owner_ == std::this_thread::get_id();
    }

private:
    struct Slot final {
        asset::AssetId asset;
        std::shared_ptr<const Record> record;
    };

    [[nodiscard]] bool Valid(const Handle handle) const noexcept {
        return slots_.Contains(handle);
    }

    std::thread::id owner_;
    mutable std::shared_mutex mutex_;
    SlotMap<Slot, Handle> slots_;
    std::unordered_map<asset::AssetId, Handle> assets_;
};

} // namespace woki::gfx
