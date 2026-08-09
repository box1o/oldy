#pragma once

#include "./fwd.hpp"
#include "./base.hpp"
#include "./mat2.hpp"              // IWYU pragma: keep
#include "./mat3.hpp"              // IWYU pragma: keep
#include "./mat4.hpp"              // IWYU pragma: keep
#include "../common/constants.hpp" // IWYU pragma: keep
#include "../common/functions.hpp" // IWYU pragma: keep

namespace woki::math {

template <std::size_t Rows, std::size_t Cols, arithmetic T>
[[nodiscard]] constexpr mat<Cols, Rows, T> transpose(const mat<Rows, Cols, T>& m) noexcept {
    return m.transpose();
}

template <std::size_t N, arithmetic T>
[[nodiscard]] constexpr T determinant(const mat<N, N, T>& m) noexcept {
    return det(m);
}

template <std::size_t N, arithmetic T>
[[nodiscard]] constexpr T trace(const mat<N, N, T>& m) noexcept {
    T result{};
    for (std::size_t i = 0; i < N; ++i) {
        result += m(i, i);
    }
    return result;
}

} // namespace woki::math
