#pragma once

#include <limits>
#include <numbers>

#include "../detail/arithmetic.hpp"

namespace woki::math {

template <floating_point T>
inline constexpr T pi = std::numbers::pi_v<T>;
template <floating_point T>
inline constexpr T two_pi = std::numbers::pi_v<T> * T{2};
template <floating_point T>
inline constexpr T half_pi = std::numbers::pi_v<T> / T{2};
template <floating_point T>
inline constexpr T epsilon = std::numeric_limits<T>::epsilon();
template <floating_point T>
inline constexpr T infinity = std::numeric_limits<T>::infinity();

} // namespace woki::math
