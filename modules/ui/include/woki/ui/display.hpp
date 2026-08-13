#pragma once

#include <string>
#include <vector>
#include <variant>

#include "style.hpp"

namespace woki::ui {

struct ImageId {
    u64 value{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return value != 0;
    }

    friend bool operator==(const ImageId&, const ImageId&) = default;
};

enum class ImageFit : u8 { Fill, Contain, Cover, None };

struct ImageOp {
    Rect rect;
    ImageId image;
    ImageFit fit{ImageFit::Fill};
    Color tint{Color::rgba(1, 1, 1, 1)};
    bool operator==(const ImageOp&) const = default;
};

struct RectOp {
    Rect rect;
    Color color;
    bool operator==(const RectOp&) const = default;
};

struct RoundOp {
    Rect rect;
    Radius radius;
    Color color;
    bool operator==(const RoundOp&) const = default;
};

struct BorderOp {
    Rect rect;
    Radius radius;
    Stroke stroke;
    bool operator==(const BorderOp&) const = default;
};

struct ShadowOp {
    Rect rect;
    Radius radius;
    Shadow shadow;
    bool operator==(const ShadowOp&) const = default;
};

struct TextOp {
    Point origin;
    std::string text;
    Color color;
    f32 size{14.0f};
    bool operator==(const TextOp&) const = default;
};

struct ClipOp {
    Rect rect;
    Radius radius;
    bool operator==(const ClipOp&) const = default;
};

struct PopClip {
    bool operator==(const PopClip&) const = default;
};

using DisplayOp = std::variant<RectOp, RoundOp, BorderOp, ShadowOp, TextOp, ImageOp, ClipOp, PopClip>;

class DisplayList {
public:
    void Clear() {
        operations_.clear();
    }

    void Add(DisplayOp operation) {
        operations_.push_back(std::move(operation));
    }

    void Append(const std::vector<DisplayOp>& operations) {
        operations_.insert(operations_.end(), operations.begin(), operations.end());
    }

    [[nodiscard]] const std::vector<DisplayOp>& Operations() const {
        return operations_;
    }

    [[nodiscard]] bool Valid() const;

private:
    std::vector<DisplayOp> operations_;
};

class Canvas {
public:
    explicit Canvas(DisplayList& list)
        : list_(list) {}

    void Fill(ui::Rect rect, Color color);
    void Round(ui::Rect rect, Radius radius, Color color);
    void Border(ui::Rect rect, Radius radius, Stroke stroke);
    void Shadow(ui::Rect rect, Radius radius, const ui::Shadow& shadow);
    void Text(Point origin, std::string text, Color color, f32 size = 14.0f);
    void Image(ui::Rect rect, ImageId image, ImageFit fit = ImageFit::Fill, Color tint = Color::rgba(1, 1, 1, 1));
    void Clip(ui::Rect rect, Radius radius = {});
    void Restore();

private:
    DisplayList& list_;
};

} // namespace woki::ui
