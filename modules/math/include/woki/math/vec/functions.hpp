#pragma once

#include <cmath>

#include "./fwd.hpp"
#include "./base.hpp"
#include "./vec2.hpp"
#include "./vec3.hpp"
#include "./vec4.hpp"
#include "../common/constants.hpp"
#include "../common/functions.hpp"

namespace woki::math {

template <std::size_t N, arithmetic T>
[[nodiscard]] constexpr vec<N, T> min(const vec<N, T>& a, const vec<N, T>& b) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = min(a[i], b[i]);
    return result;
}

template <std::size_t N, arithmetic T>
[[nodiscard]] constexpr vec<N, T> min(const vec<N, T>& a, T b) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = min(a[i], b);
    return result;
}

template <std::size_t N, arithmetic T>
[[nodiscard]] constexpr vec<N, T> max(const vec<N, T>& a, const vec<N, T>& b) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = max(a[i], b[i]);
    return result;
}

template <std::size_t N, arithmetic T>
[[nodiscard]] constexpr vec<N, T> max(const vec<N, T>& a, T b) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = max(a[i], b);
    return result;
}

template <std::size_t N, arithmetic T>
[[nodiscard]] constexpr vec<N, T> abs(const vec<N, T>& v) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = abs(v[i]);
    return result;
}

template <std::size_t N, arithmetic T>
[[nodiscard]] constexpr vec<N, T> clamp(const vec<N, T>& v, const vec<N, T>& lo, const vec<N, T>& hi) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = clamp(v[i], lo[i], hi[i]);
    return result;
}

template <std::size_t N, arithmetic T>
[[nodiscard]] constexpr vec<N, T> clamp(const vec<N, T>& v, T lo, T hi) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = clamp(v[i], lo, hi);
    return result;
}

template <std::size_t N, arithmetic T>
[[nodiscard]] constexpr vec<N, T> saturate(const vec<N, T>& v) noexcept {
    return clamp(v, T{0}, T{1});
}

template <std::size_t N, arithmetic T>
[[nodiscard]] constexpr vec<N, T> lerp(const vec<N, T>& a, const vec<N, T>& b, T t) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = lerp(a[i], b[i], t);
    return result;
}

template <std::size_t N, arithmetic T>
[[nodiscard]] constexpr T length_squared(const vec<N, T>& v) noexcept {
    return v.length_squared();
}

template <std::size_t N, arithmetic T>
[[nodiscard]] T length(const vec<N, T>& v) noexcept {
    return v.length();
}

template <std::size_t N, arithmetic T>
[[nodiscard]] vec<N, T> normalize(const vec<N, T>& v) noexcept {
    return v.normalized();
}

template <std::size_t N, arithmetic T>
[[nodiscard]] constexpr T dot(const vec<N, T>& a, const vec<N, T>& b) noexcept {
    return a.dot(b);
}

template <arithmetic T>
[[nodiscard]] constexpr vec<3, T> cross(const vec<3, T>& a, const vec<3, T>& b) noexcept {
    return a.cross(b);
}

template <arithmetic T>
[[nodiscard]] constexpr T cross(const vec<2, T>& a, const vec<2, T>& b) noexcept {
    return a.cross(b);
}

template <std::size_t N, floating_point T>
[[nodiscard]] constexpr vec<N, T> reflect(const vec<N, T>& v, const vec<N, T>& n) noexcept {
    return v - n * (T{2} * dot(v, n));
}

template <std::size_t N, floating_point T>
[[nodiscard]] constexpr vec<N, T> refract(const vec<N, T>& incident, const vec<N, T>& normal, T eta) noexcept {
    T d = dot(incident, normal);
    T k = T{1} - eta * eta * (T{1} - d * d);
    if (k < T{})
        return vec<N, T>{};
    return incident * eta - normal * (eta * d + std::sqrt(k));
}

template <std::size_t N, floating_point T>
[[nodiscard]] T distance(const vec<N, T>& a, const vec<N, T>& b) noexcept {
    return length(b - a);
}

template <std::size_t N, floating_point T>
[[nodiscard]] constexpr T distance_squared(const vec<N, T>& a, const vec<N, T>& b) noexcept {
    return length_squared(b - a);
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> smoothstep(const vec<N, T>& edge0, const vec<N, T>& edge1, const vec<N, T>& x) noexcept {
    vec<N, T> t = clamp((x - edge0) / (edge1 - edge0), T{0}, T{1});
    return t * t * (vec<N, T>(T{3}) - t * T{2});
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> smoothstep(T edge0, T edge1, const vec<N, T>& x) noexcept {
    vec<N, T> t = clamp((x - edge0) / (edge1 - edge0), T{0}, T{1});
    return t * t * (vec<N, T>(T{3}) - t * T{2});
}

// Component-wise math functions
template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> sqrt(const vec<N, T>& v) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = std::sqrt(v[i]);
    return result;
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> sin(const vec<N, T>& v) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = std::sin(v[i]);
    return result;
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> cos(const vec<N, T>& v) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = std::cos(v[i]);
    return result;
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> tan(const vec<N, T>& v) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = std::tan(v[i]);
    return result;
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> asin(const vec<N, T>& v) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = std::asin(v[i]);
    return result;
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> acos(const vec<N, T>& v) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = std::acos(v[i]);
    return result;
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> atan(const vec<N, T>& v) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = std::atan(v[i]);
    return result;
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> pow(const vec<N, T>& base, const vec<N, T>& exp) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = std::pow(base[i], exp[i]);
    return result;
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> pow(const vec<N, T>& base, T exp) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = std::pow(base[i], exp);
    return result;
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> exp(const vec<N, T>& v) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = std::exp(v[i]);
    return result;
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> log(const vec<N, T>& v) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = std::log(v[i]);
    return result;
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> floor(const vec<N, T>& v) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = std::floor(v[i]);
    return result;
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> ceil(const vec<N, T>& v) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = std::ceil(v[i]);
    return result;
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> round(const vec<N, T>& v) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = std::round(v[i]);
    return result;
}

template <std::size_t N, floating_point T>
[[nodiscard]] vec<N, T> trunc(const vec<N, T>& v) noexcept {
    vec<N, T> result;
    for (std::size_t i = 0; i < N; ++i)
        result[i] = std::trunc(v[i]);
    return result;
}

} // namespace woki::math
