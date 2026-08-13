#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <compare>
#include <limits>

#include "../types/types.hpp"

namespace woki {

template <typename Tag>
class Version final {
public:
    constexpr Version() noexcept = default;

    explicit constexpr Version(const u64 value) noexcept
        : value_(value) {}

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return value_ != 0;
    }

    [[nodiscard]] constexpr u64 Value() const noexcept {
        return value_;
    }

    [[nodiscard]] constexpr bool Increment() noexcept {
        if (value_ == std::numeric_limits<u64>::max())
            return false;
        ++value_;
        return true;
    }

    [[nodiscard]] friend constexpr auto operator<=>(const Version&, const Version&) noexcept = default;

private:
    u64 value_{};
};

} // namespace woki
