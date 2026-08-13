#pragma once

#include <array>
#include <cmath>

#include "../mat/mat4.hpp"
#include "../vec/vec3.hpp"
#include "../common/constants.hpp"
#include "../common/functions.hpp"
#include "../detail/arithmetic.hpp"

namespace woki::math {

template <floating_point T>
class quat {
public:
    using value_type = T;

    T x = T{};
    T y = T{};
    T z = T{};
    T w = T{};

    constexpr quat() noexcept = default;

    constexpr quat(T x_, T y_, T z_, T w_) noexcept
        : x(x_),
          y(y_),
          z(z_),
          w(w_) {}

    // Axis-angle constructor
    quat(const vec<3, T>& axis, T angle) noexcept {
        const T half = angle / T{2};
        const T s = std::sin(half);
        const T c = std::cos(half);
        const vec<3, T> a = axis.normalized();
        x = a.x * s;
        y = a.y * s;
        z = a.z * s;
        w = c;
    }

    // Euler angles (pitch, yaw, roll) in radians
    quat(const vec<3, T>& euler) noexcept {
        const T cy = std::cos(euler.z * T{0.5});
        const T sy = std::sin(euler.z * T{0.5});
        const T cp = std::cos(euler.y * T{0.5});
        const T sp = std::sin(euler.y * T{0.5});
        const T cr = std::cos(euler.x * T{0.5});
        const T sr = std::sin(euler.x * T{0.5});

        w = cr * cp * cy + sr * sp * sy;
        x = sr * cp * cy - cr * sp * sy;
        y = cr * sp * cy + sr * cp * sy;
        z = cr * cp * sy - sr * sp * cy;
    }

    [[nodiscard]] constexpr T length_squared() const noexcept {
        return x * x + y * y + z * z + w * w;
    }

    [[nodiscard]] T length() const noexcept {
        return std::sqrt(length_squared());
    }

    [[nodiscard]] quat normalized() const noexcept {
        T l = length();
        if (l == T{})
            return quat{};
        return *this / l;
    }

    constexpr quat& normalize() noexcept {
        T l = length();
        if (l != T{})
            *this /= l;
        return *this;
    }

    [[nodiscard]] constexpr quat conjugate() const noexcept {
        return quat(-x, -y, -z, w);
    }

    [[nodiscard]] quat inverse() const noexcept {
        T len_sq = length_squared();
        if (len_sq == T{})
            return quat{};
        return conjugate() / len_sq;
    }

    [[nodiscard]] constexpr vec<3, T> axis() const noexcept {
        const T s = std::sqrt(T{1} - w * w);
        if (s <= epsilon<T>) {
            return vec<3, T>(T{0}, T{0}, T{1});
        }
        return vec<3, T>(x / s, y / s, z / s);
    }

    [[nodiscard]] T angle() const noexcept {
        return T{2} * std::acos(clamp(w, T{-1}, T{1}));
    }

    // Hamilton product
    [[nodiscard]] friend constexpr quat operator*(const quat& a, const quat& b) noexcept {
        return quat(a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z);
    }

    constexpr quat& operator*=(const quat& rhs) noexcept {
        *this = *this * rhs;
        return *this;
    }

    [[nodiscard]] friend constexpr quat operator+(quat a, const quat& b) noexcept {
        a.x += b.x;
        a.y += b.y;
        a.z += b.z;
        a.w += b.w;
        return a;
    }

    [[nodiscard]] friend constexpr quat operator-(quat a, const quat& b) noexcept {
        a.x -= b.x;
        a.y -= b.y;
        a.z -= b.z;
        a.w -= b.w;
        return a;
    }

    [[nodiscard]] friend constexpr quat operator*(quat q, T s) noexcept {
        q.x *= s;
        q.y *= s;
        q.z *= s;
        q.w *= s;
        return q;
    }

    [[nodiscard]] friend constexpr quat operator*(T s, quat q) noexcept {
        return q * s;
    }

    constexpr quat& operator/=(T s) noexcept {
        x /= s;
        y /= s;
        z /= s;
        w /= s;
        return *this;
    }

    [[nodiscard]] friend constexpr quat operator/(quat q, T s) noexcept {
        q /= s;
        return q;
    }

