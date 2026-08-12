#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>

#include "../error/result.hpp"
#include "../types/types.hpp"

namespace woki {

class ContentHash {
public:
    static constexpr std::size_t kSize = 32;

    constexpr ContentHash() noexcept = default;

    explicit constexpr ContentHash(std::array<u8, kSize> bytes) noexcept
        : bytes_(bytes) {}

    [[nodiscard]] static Result<ContentHash> Parse(std::string_view hex);

    [[nodiscard]] constexpr const std::array<u8, kSize>& Bytes() const noexcept {
        return bytes_;
    }

    [[nodiscard]] std::string Hex() const;

    [[nodiscard]] friend constexpr auto operator<=>(const ContentHash&, const ContentHash&) noexcept = default;

private:
    std::array<u8, kSize> bytes_{};
};

[[nodiscard]] ContentHash Sha256(std::span<const std::byte> bytes) noexcept;
[[nodiscard]] ContentHash Sha256(std::string_view text) noexcept;

} // namespace woki

template <>
struct std::hash<woki::ContentHash> {
    [[nodiscard]] std::size_t operator()(const woki::ContentHash& value) const noexcept;
};
