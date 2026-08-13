#pragma once

#include <compare>

#include <woki/core.hpp>

namespace woki::rhi {

// Submission epochs identify queue progress. They are not CPU frame indices.
class SubmissionTicket;

class SubmissionEpoch final {
public:
    constexpr SubmissionEpoch() noexcept = default;

    explicit constexpr SubmissionEpoch(const u64 value) noexcept
        : value_(value) {}

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return value_ != 0;
    }

    [[nodiscard]] constexpr u64 Value() const noexcept {
        return value_;
    }

    [[nodiscard]] constexpr bool HasReached(const SubmissionTicket& ticket) const noexcept;
    [[nodiscard]] friend constexpr auto operator<=>(const SubmissionEpoch&, const SubmissionEpoch&) = default;

private:
    u64 value_{};
};

class SubmissionTicket final {
public:
    constexpr SubmissionTicket() noexcept = default;

    explicit constexpr SubmissionTicket(const u64 value) noexcept
        : value_(value) {}

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return value_ != 0;
    }

    [[nodiscard]] constexpr u64 Value() const noexcept {
        return value_;
    }

    [[nodiscard]] friend constexpr auto operator<=>(const SubmissionTicket&, const SubmissionTicket&) = default;

private:
    u64 value_{};
};

constexpr bool SubmissionEpoch::HasReached(const SubmissionTicket& ticket) const noexcept {
    return ticket.IsValid() && value_ >= ticket.Value();
}

enum class SubmissionTrackingStatus : u8 {
    Healthy,
    CompletionRegistrationFailed,
    CompletionCallbackFailed,
};

} // namespace woki::rhi
