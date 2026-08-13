#pragma once

#include <optional>

#include "color.hpp"
#include "length.hpp"
#include "geometry.hpp"
#include "motion/motion.hpp"

namespace woki::ui {

enum class Flow : u8 { Row, Column, Stack, Grid };
enum class Align : u8 { Start, Center, End, Stretch };
enum class Justify : u8 { Start, Center, End, SpaceBetween, SpaceAround };
enum class Overflow : u8 { Visible, Clip, Scroll };
enum class Position : u8 { Flow, Absolute };

struct Radius {
    f32 top_left{};
    f32 top_right{};
    f32 bottom_right{};
    f32 bottom_left{};

    [[nodiscard]] static constexpr Radius All(f32 value) {
        return {value, value, value, value};
    }

    [[nodiscard]] bool operator==(const Radius&) const = default;
};

struct Stroke {
    Color color{};
    f32 width{};
    [[nodiscard]] bool operator==(const Stroke&) const = default;
};

struct Shadow {
    Color color{Color::transparent()};
    Point offset{};
    f32 blur{};
    f32 spread{};
    [[nodiscard]] bool operator==(const Shadow&) const = default;
};

struct Style {
    Flow flow{Flow::Column};
    Position position{Position::Flow};
    Align align{Align::Start};
    Justify justify{Justify::Start};
    Overflow overflow{Overflow::Visible};

    Length width{};
    Length height{};
    f32 min_width{};
    f32 min_height{};
    f32 max_width{INFINITY};
    f32 max_height{INFINITY};
    f32 gap{};
    f32 aspect{};
    u32 columns{1};
    u32 span{1};
    Inset padding{};
    Inset margin{};

    std::optional<f32> top;
    std::optional<f32> right;
    std::optional<f32> bottom;
    std::optional<f32> left;

    Color background{Color::transparent()};
    Color foreground{Color::rgba(1.0f, 1.0f, 1.0f)};
    Radius radius{};
    Stroke border{};
    Shadow shadow{};
    f32 opacity{1.0f};
    f32 font_size{14.0f};
    f32 line_height{1.2f};
    std::optional<Color> hover_background;
    std::optional<Color> pressed_background;
    Transition transition{};
    bool center_x{};
    bool center_y{};

    [[nodiscard]] bool operator==(const Style&) const = default;
};

} // namespace woki::ui
