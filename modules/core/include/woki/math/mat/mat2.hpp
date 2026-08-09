#pragma once

#include <array>
#include <cassert>
#include <cstddef>

#include "fwd.hpp"
#include "../common/functions.hpp"
#include "../detail/arithmetic.hpp"

namespace woki::math {

template <arithmetic T>
class mat<2, 2, T> {
public:
    static constexpr std::size_t rows = 2;
    static constexpr std::size_t cols = 2;
    using value_type = T;
    using col_type = std::array<T, 2>;

    union {
        struct {
            T m00, m10;
            T m01, m11;
        };

        std::array<col_type, 2> data_;
    };

    constexpr mat() noexcept
        : data_{col_type{T{}, T{}}, col_type{T{}, T{}}} {}

    explicit constexpr mat(T value) noexcept
        : data_{col_type{value, value}, col_type{value, value}} {}

    constexpr mat(layout order, T v0, T v1, T v2, T v3) noexcept {
        if (order == layout::rowm) {
            data_[0] = {v0, v2};
            data_[1] = {v1, v3};
        } else {
            data_[0] = {v0, v1};
            data_[1] = {v2, v3};
        }
    }

    template <arithmetic U>
    constexpr explicit mat(const mat<2, 2, U>& other) noexcept {
        for (std::size_t j = 0; j < 2; ++j) {
            for (std::size_t i = 0; i < 2; ++i) {
                data_[j][i] = static_cast<T>(other(i, j));
            }
        }
    }

    [[nodiscard]] static constexpr mat identity() noexcept {
        return mat(layout::rowm, T{1}, T{0}, T{0}, T{1});
    }

    [[nodiscard]] constexpr T& operator()(std::size_t r, std::size_t c) noexcept {
        assert(r < 2 && c < 2);
        return data_[c][r];
    }

    [[nodiscard]] constexpr const T& operator()(std::size_t r, std::size_t c) const noexcept {
        assert(r < 2 && c < 2);
        return data_[c][r];
    }

    [[nodiscard]] constexpr col_type& operator[](std::size_t col) noexcept {
        assert(col < 2);
        return data_[col];
    }

    [[nodiscard]] constexpr const col_type& operator[](std::size_t col) const noexcept {
        assert(col < 2);
        return data_[col];
    }

    [[nodiscard]] constexpr col_type& col(std::size_t j) noexcept {
        assert(j < 2);
        return data_[j];
    }

    [[nodiscard]] constexpr const col_type& col(std::size_t j) const noexcept {
        assert(j < 2);
        return data_[j];
    }

    constexpr void set_col(std::size_t j, const col_type& c) noexcept {
        assert(j < 2);
        data_[j] = c;
    }

    [[nodiscard]] constexpr std::array<T, 2> row(std::size_t i) const noexcept {
        assert(i < 2);
        return {data_[0][i], data_[1][i]};
    }

    constexpr void set_row(std::size_t i, const std::array<T, 2>& r) noexcept {
        assert(i < 2);
        data_[0][i] = r[0];
        data_[1][i] = r[1];
    }

    [[nodiscard]] constexpr T* data() noexcept {
        return &data_[0][0];
    }

    [[nodiscard]] constexpr const T* data() const noexcept {
        return &data_[0][0];
    }

    constexpr mat& operator+=(const mat& rhs) noexcept {
        m00 += rhs.m00;
        m01 += rhs.m01;
        m10 += rhs.m10;
        m11 += rhs.m11;
        return *this;
    }

    constexpr mat& operator-=(const mat& rhs) noexcept {
        m00 -= rhs.m00;
        m01 -= rhs.m01;
        m10 -= rhs.m10;
        m11 -= rhs.m11;
        return *this;
    }

    constexpr mat& operator*=(T s) noexcept {
        m00 *= s;
        m01 *= s;
        m10 *= s;
        m11 *= s;
        return *this;
    }

    constexpr mat& operator/=(T s) noexcept {
        assert(s != T{});
        m00 /= s;
        m01 /= s;
        m10 /= s;
        m11 /= s;
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

    // NOTE: mat2 uses the generic operator* in base.hpp for mat*mat.

    [[nodiscard]] constexpr mat transpose() const noexcept {
        return mat(layout::rowm, m00, m01, m10, m11);
    }

    [[nodiscard]] friend constexpr bool operator==(const mat& a, const mat& b) noexcept {
        if constexpr (floating_point<T>) {
            return approx_equal(a.m00, b.m00) && approx_equal(a.m01, b.m01) && approx_equal(a.m10, b.m10) && approx_equal(a.m11, b.m11);
        }
        return a.m00 == b.m00 && a.m01 == b.m01 && a.m10 == b.m10 && a.m11 == b.m11;
    }

    [[nodiscard]] constexpr bool operator!=(const mat& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] constexpr T det() const noexcept {
        return m00 * m11 - m01 * m10;
    }

    [[nodiscard]] mat inverse() const noexcept
    requires floating_point<T>
    {
        const T d = det();
        if (abs(d) <= epsilon<T>) {
            return identity();
        }
        const T inv_det = T{1} / d;
        return mat(layout::rowm, m11 * inv_det, -m01 * inv_det, -m10 * inv_det, m00 * inv_det);
    }
};

} // namespace woki::math
