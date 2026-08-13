#include <woki/ui/length.hpp>

namespace woki::ui {

f32 Length::Factor() const {
    if (const auto* grow = std::get_if<Grow>(&value_)) {
        return std::max(0.0f, grow->factor);
    }
    return 0.0f;
}

f32 Length::Resolve(f32 available, f32 intrinsic) const {
    if (const auto* pixels = std::get_if<Px>(&value_)) {
        return std::max(0.0f, pixels->value);
    }
    if (const auto* percent = std::get_if<Percent>(&value_)) {
        return std::max(0.0f, available * percent->value);
    }
    return std::max(0.0f, intrinsic);
}

} // namespace woki::ui
