#pragma once

#include <string_view>

#include "../geometry.hpp"

namespace woki::ui {

struct TextStyle {
    f32 size{14.0f};
    f32 line{1.2f};
    bool wrap{true};
};

class TextEngine {
public:
    virtual ~TextEngine() = default;
    [[nodiscard]] virtual Size Measure(std::string_view text, f32 max_width, TextStyle style) const = 0;
    [[nodiscard]] virtual size_t Hit(std::string_view text, f32 x, TextStyle style) const = 0;
};

class SimpleText final : public TextEngine {
public:
    [[nodiscard]] Size Measure(std::string_view text, f32 max_width, TextStyle style) const override;
    [[nodiscard]] size_t Hit(std::string_view text, f32 x, TextStyle style) const override;
};

} // namespace woki::ui
