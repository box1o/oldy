#pragma once

#include <format>
#include <cstddef>
#include <ostream>

#include "../mat/mat2.hpp"
#include "../mat/mat3.hpp"
#include "../mat/mat4.hpp"
#include "../vec/vec2.hpp"
#include "../vec/vec3.hpp"
#include "../vec/vec4.hpp"
#include "../quat/quat.hpp"

namespace woki::math {

// ---------------------------------------------------------------------------
// ostream operators
// ---------------------------------------------------------------------------

template <std::size_t N, arithmetic T>
inline std::ostream& operator<<(std::ostream& os, const vec<N, T>& v) {
    os << "vec" << N << "(";
    for (std::size_t i = 0; i < N; ++i) {
        if (i)
            os << ", ";
        os << v[i];
    }
    return os << ")";
}

template <std::size_t Rows, std::size_t Cols, arithmetic T>
inline std::ostream& operator<<(std::ostream& os, const mat<Rows, Cols, T>& m) {
    os << "mat" << Rows << "x" << Cols << "(";
    for (std::size_t i = 0; i < Rows; ++i) {
        if (i)
            os << ", ";
        os << "[";
        for (std::size_t j = 0; j < Cols; ++j) {
            if (j)
                os << ", ";
            os << m(i, j);
        }
        os << "]";
    }
    return os << ")";
}

template <floating_point T>
inline std::ostream& operator<<(std::ostream& os, const quat<T>& q) {
    return os << "quat(" << q.x << ", " << q.y << ", " << q.z << ", " << q.w << ")";
}

} // namespace woki::math

// ---------------------------------------------------------------------------
// std::format specializations  (must live in namespace std)
// ---------------------------------------------------------------------------

namespace std {

template <std::size_t N, woki::math::arithmetic T>
struct formatter<woki::math::vec<N, T>> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const woki::math::vec<N, T>& v, format_context& ctx) const {
        auto out = ctx.out();
        out = std::format_to(out, "vec{}(", N);
        for (std::size_t i = 0; i < N; ++i) {
            if (i)
                out = std::format_to(out, ", ");
            out = std::format_to(out, "{}", v[i]);
        }
        return std::format_to(out, ")");
    }
};

template <std::size_t Rows, std::size_t Cols, woki::math::arithmetic T>
struct formatter<woki::math::mat<Rows, Cols, T>> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const woki::math::mat<Rows, Cols, T>& m, format_context& ctx) const {
        auto out = ctx.out();
        out = std::format_to(out, "mat{}x{}(\n", Rows, Cols);
        for (std::size_t i = 0; i < Rows; ++i) {
            out = std::format_to(out, "  [");
            for (std::size_t j = 0; j < Cols; ++j) {
                if (j)
                    out = std::format_to(out, ", ");
                out = std::format_to(out, "{}", m(i, j));
            }
            out = std::format_to(out, "]");
            if (i + 1 < Rows)
                out = std::format_to(out, ",");
            out = std::format_to(out, "\n");
        }
        return std::format_to(out, ")");
    }
};

template <woki::math::floating_point T>
struct formatter<woki::math::quat<T>> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const woki::math::quat<T>& q, format_context& ctx) const {
        return std::format_to(ctx.out(), "quat({}, {}, {}, {})", q.x, q.y, q.z, q.w);
    }
};

} // namespace std
