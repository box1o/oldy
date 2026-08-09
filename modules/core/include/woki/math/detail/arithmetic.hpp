#pragma once

#include <cstdint>
#include <concepts>

namespace woki::math {

template <typename T>
concept integral = std::integral<T>;

template <typename T>
concept floating_point = std::floating_point<T>;

template <typename T>
concept arithmetic = integral<T> || floating_point<T>;

template <typename T>
concept signed_arithmetic = arithmetic<T> && std::signed_integral<T>;

template <typename T>
concept unsigned_arithmetic = arithmetic<T> && std::unsigned_integral<T>;

enum class layout : std::uint16_t { rowm, colm };

} // namespace woki::math
