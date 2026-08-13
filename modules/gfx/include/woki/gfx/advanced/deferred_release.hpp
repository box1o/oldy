#pragma once

#include <vector>
#include <utility>
#include <concepts>
#include <algorithm>

#include <woki/rhi/submission.hpp>

namespace woki::gfx {

// Retire and Collect are owner-thread operations. Resources left pending may only
// be destroyed after the owning device is idle or has entered teardown.
class DeferredReleaseQueue final {
public:
    template <std::movable T>
    void Retire(T resource, const rhi::SubmissionTicket safe_after) {
        pending_.push_back({safe_after, createScope<Owned<T>>(std::move(resource))});
    }

    size_t Collect(const rhi::SubmissionEpoch completed) {
        const size_t before = pending_.size();
        std::erase_if(pending_, [&](const Entry& entry) { return !entry.safe_after.IsValid() || completed.HasReached(entry.safe_after); });
        return before - pending_.size();
    }

    [[nodiscard]] size_t PendingCount() const noexcept {
        return pending_.size();
    }

    [[nodiscard]] size_t UnprovenCount() const noexcept {
        return static_cast<size_t>(std::ranges::count_if(pending_, [](const Entry& entry) { return !entry.safe_after.IsValid(); }));
    }

    // Device teardown is the only path where backend ownership, rather than a
    // completion watermark, proves that pending native resources may be abandoned.
    size_t AbandonForDeviceLoss() noexcept {
        const size_t count = pending_.size();
        pending_.clear();
        return count;
    }

private:
    struct Ownership {
        virtual ~Ownership() = default;
    };

    template <typename T>
    struct Owned final : Ownership {
        explicit Owned(T value)
            : value_(std::move(value)) {}

        T value_;
    };

    struct Entry {
        rhi::SubmissionTicket safe_after;
        scope<Ownership> ownership;
    };

    std::vector<Entry> pending_;
};

} // namespace woki::gfx
