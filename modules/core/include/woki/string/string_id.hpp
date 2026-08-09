#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <string_view>

#include "../hash/hash.hpp"
#include "../types/types.hpp"

namespace woki {

class StringId {
public:
    constexpr StringId() noexcept = default;

    explicit constexpr StringId(u64 value) noexcept
        : value_(value) {}

    explicit constexpr StringId(std::string_view value) noexcept
        : value_(HashString(value)) {}

    [[nodiscard]] constexpr u64 Value() const noexcept {
        return value_;
    }

    [[nodiscard]] constexpr bool Empty() const noexcept {
        return value_ == 0;
    }

    [[nodiscard]] friend constexpr bool operator==(StringId lhs, StringId rhs) noexcept = default;
    [[nodiscard]] friend constexpr auto operator<=>(StringId lhs, StringId rhs) noexcept = default;

private:
    u64 value_ = 0;
};

[[nodiscard]] constexpr StringId ToStringId(std::string_view value) noexcept {
    return StringId(value);
}

} // namespace woki

template <>
struct std::hash<woki::StringId> {
    [[nodiscard]] std::size_t operator()(woki::StringId value) const noexcept {
        return static_cast<std::size_t>(value.Value());
    }
};
