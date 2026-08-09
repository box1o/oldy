#pragma once

#include <array>
#include <cassert>
#include <cstddef>

#include "./fwd.hpp"
#include "../common/functions.hpp"
#include "../detail/arithmetic.hpp"

namespace woki::math {

template <arithmetic T>
class vec<2, T> {
public:
    static constexpr std::size_t size = 2;
    using value_type = T;

    union {
        struct {
            T x, y;
        };

        struct {
            T r, g;
        };

        std::array<T, 2> data_;
    };

    constexpr vec() noexcept
        : data_{T{}, T{}} {}

    explicit constexpr vec(T scalar) noexcept
        : data_{scalar, scalar} {}

    constexpr vec(T x_, T y_) noexcept
        : data_{x_, y_} {}

    template <arithmetic U>
    constexpr explicit vec(const vec<2, U>& other) noexcept
        : data_{static_cast<T>(other.x), static_cast<T>(other.y)} {}

    [[nodiscard]] constexpr T* data() noexcept {
        return data_.data();
    }

    [[nodiscard]] constexpr const T* data() const noexcept {
        return data_.data();
    }

    [[nodiscard]] constexpr T& operator[](std::size_t i) noexcept {
        assert(i < 2);
        return data_[i];
    }

    [[nodiscard]] constexpr const T& operator[](std::size_t i) const noexcept {
        assert(i < 2);
        return data_[i];
    }

    constexpr vec& operator+=(const vec& o) noexcept {
        x += o.x;
        y += o.y;
        return *this;
    }

    constexpr vec& operator-=(const vec& o) noexcept {
        x -= o.x;
        y -= o.y;
        return *this;
    }

    constexpr vec& operator*=(const vec& o) noexcept {
        x *= o.x;
        y *= o.y;
        return *this;
    }

    constexpr vec& operator*=(T s) noexcept {
        x *= s;
        y *= s;
        return *this;
    }

    constexpr vec& operator/=(const vec& o) noexcept {
        assert(o.x != T{} && o.y != T{});
        x /= o.x;
        y /= o.y;
        return *this;
    }

    constexpr vec& operator/=(T s) noexcept {
        assert(s != T{});
        x /= s;
        y /= s;
        return *this;
    }

    [[nodiscard]] friend constexpr vec operator+(vec a, const vec& b) noexcept {
        a += b;
        return a;
    }

    [[nodiscard]] friend constexpr vec operator-(vec a, const vec& b) noexcept {
        a -= b;
        return a;
    }

    [[nodiscard]] friend constexpr vec operator*(vec a, const vec& b) noexcept {
        a *= b;
        return a;
    }

    [[nodiscard]] friend constexpr vec operator*(vec v, T s) noexcept {
        v *= s;
        return v;
    }

    [[nodiscard]] friend constexpr vec operator*(T s, vec v) noexcept {
        v *= s;
        return v;
    }

    [[nodiscard]] friend constexpr vec operator/(vec a, const vec& b) noexcept {
        a /= b;
        return a;
    }

    [[nodiscard]] friend constexpr vec operator/(vec v, T s) noexcept {
        v /= s;
        return v;
    }

    [[nodiscard]] constexpr vec operator-() const noexcept {
        return vec(-x, -y);
    }

    [[nodiscard]] friend constexpr bool operator==(const vec& a, const vec& b) noexcept {
        if constexpr (floating_point<T>)
            return approx_equal(a.x, b.x) && approx_equal(a.y, b.y);
        return a.x == b.x && a.y == b.y;
    }

    [[nodiscard]] constexpr bool operator!=(const vec& o) const noexcept {
        return !(*this == o);
    }

    [[nodiscard]] constexpr T dot(const vec& o) const noexcept {
        return x * o.x + y * o.y;
    }

    [[nodiscard]] constexpr T cross(const vec& o) const noexcept {
        return x * o.y - y * o.x;
    }

    [[nodiscard]] constexpr T length_squared() const noexcept {
        return dot(*this);
    }

    [[nodiscard]] T length() const noexcept {
        return sqrt(length_squared());
    }

    [[nodiscard]] vec normalized() const noexcept {
        T l = length();
        if (l == T{})
            return vec{};
        return *this / l;
    }

    constexpr vec& normalize() noexcept {
        T l = length();
        if (l != T{})
            *this /= l;
        return *this;
    }
};

} // namespace woki::math
