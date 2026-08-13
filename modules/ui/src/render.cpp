#include <woki/ui/runtime.hpp>

namespace woki::ui {

namespace {

Color WithOpacity(Color color, f32 opacity) {
    color.a *= opacity;
    return color;
}

} // namespace

void Runtime::Paint(Element& element, Canvas& canvas, Rect viewport) {
    if (element.Type() == Kind::Portal)
        return;
    const Style& style = element.GetStyle();
    const bool visible = element.Bounds().Intersects(viewport);
    if (!visible && style.overflow != Overflow::Visible) {
        ++stats_.culled;
        return;
    }
    if (!element.Needs(Dirty::Paint) && !element.PaintCache().empty()) {
        display_.Append(element.PaintCache());
        return;
    }

    const size_t first = display_.Operations().size();
    ++stats_.painted;
    if (visible) {
        Shadow shadow = style.shadow;
        shadow.color = WithOpacity(shadow.color, style.opacity);
        if (shadow.color.a > 0.0f)
            canvas.Shadow(element.Bounds(), style.radius, shadow);
        const Color background = WithOpacity(Background(element), style.opacity);
        if (background.a > 0.0f)
            canvas.Round(element.Bounds(), style.radius, background);
        Stroke border = style.border;
        border.color = WithOpacity(border.color, style.opacity);
        if (border.width > 0.0f && border.color.a > 0.0f)
            canvas.Border(element.Bounds(), style.radius, border);
        if (element.Type() == Kind::Text && !element.Content().empty())
            canvas.Text(
                {element.Bounds().x, element.Bounds().y},
                element.Content(),
                WithOpacity(style.foreground, style.opacity),
                style.font_size
            );
        if (element.ImageResource())
            canvas.Image(
                element.Bounds(),
                element.ImageResource(),
                element.ImageSizing(),
                Color::rgba(1, 1, 1, style.opacity)
            );
    }
    const bool clip = style.overflow != Overflow::Visible;
    if (clip)
        canvas.Clip(element.Bounds(), style.radius);
    for (const auto& child : element.Children())
        Paint(*child, canvas, viewport);
    if (clip)
        canvas.Restore();

    element.Clear(Dirty::Paint);
    element.CountPaint();
    const auto& operations = display_.Operations();
    element.SetPaintCache({operations.begin() + static_cast<ptrdiff_t>(first), operations.end()});
}

u64 Runtime::MotionKey(const Element& element) const {
    return element.Token();
}

Color Runtime::Background(const Element& element) const {
    return std::get<Color>(motion_.Get(Key{MotionKey(element)}, Property::Background, time_));
}

void Runtime::MarkPaint(Element& element) {
    element.Mark(Dirty::Paint);
    for (const auto& child : element.Children())
        MarkPaint(*child);
}

void Runtime::SyncMotion(Element& element) {
    const Style& style = element.GetStyle();
    Color target = style.background;
    if (element.Hovered() && style.hover_background)
        target = *style.hover_background;
    if (element.Pressed() && style.pressed_background)
        target = *style.pressed_background;
    const u64 key = MotionKey(element);
    const auto previous = backgrounds_.find(key);
    if (previous == backgrounds_.end()) {
        backgrounds_.emplace(key, target);
        motion_.Set(Key{key}, Property::Background, target, {.duration = {}}, time_);
    } else if (previous->second != target) {
        previous->second = target;
        motion_.Set(Key{key}, Property::Background, target, style.transition, time_);
    }
    for (const auto& child : element.Children())
        SyncMotion(*child);
}

void Runtime::CollectKeys(Element& element, std::unordered_set<u64>& keys) const {
    keys.insert(MotionKey(element));
    for (const auto& child : element.Children())
        CollectKeys(*child, keys);
}

} // namespace woki::ui
