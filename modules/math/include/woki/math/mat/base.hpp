#pragma once

#include <array>
#include <cassert>
#include <cstddef>

#include "fwd.hpp"
#include "../common/constants.hpp"
#include "../common/functions.hpp"
#include "../detail/arithmetic.hpp"

namespace woki::math {

// Forward declarations
template <std::size_t N, arithmetic T>
[[nodiscard]] constexpr T det(const mat<N, N, T>& m) noexcept;

template <std::size_t N, floating_point T>
[[nodiscard]] inline mat<N, N, T> inverse(const mat<N, N, T>& m) noexcept;

// NOTE: Column-major storage: data_[col][row]
template <std::size_t Rows, std::size_t Cols, arithmetic T>
class mat {
public:
    static constexpr std::size_t rows = Rows;
    static constexpr std::size_t cols = Cols;
    using value_type = T;
    using col_type = std::array<T, Rows>;
    using row_type = std::array<T, Cols>;

    constexpr mat() noexcept = default;

    explicit constexpr mat(T value) noexcept {
        for (std::size_t j = 0; j < Cols; ++j) {
            for (std::size_t i = 0; i < Rows; ++i) {
                data_[j][i] = value;
            }
        }
    }

    template <arithmetic... Args>
    requires(sizeof...(Args) == Rows * Cols)
    constexpr mat(layout order, Args... args) noexcept {
        T temp[] = {static_cast<T>(args)...};
        if (order == layout::colm) {
            std::size_t idx = 0;
            for (std::size_t j = 0; j < Cols; ++j) {
                for (std::size_t i = 0; i < Rows; ++i) {
                    data_[j][i] = temp[idx++];
                }
            }
        } else {
            for (std::size_t i = 0; i < Rows; ++i) {
                for (std::size_t j = 0; j < Cols; ++j) {
                    data_[j][i] = temp[i * Cols + j];
                }
            }
        }
    }

    template <arithmetic U>
    constexpr explicit mat(const mat<Rows, Cols, U>& other) noexcept {
        for (std::size_t j = 0; j < Cols; ++j) {
            for (std::size_t i = 0; i < Rows; ++i) {
                data_[j][i] = static_cast<T>(other(i, j));
            }
        }
    }

    static constexpr mat identity() noexcept
    requires(Rows == Cols)
    {
        mat result{};
        for (std::size_t i = 0; i < Rows; ++i) {
            result(i, i) = T{1};
        }
        return result;
    }

    [[nodiscard]] constexpr T& operator()(std::size_t r, std::size_t c) noexcept {
        assert(r < Rows && c < Cols);
        return data_[c][r];
    }

    [[nodiscard]] constexpr const T& operator()(std::size_t r, std::size_t c) const noexcept {
        assert(r < Rows && c < Cols);
        return data_[c][r];
    }

    [[nodiscard]] constexpr col_type& operator[](std::size_t col) noexcept {
        assert(col < Cols);
        return data_[col];
    }

    [[nodiscard]] constexpr const col_type& operator[](std::size_t col) const noexcept {
        assert(col < Cols);
        return data_[col];
    }

    [[nodiscard]] constexpr col_type& col(std::size_t j) noexcept {
        assert(j < Cols);
        return data_[j];
    }

    [[nodiscard]] constexpr const col_type& col(std::size_t j) const noexcept {
        assert(j < Cols);
        return data_[j];
    }

    constexpr void set_col(std::size_t j, const col_type& c) noexcept {
        assert(j < Cols);
        data_[j] = c;
    }

    [[nodiscard]] constexpr row_type row(std::size_t i) const noexcept {
        assert(i < Rows);
        row_type r{};
        for (std::size_t j = 0; j < Cols; ++j) {
            r[j] = data_[j][i];
        }
        return r;
    }

    constexpr void set_row(std::size_t i, const row_type& r) noexcept {
        assert(i < Rows);
        for (std::size_t j = 0; j < Cols; ++j) {
            data_[j][i] = r[j];
        }
    }

    [[nodiscard]] constexpr T* data() noexcept {
        return &data_[0][0];
    }

    [[nodiscard]] constexpr const T* data() const noexcept {
        return &data_[0][0];
    }

    constexpr mat& operator+=(const mat& rhs) noexcept {
        for (std::size_t j = 0; j < Cols; ++j) {
            for (std::size_t i = 0; i < Rows; ++i) {
                data_[j][i] += rhs.data_[j][i];
            }
        }
        return *this;
    }

