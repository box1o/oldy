#pragma once

#include <cmath>

#include "./constants.hpp"
#include "../detail/arithmetic.hpp"

namespace woki::math {

template <floating_point T>
[[nodiscard]] constexpr T radians(T degrees) noexcept {
    return degrees * (pi<T> / T{180});
}

template <floating_point T>
[[nodiscard]] constexpr T degrees(T radians_value) noexcept {
    return radians_value * (T{180} / pi<T>);
}

template <arithmetic T>
[[nodiscard]] constexpr T lerp(T a, T b, T t) noexcept {
    return a + t * (b - a);
}

template <arithmetic T>
[[nodiscard]] constexpr T sign(T value) noexcept {
    if constexpr (unsigned_arithmetic<T>) {
        return value == T{0} ? T{0} : T{1};
    } else {
        if (value > T{0})
            return T{1};
        if (value < T{0})
            return T{-1};
        return T{0};
    }
}

template <arithmetic T>
[[nodiscard]] constexpr T abs(T value) noexcept {
    if constexpr (floating_point<T>) {
        return std::fabs(value);
    } else {
        return value < T{0} ? -value : value;
    }
}

template <arithmetic T>
[[nodiscard]] constexpr T min(T a, T b) noexcept {
    return a < b ? a : b;
}

template <arithmetic T>
[[nodiscard]] constexpr T max(T a, T b) noexcept {
    return a > b ? a : b;
}

template <arithmetic T>
[[nodiscard]] constexpr T clamp(T value, T lo, T hi) noexcept {
    return min(max(value, lo), hi);
}

template <arithmetic T>
[[nodiscard]] inline T sqrt(T value) noexcept {
    if constexpr (floating_point<T>) {
        return std::sqrt(value);
    } else {
        return static_cast<T>(std::sqrt(static_cast<long double>(value)));
    }
}

template <floating_point T>
[[nodiscard]] constexpr bool approx_equal(T a, T b, T tolerance = epsilon<T>) noexcept {
    return abs(a - b) <= tolerance;
}

using std::acos;
using std::asin;
using std::atan;
using std::atan2;
using std::ceil;
using std::cos;
using std::exp;
using std::floor;
using std::pow;
using std::round;
using std::sin;
using std::tan;
using std::trunc;

} // namespace woki::math
