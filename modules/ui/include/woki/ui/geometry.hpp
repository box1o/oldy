#pragma once

#include <cmath>
#include <algorithm>

#include <woki/math.hpp>
#include <woki/types/types.hpp>

namespace woki::ui {

struct Point {
    f32 x{};
    f32 y{};

    constexpr Point() = default;

    constexpr Point(f32 x_value, f32 y_value)
        : x(x_value),
          y(y_value) {}

    constexpr explicit Point(const math::vec2f& value)
        : x(value.x),
          y(value.y) {}

    [[nodiscard]] constexpr math::vec2f Vector() const {
        return {x, y};
    }

    [[nodiscard]] constexpr bool operator==(const Point&) const = default;
};

struct Size {
    f32 width{};
    f32 height{};

    [[nodiscard]] constexpr bool Empty() const {
        return width <= 0.0f || height <= 0.0f;
    }

    [[nodiscard]] constexpr bool operator==(const Size&) const = default;
};

struct Rect {
    f32 x{};
    f32 y{};
    f32 width{};
    f32 height{};

    [[nodiscard]] constexpr f32 Right() const {
        return x + width;
    }

    [[nodiscard]] constexpr f32 Bottom() const {
        return y + height;
    }

    [[nodiscard]] constexpr bool Contains(Point point) const {
        return point.x >= x && point.y >= y && point.x <= Right() && point.y <= Bottom();
    }

    [[nodiscard]] constexpr bool Intersects(const Rect& other) const {
        return x < other.Right() && Right() > other.x && y < other.Bottom() && Bottom() > other.y;
    }

    [[nodiscard]] constexpr bool operator==(const Rect&) const = default;
};

struct Inset {
    f32 top{};
    f32 right{};
    f32 bottom{};
    f32 left{};

    [[nodiscard]] static constexpr Inset All(f32 value) {
        return {value, value, value, value};
    }

    [[nodiscard]] static constexpr Inset Axis(f32 horizontal, f32 vertical) {
        return {vertical, horizontal, vertical, horizontal};
    }

    [[nodiscard]] constexpr bool operator==(const Inset&) const = default;
};

struct Constraints {
    Size min{};
    Size max{INFINITY, INFINITY};

    [[nodiscard]] Size Clamp(Size value) const {
        return {
            std::clamp(value.width, min.width, max.width),
            std::clamp(value.height, min.height, max.height),
        };
    }

    [[nodiscard]] bool operator==(const Constraints&) const = default;
};

[[nodiscard]] constexpr Rect InsetRect(Rect rect, Inset inset) {
    return {
        rect.x + inset.left,
        rect.y + inset.top,
        std::max(0.0f, rect.width - inset.left - inset.right),
        std::max(0.0f, rect.height - inset.top - inset.bottom),
    };
}

} // namespace woki::ui
