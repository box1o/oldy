#pragma once

#include <cstddef>

#include "../mat/fwd.hpp"
#include "../vec/fwd.hpp"
#include "../detail/arithmetic.hpp"

namespace woki::math {

template <std::size_t Rows, std::size_t Cols, arithmetic T>
[[nodiscard]] constexpr vec<Rows, T> operator*(const mat<Rows, Cols, T>& m, const vec<Cols, T>& v) noexcept {
    vec<Rows, T> result{};
    for (std::size_t i = 0; i < Rows; ++i) {
        T sum{};
        for (std::size_t j = 0; j < Cols; ++j)
            sum += m(i, j) * v[j];
        result[i] = sum;
    }
    return result;
}

template <std::size_t Rows, std::size_t Cols, arithmetic T>
[[nodiscard]] constexpr vec<Cols, T> operator*(const vec<Rows, T>& v, const mat<Rows, Cols, T>& m) noexcept {
    vec<Cols, T> result{};
    for (std::size_t j = 0; j < Cols; ++j) {
        T sum{};
        for (std::size_t i = 0; i < Rows; ++i)
            sum += v[i] * m(i, j);
        result[j] = sum;
    }
    return result;
}

} // namespace woki::math
