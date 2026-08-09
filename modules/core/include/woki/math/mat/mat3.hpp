#pragma once

#include <array>
#include <cassert>
#include <cstddef>

#include "fwd.hpp"
#include "../common/functions.hpp"
#include "../detail/arithmetic.hpp"

namespace woki::math {

template <arithmetic T>
class mat<3, 3, T> {
public:
    static constexpr std::size_t rows = 3;
    static constexpr std::size_t cols = 3;
    using value_type = T;
    using col_type = std::array<T, 3>;

    union {
        struct {
            T m00, m10, m20;
            T m01, m11, m21;
            T m02, m12, m22;
        };

        std::array<col_type, 3> data_;
    };

    constexpr mat() noexcept
        : data_{col_type{T{}, T{}, T{}}, col_type{T{}, T{}, T{}}, col_type{T{}, T{}, T{}}} {}

    explicit constexpr mat(T value) noexcept
        : data_{col_type{value, value, value}, col_type{value, value, value}, col_type{value, value, value}} {}

    constexpr mat(layout order, T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7, T v8) noexcept {
        if (order == layout::rowm) {
            data_[0] = {v0, v3, v6};
            data_[1] = {v1, v4, v7};
            data_[2] = {v2, v5, v8};
        } else {
            data_[0] = {v0, v1, v2};
            data_[1] = {v3, v4, v5};
            data_[2] = {v6, v7, v8};
        }
    }

    template <arithmetic U>
    constexpr explicit mat(const mat<3, 3, U>& other) noexcept {
        for (std::size_t j = 0; j < 3; ++j) {
            for (std::size_t i = 0; i < 3; ++i) {
                data_[j][i] = static_cast<T>(other(i, j));
            }
        }
    }

    [[nodiscard]] static constexpr mat identity() noexcept {
        return mat(layout::rowm, T{1}, T{0}, T{0}, T{0}, T{1}, T{0}, T{0}, T{0}, T{1});
    }

    [[nodiscard]] constexpr T& operator()(std::size_t r, std::size_t c) noexcept {
        assert(r < 3 && c < 3);
        return data_[c][r];
    }

    [[nodiscard]] constexpr const T& operator()(std::size_t r, std::size_t c) const noexcept {
        assert(r < 3 && c < 3);
        return data_[c][r];
    }

    [[nodiscard]] constexpr col_type& operator[](std::size_t col) noexcept {
        assert(col < 3);
        return data_[col];
    }

    [[nodiscard]] constexpr const col_type& operator[](std::size_t col) const noexcept {
        assert(col < 3);
        return data_[col];
    }

    [[nodiscard]] constexpr col_type& col(std::size_t j) noexcept {
        assert(j < 3);
        return data_[j];
    }

    [[nodiscard]] constexpr const col_type& col(std::size_t j) const noexcept {
        assert(j < 3);
        return data_[j];
    }

    constexpr void set_col(std::size_t j, const col_type& c) noexcept {
        assert(j < 3);
        data_[j] = c;
    }

    [[nodiscard]] constexpr std::array<T, 3> row(std::size_t i) const noexcept {
        assert(i < 3);
        return {data_[0][i], data_[1][i], data_[2][i]};
    }

    constexpr void set_row(std::size_t i, const std::array<T, 3>& r) noexcept {
        assert(i < 3);
        data_[0][i] = r[0];
        data_[1][i] = r[1];
        data_[2][i] = r[2];
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
        m02 += rhs.m02;
        m10 += rhs.m10;
        m11 += rhs.m11;
        m12 += rhs.m12;
        m20 += rhs.m20;
        m21 += rhs.m21;
        m22 += rhs.m22;
        return *this;
    }

    constexpr mat& operator-=(const mat& rhs) noexcept {
        m00 -= rhs.m00;
        m01 -= rhs.m01;
        m02 -= rhs.m02;
        m10 -= rhs.m10;
        m11 -= rhs.m11;
        m12 -= rhs.m12;
        m20 -= rhs.m20;
        m21 -= rhs.m21;
        m22 -= rhs.m22;
        return *this;
    }

    constexpr mat& operator*=(T s) noexcept {
        m00 *= s;
        m01 *= s;
        m02 *= s;
        m10 *= s;
        m11 *= s;
        m12 *= s;
        m20 *= s;
        m21 *= s;
        m22 *= s;
        return *this;
    }

    constexpr mat& operator/=(T s) noexcept {
        assert(s != T{});
        m00 /= s;
        m01 /= s;
        m02 /= s;
        m10 /= s;
        m11 /= s;
        m12 /= s;
        m20 /= s;
        m21 /= s;
        m22 /= s;
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

    // NOTE: mat3 uses the generic operator* in base.hpp for mat*mat.

    [[nodiscard]] constexpr mat transpose() const noexcept {
        return mat(layout::rowm, m00, m10, m20, m01, m11, m21, m02, m12, m22);
    }

    [[nodiscard]] friend constexpr bool operator==(const mat& a, const mat& b) noexcept {
        if constexpr (floating_point<T>) {
            return approx_equal(a.m00, b.m00) && approx_equal(a.m01, b.m01) && approx_equal(a.m02, b.m02) && approx_equal(a.m10, b.m10) && approx_equal(a.m11, b.m11) && approx_equal(a.m12, b.m12)
                   && approx_equal(a.m20, b.m20) && approx_equal(a.m21, b.m21) && approx_equal(a.m22, b.m22);
        }
        return a.m00 == b.m00 && a.m01 == b.m01 && a.m02 == b.m02 && a.m10 == b.m10 && a.m11 == b.m11 && a.m12 == b.m12 && a.m20 == b.m20 && a.m21 == b.m21 && a.m22 == b.m22;
    }

    [[nodiscard]] constexpr bool operator!=(const mat& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] constexpr T det() const noexcept {
        return m00 * (m11 * m22 - m12 * m21) - m01 * (m10 * m22 - m12 * m20) + m02 * (m10 * m21 - m11 * m20);
    }

    [[nodiscard]] mat inverse() const noexcept
    requires floating_point<T>
    {
        const T d = det();
        if (abs(d) <= epsilon<T>) {
            return identity();
        }

        const T inv_det = T{1} / d;
        return mat(layout::rowm, (m11 * m22 - m12 * m21) * inv_det, (m02 * m21 - m01 * m22) * inv_det, (m01 * m12 - m02 * m11) * inv_det,

            (m12 * m20 - m10 * m22) * inv_det, (m00 * m22 - m02 * m20) * inv_det, (m02 * m10 - m00 * m12) * inv_det,

            (m10 * m21 - m11 * m20) * inv_det, (m01 * m20 - m00 * m21) * inv_det, (m00 * m11 - m01 * m10) * inv_det);
    }
};

} // namespace woki::math
