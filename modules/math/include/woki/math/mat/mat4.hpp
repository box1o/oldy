#pragma once

#include <array>
#include <cassert>
#include <cstddef>

#include "fwd.hpp"
#include "../common/functions.hpp"
#include "../detail/arithmetic.hpp"

namespace woki::math {

template <arithmetic T>
class mat<4, 4, T> {
public:
    static constexpr std::size_t rows = 4;
    static constexpr std::size_t cols = 4;
    using value_type = T;
    using col_type = std::array<T, 4>;

    union {
        struct {
            T m00, m10, m20, m30;
            T m01, m11, m21, m31;
            T m02, m12, m22, m32;
            T m03, m13, m23, m33;
        };

        std::array<col_type, 4> data_;
    };

    constexpr mat() noexcept
        : data_{col_type{T{}, T{}, T{}, T{}}, col_type{T{}, T{}, T{}, T{}}, col_type{T{}, T{}, T{}, T{}}, col_type{T{}, T{}, T{}, T{}}} {}

    explicit constexpr mat(T value) noexcept
        : data_{col_type{value, value, value, value}, col_type{value, value, value, value}, col_type{value, value, value, value}, col_type{value, value, value, value}} {}

    constexpr mat(layout order, T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7, T v8, T v9, T v10, T v11, T v12, T v13, T v14, T v15) noexcept {
        if (order == layout::rowm) {
            data_[0] = {v0, v4, v8, v12};
            data_[1] = {v1, v5, v9, v13};
            data_[2] = {v2, v6, v10, v14};
            data_[3] = {v3, v7, v11, v15};
        } else {
            data_[0] = {v0, v1, v2, v3};
            data_[1] = {v4, v5, v6, v7};
            data_[2] = {v8, v9, v10, v11};
            data_[3] = {v12, v13, v14, v15};
        }
    }

    template <arithmetic U>
    constexpr explicit mat(const mat<4, 4, U>& other) noexcept {
        for (std::size_t j = 0; j < 4; ++j) {
            for (std::size_t i = 0; i < 4; ++i) {
                data_[j][i] = static_cast<T>(other(i, j));
            }
        }
    }

    [[nodiscard]] static constexpr mat identity() noexcept {
        return mat(layout::rowm, T{1}, T{0}, T{0}, T{0}, T{0}, T{1}, T{0}, T{0}, T{0}, T{0}, T{1}, T{0}, T{0}, T{0}, T{0}, T{1});
    }

    [[nodiscard]] constexpr T& operator()(std::size_t r, std::size_t c) noexcept {
        assert(r < 4 && c < 4);
        return data_[c][r];
    }

    [[nodiscard]] constexpr const T& operator()(std::size_t r, std::size_t c) const noexcept {
        assert(r < 4 && c < 4);
        return data_[c][r];
    }

    [[nodiscard]] constexpr col_type& operator[](std::size_t col) noexcept {
        assert(col < 4);
        return data_[col];
    }

    [[nodiscard]] constexpr const col_type& operator[](std::size_t col) const noexcept {
        assert(col < 4);
        return data_[col];
    }

    [[nodiscard]] constexpr col_type& col(std::size_t j) noexcept {
        assert(j < 4);
        return data_[j];
    }

    [[nodiscard]] constexpr const col_type& col(std::size_t j) const noexcept {
        assert(j < 4);
        return data_[j];
    }

    constexpr void set_col(std::size_t j, const col_type& c) noexcept {
        assert(j < 4);
        data_[j] = c;
    }

    [[nodiscard]] constexpr std::array<T, 4> row(std::size_t i) const noexcept {
        assert(i < 4);
        return {data_[0][i], data_[1][i], data_[2][i], data_[3][i]};
    }

    constexpr void set_row(std::size_t i, const std::array<T, 4>& r) noexcept {
        assert(i < 4);
        data_[0][i] = r[0];
        data_[1][i] = r[1];
        data_[2][i] = r[2];
        data_[3][i] = r[3];
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
        m03 += rhs.m03;
        m10 += rhs.m10;
        m11 += rhs.m11;
        m12 += rhs.m12;
        m13 += rhs.m13;
        m20 += rhs.m20;
        m21 += rhs.m21;
        m22 += rhs.m22;
        m23 += rhs.m23;
        m30 += rhs.m30;
        m31 += rhs.m31;
        m32 += rhs.m32;
        m33 += rhs.m33;
        return *this;
    }

