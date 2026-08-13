#include <cmath>

#include <woki/ui/display.hpp>

namespace woki::ui {

namespace {

bool Finite(Rect rect) {
    return std::isfinite(rect.x) && std::isfinite(rect.y) && std::isfinite(rect.width) && std::isfinite(rect.height)
           && rect.width >= 0.0f && rect.height >= 0.0f;
}

bool Finite(Color color) {
    return std::isfinite(color.r) && std::isfinite(color.g) && std::isfinite(color.b) && std::isfinite(color.a);
}

bool Finite(Radius radius) {
    return std::isfinite(radius.top_left) && std::isfinite(radius.top_right) && std::isfinite(radius.bottom_right)
           && std::isfinite(radius.bottom_left) && radius.top_left >= 0.0f && radius.top_right >= 0.0f
           && radius.bottom_right >= 0.0f && radius.bottom_left >= 0.0f;
}

bool Finite(Stroke stroke) {
    return Finite(stroke.color) && std::isfinite(stroke.width) && stroke.width >= 0.0f;
}

bool Finite(const Shadow& shadow) {
    return Finite(shadow.color) && std::isfinite(shadow.offset.x) && std::isfinite(shadow.offset.y)
           && std::isfinite(shadow.blur) && std::isfinite(shadow.spread) && shadow.blur >= 0.0f;
}

} // namespace

bool DisplayList::Valid() const {
    i32 clips = 0;
    for (const auto& operation : operations_) {
        const bool valid = std::visit(
            [&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::same_as<T, PopClip>) {
                    return --clips >= 0;
                } else if constexpr (std::same_as<T, TextOp>) {
                    return std::isfinite(value.origin.x) && std::isfinite(value.origin.y) && std::isfinite(value.size)
                           && value.size >= 0.0f && Finite(value.color);
                } else if constexpr (std::same_as<T, ImageOp>) {
                    return Finite(value.rect) && static_cast<bool>(value.image) && Finite(value.tint);
                } else {
                    if constexpr (std::same_as<T, ClipOp>) {
                        ++clips;
                    }
                    if constexpr (std::same_as<T, RectOp>) {
                        return Finite(value.rect) && Finite(value.color);
                    } else if constexpr (std::same_as<T, RoundOp>) {
                        return Finite(value.rect) && Finite(value.radius) && Finite(value.color);
                    } else if constexpr (std::same_as<T, BorderOp>) {
                        return Finite(value.rect) && Finite(value.radius) && Finite(value.stroke);
                    } else if constexpr (std::same_as<T, ShadowOp>) {
                        return Finite(value.rect) && Finite(value.radius) && Finite(value.shadow);
                    } else {
                        return Finite(value.rect) && Finite(value.radius);
                    }
                }
            },
            operation
        );
        if (!valid) {
            return false;
        }
    }
    return clips == 0;
}

void Canvas::Fill(ui::Rect rect, Color color) {
    list_.Add(RectOp{rect, color.Clamped()});
}

void Canvas::Round(ui::Rect rect, Radius radius, Color color) {
    list_.Add(RoundOp{rect, radius, color.Clamped()});
}

void Canvas::Border(ui::Rect rect, Radius radius, Stroke stroke) {
    list_.Add(BorderOp{rect, radius, stroke});
}

void Canvas::Shadow(ui::Rect rect, Radius radius, const ui::Shadow& shadow) {
    list_.Add(ShadowOp{rect, radius, shadow});
}

void Canvas::Text(Point origin, std::string text, Color color, f32 size) {
    list_.Add(TextOp{origin, std::move(text), color.Clamped(), size});
}

void Canvas::Image(ui::Rect rect, ImageId image, ImageFit fit, Color tint) {
    list_.Add(ImageOp{rect, image, fit, tint.Clamped()});
}

void Canvas::Clip(ui::Rect rect, Radius radius) {
    list_.Add(ClipOp{rect, radius});
}

void Canvas::Restore() {
    list_.Add(PopClip{});
}

} // namespace woki::ui
