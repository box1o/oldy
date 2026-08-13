#pragma once

#include <array>
#include <cassert>
#include <cstddef>

#include "./fwd.hpp"
#include "../common/functions.hpp"
#include "../detail/arithmetic.hpp"

namespace woki::math {

template <std::size_t N, arithmetic T>
requires(N >= 2)
class vec {
public:
    static constexpr std::size_t size = N;
    using value_type = T;

    constexpr vec() noexcept = default;

    explicit constexpr vec(T scalar) noexcept {
        data_.fill(scalar);
    }

    template <arithmetic... Args>
    requires(sizeof...(Args) == N)
    constexpr vec(Args... args) noexcept
        : data_{static_cast<T>(args)...} {}

    template <arithmetic U>
    constexpr explicit vec(const vec<N, U>& other) noexcept {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] = static_cast<T>(other[i]);
        }
    }

    [[nodiscard]] constexpr T& operator[](std::size_t i) noexcept {
        assert(i < N);
        return data_[i];
    }

    [[nodiscard]] constexpr const T& operator[](std::size_t i) const noexcept {
        assert(i < N);
        return data_[i];
    }

    [[nodiscard]] constexpr T* data() noexcept {
        return data_.data();
    }

    [[nodiscard]] constexpr const T* data() const noexcept {
        return data_.data();
    }

    constexpr vec& operator+=(const vec& other) noexcept {
        for (std::size_t i = 0; i < N; ++i)
            data_[i] += other.data_[i];
        return *this;
    }

    constexpr vec& operator-=(const vec& other) noexcept {
        for (std::size_t i = 0; i < N; ++i)
            data_[i] -= other.data_[i];
        return *this;
    }

    constexpr vec& operator*=(const vec& other) noexcept {
        for (std::size_t i = 0; i < N; ++i)
            data_[i] *= other.data_[i];
        return *this;
    }

    constexpr vec& operator*=(T scalar) noexcept {
        for (std::size_t i = 0; i < N; ++i)
            data_[i] *= scalar;
        return *this;
    }

    constexpr vec& operator/=(const vec& other) noexcept {
        for (std::size_t i = 0; i < N; ++i) {
            assert(other.data_[i] != T{});
            data_[i] /= other.data_[i];
        }
        return *this;
    }

    constexpr vec& operator/=(T scalar) noexcept {
        assert(scalar != T{});
        for (std::size_t i = 0; i < N; ++i)
            data_[i] /= scalar;
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
        vec result{};
        for (std::size_t i = 0; i < N; ++i)
            result.data_[i] = -data_[i];
        return result;
    }

    [[nodiscard]] friend constexpr bool operator==(const vec& a, const vec& b) noexcept {
        for (std::size_t i = 0; i < N; ++i) {
            if constexpr (floating_point<T>) {
                if (!approx_equal(a.data_[i], b.data_[i]))
                    return false;
            } else {
                if (a.data_[i] != b.data_[i])
                    return false;
            }
        }
        return true;
    }

    [[nodiscard]] constexpr bool operator!=(const vec& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] constexpr T dot(const vec& other) const noexcept {
        T result{};
        for (std::size_t i = 0; i < N; ++i)
            result += data_[i] * other.data_[i];
        return result;
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

private:
    std::array<T, N> data_{};
};

} // namespace woki::math
