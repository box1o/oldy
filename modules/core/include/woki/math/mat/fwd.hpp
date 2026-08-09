#pragma once

#include <cstddef>

#include "../detail/arithmetic.hpp"

namespace woki::math {

template <std::size_t Rows, std::size_t Cols, arithmetic T>
class mat;

namespace detail {

template <std::size_t N, floating_point T>
[[nodiscard]] inline mat<N, N, T> InvertMatrix(const mat<N, N, T>& matrix) noexcept {
    mat<N, N, T> a(matrix);
    mat<N, N, T> inverse = mat<N, N, T>::identity();

    for (std::size_t i = 0; i < N; ++i) {
        std::size_t pivot = i;
        T largest = a(i, i) < T{} ? -a(i, i) : a(i, i);
        for (std::size_t row = i + 1; row < N; ++row) {
            const T value = a(row, i) < T{} ? -a(row, i) : a(row, i);
            if (value > largest) {
                largest = value;
                pivot = row;
            }
        }

        if (largest == T{}) {
            return mat<N, N, T>::identity();
        }

        if (pivot != i) {
            for (std::size_t column = 0; column < N; ++column) {
                const T a_value = a(i, column);
                a(i, column) = a(pivot, column);
                a(pivot, column) = a_value;

                const T inverse_value = inverse(i, column);
                inverse(i, column) = inverse(pivot, column);
                inverse(pivot, column) = inverse_value;
            }
        }

        const T divisor = a(i, i);
        for (std::size_t column = 0; column < N; ++column) {
            a(i, column) /= divisor;
            inverse(i, column) /= divisor;
        }

        for (std::size_t row = 0; row < N; ++row) {
            if (row == i) {
                continue;
            }
            const T factor = a(row, i);
            for (std::size_t column = 0; column < N; ++column) {
                a(row, column) -= factor * a(i, column);
                inverse(row, column) -= factor * inverse(i, column);
            }
        }
    }

    return inverse;
}

} // namespace detail

} // namespace woki::math
