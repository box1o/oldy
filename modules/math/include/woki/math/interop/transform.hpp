#pragma once

#include <cmath>

#include "../mat/fwd.hpp"
#include "../vec/fwd.hpp"
#include "../mat/mat4.hpp"         // IWYU pragma: keep
#include "../vec/vec3.hpp"         // IWYU pragma: keep
#include "../common/constants.hpp" // IWYU pragma: keep
#include "../common/functions.hpp" // IWYU pragma: keep
#include "../detail/arithmetic.hpp"

namespace woki::math {

template <floating_point T>
[[nodiscard]] constexpr mat<4, 4, T> translate(const vec<3, T>& v) noexcept {
    return mat<4, 4, T>(layout::rowm, T{1}, T{0}, T{0}, v[0], T{0}, T{1}, T{0}, v[1], T{0}, T{0}, T{1}, v[2], T{0}, T{0}, T{0}, T{1});
}

template <floating_point T>
[[nodiscard]] constexpr mat<4, 4, T> scale(const vec<3, T>& v) noexcept {
    return mat<4, 4, T>(layout::rowm, v[0], T{0}, T{0}, T{0}, T{0}, v[1], T{0}, T{0}, T{0}, T{0}, v[2], T{0}, T{0}, T{0}, T{0}, T{1});
}

template <floating_point T>
[[nodiscard]] constexpr mat<4, 4, T> scale(T s) noexcept {
    return scale(vec<3, T>(s, s, s));
}

template <floating_point T>
[[nodiscard]] inline mat<4, 4, T> rotate_x(T angle) noexcept {
    const T c = std::cos(angle);
    const T s = std::sin(angle);
    return mat<4, 4, T>(layout::rowm, T{1}, T{0}, T{0}, T{0}, T{0}, c, -s, T{0}, T{0}, s, c, T{0}, T{0}, T{0}, T{0}, T{1});
}

template <floating_point T>
[[nodiscard]] inline mat<4, 4, T> rotate_y(T angle) noexcept {
    const T c = std::cos(angle);
    const T s = std::sin(angle);
    return mat<4, 4, T>(layout::rowm, c, T{0}, s, T{0}, T{0}, T{1}, T{0}, T{0}, -s, T{0}, c, T{0}, T{0}, T{0}, T{0}, T{1});
}

template <floating_point T>
[[nodiscard]] inline mat<4, 4, T> rotate_z(T angle) noexcept {
    const T c = std::cos(angle);
    const T s = std::sin(angle);
    return mat<4, 4, T>(layout::rowm, c, -s, T{0}, T{0}, s, c, T{0}, T{0}, T{0}, T{0}, T{1}, T{0}, T{0}, T{0}, T{0}, T{1});
}

template <floating_point T>
[[nodiscard]] inline mat<4, 4, T> rotate(T angle, const vec<3, T>& axis) noexcept {
    const vec<3, T> a = axis.normalized();
    const T c = std::cos(angle);
    const T s = std::sin(angle);
    const T t = T{1} - c;

    return mat<4, 4, T>(layout::rowm, t * a[0] * a[0] + c, t * a[0] * a[1] - s * a[2], t * a[0] * a[2] + s * a[1], T{0}, t * a[0] * a[1] + s * a[2], t * a[1] * a[1] + c, t * a[1] * a[2] - s * a[0], T{0},
        t * a[0] * a[2] - s * a[1], t * a[1] * a[2] + s * a[0], t * a[2] * a[2] + c, T{0}, T{0}, T{0}, T{0}, T{1});
}

template <floating_point T>
[[nodiscard]] inline mat<4, 4, T> rotate(const vec<3, T>& angles) noexcept {
    return rotate_x(angles[0]) * rotate_y(angles[1]) * rotate_z(angles[2]);
}

template <floating_point T>
[[nodiscard]] inline mat<4, 4, T> lookAt(const vec<3, T>& eye, const vec<3, T>& center, const vec<3, T>& up) noexcept {
    const vec<3, T> f = (center - eye).normalized();
    const vec<3, T> s = f.cross(up).normalized();
    const vec<3, T> u = s.cross(f);

    return mat<4, 4, T>(layout::rowm, s[0], s[1], s[2], -s.dot(eye), u[0], u[1], u[2], -u.dot(eye), -f[0], -f[1], -f[2], f.dot(eye), T{0}, T{0}, T{0}, T{1});
}

template <floating_point T>
[[nodiscard]] inline mat<4, 4, T> perspective(T fovy, T aspect, T z_near, T z_far) noexcept {
    const T half = fovy / T{2};
    const T tan_half = std::tan(half);

    return mat<4, 4, T>(layout::rowm, T{1} / (aspect * tan_half), T{0}, T{0}, T{0}, T{0}, T{1} / tan_half, T{0}, T{0}, T{0}, T{0}, -(z_far + z_near) / (z_far - z_near), -(T{2} * z_far * z_near) / (z_far - z_near), T{0},
        T{0}, -T{1}, T{0});
}

template <floating_point T>
[[nodiscard]] constexpr mat<4, 4, T> ortho(T left, T right, T bottom, T top, T z_near, T z_far) noexcept {
    return mat<4, 4, T>(layout::rowm, T{2} / (right - left), T{0}, T{0}, -(right + left) / (right - left), T{0}, T{2} / (top - bottom), T{0}, -(top + bottom) / (top - bottom), T{0}, T{0}, -T{2} / (z_far - z_near),
        -(z_far + z_near) / (z_far - z_near), T{0}, T{0}, T{0}, T{1});
}

} // namespace woki::math
