#pragma once

#include <cstddef>

#include "../detail/arithmetic.hpp"

namespace woki::math {
template <std::size_t N, arithmetic T>
requires(N >= 2)
class vec;

} // namespace woki::math