    constexpr mat& operator-=(const mat& rhs) noexcept {
        m00 -= rhs.m00;
        m01 -= rhs.m01;
        m02 -= rhs.m02;
        m03 -= rhs.m03;
        m10 -= rhs.m10;
        m11 -= rhs.m11;
        m12 -= rhs.m12;
        m13 -= rhs.m13;
        m20 -= rhs.m20;
        m21 -= rhs.m21;
        m22 -= rhs.m22;
        m23 -= rhs.m23;
        m30 -= rhs.m30;
        m31 -= rhs.m31;
        m32 -= rhs.m32;
        m33 -= rhs.m33;
        return *this;
    }

    constexpr mat& operator*=(T s) noexcept {
        m00 *= s;
        m01 *= s;
        m02 *= s;
        m03 *= s;
        m10 *= s;
        m11 *= s;
        m12 *= s;
        m13 *= s;
        m20 *= s;
        m21 *= s;
        m22 *= s;
        m23 *= s;
        m30 *= s;
        m31 *= s;
        m32 *= s;
        m33 *= s;
        return *this;
    }

    constexpr mat& operator/=(T s) noexcept {
        assert(s != T{});
        m00 /= s;
        m01 /= s;
        m02 /= s;
        m03 /= s;
        m10 /= s;
        m11 /= s;
        m12 /= s;
        m13 /= s;
        m20 /= s;
        m21 /= s;
        m22 /= s;
        m23 /= s;
        m30 /= s;
        m31 /= s;
        m32 /= s;
        m33 /= s;
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

    // NOTE: mat4 uses the generic operator* in base.hpp for mat*mat.

    [[nodiscard]] constexpr mat transpose() const noexcept {
        return mat(layout::rowm, m00, m10, m20, m30, m01, m11, m21, m31, m02, m12, m22, m32, m03, m13, m23, m33);
    }

    [[nodiscard]] friend constexpr bool operator==(const mat& a, const mat& b) noexcept {
        if constexpr (floating_point<T>) {
            return approx_equal(a.m00, b.m00) && approx_equal(a.m01, b.m01) && approx_equal(a.m02, b.m02) && approx_equal(a.m03, b.m03) && approx_equal(a.m10, b.m10) && approx_equal(a.m11, b.m11)
                   && approx_equal(a.m12, b.m12) && approx_equal(a.m13, b.m13) && approx_equal(a.m20, b.m20) && approx_equal(a.m21, b.m21) && approx_equal(a.m22, b.m22) && approx_equal(a.m23, b.m23)
                   && approx_equal(a.m30, b.m30) && approx_equal(a.m31, b.m31) && approx_equal(a.m32, b.m32) && approx_equal(a.m33, b.m33);
        }
        return a.m00 == b.m00 && a.m01 == b.m01 && a.m02 == b.m02 && a.m03 == b.m03 && a.m10 == b.m10 && a.m11 == b.m11 && a.m12 == b.m12 && a.m13 == b.m13 && a.m20 == b.m20 && a.m21 == b.m21 && a.m22 == b.m22
               && a.m23 == b.m23 && a.m30 == b.m30 && a.m31 == b.m31 && a.m32 == b.m32 && a.m33 == b.m33;
    }

    [[nodiscard]] constexpr bool operator!=(const mat& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] constexpr T det() const noexcept {
        const T a0 = m00 * m11 - m01 * m10;
        const T a1 = m00 * m12 - m02 * m10;
        const T a2 = m00 * m13 - m03 * m10;
        const T a3 = m01 * m12 - m02 * m11;
        const T a4 = m01 * m13 - m03 * m11;
        const T a5 = m02 * m13 - m03 * m12;
        const T b0 = m20 * m31 - m21 * m30;
        const T b1 = m20 * m32 - m22 * m30;
        const T b2 = m20 * m33 - m23 * m30;
        const T b3 = m21 * m32 - m22 * m31;
        const T b4 = m21 * m33 - m23 * m31;
        const T b5 = m22 * m33 - m23 * m32;

        return a0 * b5 - a1 * b4 + a2 * b3 + a3 * b2 - a4 * b1 + a5 * b0;
    }

    [[nodiscard]] mat inverse() const noexcept
    requires floating_point<T>
    {
        return detail::InvertMatrix(*this);
    }
};

} // namespace woki::math
