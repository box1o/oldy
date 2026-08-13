#pragma once

// IWYU pragma: private, include "woki/task.hpp"

#include <atomic>
#include <memory>

namespace woki::task {

namespace detail {
struct CancellationState {
    std::atomic_bool requested{false};
};
} // namespace detail

class CancellationToken {
public:
    CancellationToken() = default;

    [[nodiscard]] bool IsCancellationRequested() const noexcept {
        return state_ != nullptr && state_->requested.load(std::memory_order_acquire);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return state_ != nullptr;
    }

private:
    explicit CancellationToken(std::shared_ptr<const detail::CancellationState> state)
        : state_(std::move(state)) {}

    std::shared_ptr<const detail::CancellationState> state_;
    friend class CancellationSource;
};

class CancellationSource {
public:
    CancellationSource()
        : state_(std::make_shared<detail::CancellationState>()) {}

    [[nodiscard]] CancellationToken Token() const noexcept {
        return CancellationToken(state_);
    }

    [[nodiscard]] bool RequestCancellation() noexcept {
        return state_ != nullptr && !state_->requested.exchange(true, std::memory_order_acq_rel);
    }

    [[nodiscard]] bool IsCancellationRequested() const noexcept {
        return state_ != nullptr && state_->requested.load(std::memory_order_acquire);
    }

private:
    std::shared_ptr<detail::CancellationState> state_;
};

} // namespace woki::task