    [[nodiscard]] constexpr quat operator-() const noexcept {
        return quat(-x, -y, -z, -w);
    }

    [[nodiscard]] friend constexpr bool operator==(const quat& a, const quat& b) noexcept {
        return approx_equal(a.x, b.x) && approx_equal(a.y, b.y) && approx_equal(a.z, b.z) && approx_equal(a.w, b.w);
    }

    [[nodiscard]] constexpr bool operator!=(const quat& other) const noexcept {
        return !(*this == other);
    }

    // Rotate a vector by this quaternion
    [[nodiscard]] vec<3, T> rotate(const vec<3, T>& v) const noexcept {
        quat p(v.x, v.y, v.z, T{});
        quat result = (*this) * p * inverse();
        return vec<3, T>(result.x, result.y, result.z);
    }

    [[nodiscard]] mat<4, 4, T> toMat4() const noexcept {
        const T xx = x * x;
        const T yy = y * y;
        const T zz = z * z;
        const T xy = x * y;
        const T xz = x * z;
        const T yz = y * z;
        const T wx = w * x;
        const T wy = w * y;
        const T wz = w * z;

        return mat<4, 4, T>(layout::rowm, T{1} - T{2} * (yy + zz), T{2} * (xy - wz), T{2} * (xz + wy), T{0}, T{2} * (xy + wz), T{1} - T{2} * (xx + zz), T{2} * (yz - wx), T{0}, T{2} * (xz - wy), T{2} * (yz + wx),
            T{1} - T{2} * (xx + yy), T{0}, T{0}, T{0}, T{0}, T{1});
    }

    static quat fromMat4(const mat<4, 4, T>& m) noexcept {
        T trace = m(0, 0) + m(1, 1) + m(2, 2);
        quat result;
        if (trace > T{}) {
            T s = T{0.5} / std::sqrt(trace + T{1});
            result.w = T{0.25} / s;
            result.x = (m(2, 1) - m(1, 2)) * s;
            result.y = (m(0, 2) - m(2, 0)) * s;
            result.z = (m(1, 0) - m(0, 1)) * s;
        } else if (m(0, 0) > m(1, 1) && m(0, 0) > m(2, 2)) {
            T s = T{2} * std::sqrt(T{1} + m(0, 0) - m(1, 1) - m(2, 2));
            result.w = (m(2, 1) - m(1, 2)) / s;
            result.x = T{0.25} * s;
            result.y = (m(0, 1) + m(1, 0)) / s;
            result.z = (m(0, 2) + m(2, 0)) / s;
        } else if (m(1, 1) > m(2, 2)) {
            T s = T{2} * std::sqrt(T{1} + m(1, 1) - m(0, 0) - m(2, 2));
            result.w = (m(0, 2) - m(2, 0)) / s;
            result.x = (m(0, 1) + m(1, 0)) / s;
            result.y = T{0.25} * s;
            result.z = (m(1, 2) + m(2, 1)) / s;
        } else {
            T s = T{2} * std::sqrt(T{1} + m(2, 2) - m(0, 0) - m(1, 1));
            result.w = (m(1, 0) - m(0, 1)) / s;
            result.x = (m(0, 2) + m(2, 0)) / s;
            result.y = (m(1, 2) + m(2, 1)) / s;
            result.z = T{0.25} * s;
        }
        return result.normalized();
    }
};

template <floating_point T>
[[nodiscard]] inline quat<T> slerp(const quat<T>& a, const quat<T>& b, T t) noexcept {
    quat<T> b_ = b;
    T dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

    if (dot < T{}) {
        dot = -dot;
        b_ = -b;
    }

    if (dot > T{0.9995f}) {
        quat<T> result = a + (b_ - a) * t;
        return result.normalized();
    }

    T theta_0 = std::acos(dot);
    T theta = theta_0 * t;
    T sin_theta = std::sin(theta);
    T sin_theta_0 = std::sin(theta_0);

    T s0 = std::cos(theta) - dot * sin_theta / sin_theta_0;
    T s1 = sin_theta / sin_theta_0;

    return a * s0 + b_ * s1;
}

} // namespace woki::math
