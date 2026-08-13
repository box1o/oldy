#include <cmath>
#include <algorithm>

#include <woki/ui/text/text.hpp>

namespace woki::ui {

Size SimpleText::Measure(std::string_view text, f32 max_width, TextStyle style) const {
    const f32 advance = style.size * 0.55f;
    const f32 natural = advance * static_cast<f32>(text.size());
    if (!style.wrap || !std::isfinite(max_width) || natural <= max_width || max_width <= 0.0f) {
        return {natural, style.size * style.line};
    }
    const f32 lines = std::ceil(natural / max_width);
    return {max_width, lines * style.size * style.line};
}

size_t SimpleText::Hit(std::string_view text, f32 x, TextStyle style) const {
    const f32 advance = std::max(style.size * 0.55f, 0.001f);
    return std::min(text.size(), static_cast<size_t>(std::max(0.0f, std::floor(x / advance + 0.5f))));
}

} // namespace woki::ui
