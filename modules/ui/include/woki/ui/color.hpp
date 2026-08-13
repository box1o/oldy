#pragma once

#include <algorithm>

#include <woki/math.hpp>
#include <woki/types/types.hpp>

namespace woki::ui {

struct Color {
    f32 r{};
    f32 g{};
    f32 b{};
    f32 a{1.0f};

    [[nodiscard]] static constexpr Color rgba(f32 red, f32 green, f32 blue, f32 alpha = 1.0f) {
        return {red, green, blue, alpha};
    }

    [[nodiscard]] static constexpr Color transparent() {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }

    [[nodiscard]] constexpr math::vec4f Vector() const {
        return {r, g, b, a};
    }

    [[nodiscard]] Color Clamped() const {
        return {
            std::clamp(r, 0.0f, 1.0f),
            std::clamp(g, 0.0f, 1.0f),
            std::clamp(b, 0.0f, 1.0f),
            std::clamp(a, 0.0f, 1.0f),
        };
    }

    [[nodiscard]] bool operator==(const Color&) const = default;
};

} // namespace woki::ui