    constexpr mat& operator-=(const mat& rhs) noexcept {
        for (std::size_t j = 0; j < Cols; ++j) {
            for (std::size_t i = 0; i < Rows; ++i) {
                data_[j][i] -= rhs.data_[j][i];
            }
        }
        return *this;
    }

    constexpr mat& operator*=(T s) noexcept {
        for (auto& c : data_) {
            for (auto& v : c) {
                v *= s;
            }
        }
        return *this;
    }

    constexpr mat& operator/=(T s) noexcept {
        assert(s != T{});
        for (auto& c : data_) {
            for (auto& v : c) {
                v /= s;
            }
        }
        return *this;
    }

    [[nodiscard]] friend constexpr mat operator+(mat a, const mat& b) noexcept {
        a += b;
        return a;
    }

    [[nodiscard]] friend constexpr mat operator-(mat a, const mat& b) noexcept {
        a -= b;
        return a;
    }

    [[nodiscard]] friend constexpr mat operator*(mat a, T s) noexcept {
        a *= s;
        return a;
    }

    [[nodiscard]] friend constexpr mat operator*(T s, mat a) noexcept {
        a *= s;
        return a;
    }

    [[nodiscard]] friend constexpr mat operator/(mat a, T s) noexcept {
        a /= s;
        return a;
    }

    [[nodiscard]] constexpr mat<Cols, Rows, T> transpose() const noexcept {
        mat<Cols, Rows, T> r{};
        for (std::size_t i = 0; i < Rows; ++i) {
            for (std::size_t j = 0; j < Cols; ++j) {
                r(j, i) = (*this)(i, j);
            }
        }
        return r;
    }

    [[nodiscard]] constexpr T det() const noexcept
    requires(Rows == Cols)
    {
        return woki::math::det(*this);
    }

    [[nodiscard]] constexpr mat inverse() const noexcept
    requires((Rows == Cols) && floating_point<T>)
    {
        return woki::math::inverse(*this);
    }

    [[nodiscard]] friend constexpr bool operator==(const mat& a, const mat& b) noexcept {
        for (std::size_t j = 0; j < Cols; ++j) {
            for (std::size_t i = 0; i < Rows; ++i) {
                if constexpr (floating_point<T>) {
                    if (!approx_equal(a.data_[j][i], b.data_[j][i])) {
                        return false;
                    }
                } else {
                    if (a.data_[j][i] != b.data_[j][i]) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    [[nodiscard]] constexpr bool operator!=(const mat& other) const noexcept {
        return !(*this == other);
    }

private:
    std::array<col_type, Cols> data_{};
};

template <std::size_t R, std::size_t C, std::size_t K, arithmetic T>
[[nodiscard]] constexpr mat<R, K, T> operator*(const mat<R, C, T>& a, const mat<C, K, T>& b) noexcept {
    mat<R, K, T> r{};
    for (std::size_t i = 0; i < R; ++i) {
        for (std::size_t k = 0; k < K; ++k) {
            T acc{};
            for (std::size_t j = 0; j < C; ++j) {
                acc += a(i, j) * b(j, k);
            }
            r(i, k) = acc;
        }
    }
    return r;
}

template <std::size_t N, arithmetic T>
[[nodiscard]] constexpr T det(const mat<N, N, T>& m) noexcept {
    mat<N, N, T> a(m);
    T d = T{1};
    int sign = 1;

    for (std::size_t i = 0; i < N; ++i) {
        std::size_t pivot = i;
        T maxv = abs(a(i, i));
        for (std::size_t r = i + 1; r < N; ++r) {
            const T v = abs(a(r, i));
            if (v > maxv) {
                maxv = v;
                pivot = r;
            }
        }

        if (maxv == T{}) {
            return T{0};
        }

        if (pivot != i) {
            for (std::size_t c = 0; c < N; ++c) {
                const T tmp = a(i, c);
                a(i, c) = a(pivot, c);
                a(pivot, c) = tmp;
            }
            sign = -sign;
        }

        d *= a(i, i);
        const T piv = a(i, i);
        for (std::size_t r = i + 1; r < N; ++r) {
            const T f = a(r, i) / piv;
            for (std::size_t c = i; c < N; ++c) {
                a(r, c) -= f * a(i, c);
            }
        }
    }

    return sign == 1 ? d : -d;
}

template <std::size_t N, floating_point T>
[[nodiscard]] inline mat<N, N, T> inverse(const mat<N, N, T>& m) noexcept {
    return detail::InvertMatrix(m);
}

} // namespace woki::math
