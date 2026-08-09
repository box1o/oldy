#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <cstddef>
#include <functional>
#include <string_view>

#include "../types/types.hpp"

namespace woki {

[[nodiscard]] constexpr u64 HashBytes(const void* data, std::size_t size) noexcept {
    constexpr u64 kOffsetBasis = 14695981039346656037ull;
    constexpr u64 kPrime = 1099511628211ull;

    const auto* bytes = static_cast<const unsigned char*>(data);
    u64 hash = kOffsetBasis;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= static_cast<u64>(bytes[i]);
        hash *= kPrime;
    }
    return hash;
}

[[nodiscard]] constexpr u64 HashString(std::string_view value) noexcept {
    return HashBytes(value.data(), value.size());
}

template <typename T>
[[nodiscard]] inline std::size_t HashValue(const T& value) {
    return std::hash<T>{}(value);
}

template <>
[[nodiscard]] inline std::size_t HashValue<std::string_view>(const std::string_view& value) {
    return static_cast<std::size_t>(HashString(value));
}

template <typename T>
inline void HashCombine(std::size_t& seed, const T& value) {
    seed ^= HashValue(value) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
}

} // namespace woki
