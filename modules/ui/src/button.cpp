#include <woki/ui/widgets/button.hpp>

namespace woki::ui {

namespace {

Color Background(const Theme& theme, Tone tone) {
    switch (tone) {
        case Tone::Secondary:
            return theme.ColorOf("secondary", Color::rgba(0.2f, 0.2f, 0.22f));
        case Tone::Destructive:
            return theme.ColorOf("destructive", Color::rgba(0.75f, 0.18f, 0.2f));
        case Tone::Outline:
        case Tone::Ghost:
            return Color::transparent();
        case Tone::Primary:
            return theme.ColorOf("primary", Color::rgba(0.22f, 0.48f, 0.95f));
    }
    return {};
}

} // namespace

View Button(std::string label, std::function<void()> action, Tone tone, ControlSize size, const Theme& theme) {
    const f32 height = size == ControlSize::Small ? 28.0f : size == ControlSize::Large ? 40.0f : 34.0f;
    Semantics semantics;
    semantics.role = Role::Button;
    semantics.label = label;
    semantics.focusable = true;

    auto handler = [action = std::move(action)](EventContext& context, const Event& event) {
        if (context.phase != Phase::Target)
            return;
        const auto* pointer = std::get_if<PointerEvent>(&event);
        const auto* key = std::get_if<KeyEvent>(&event);
        if ((pointer && pointer->type == PointerEvent::Type::Up)
            || (key && key->pressed && (key->key == KeyCode::Enter || key->key == KeyCode::Space))) {
            if (action)
                action();
            context.Handle();
        }
    };

    View button = Row(Text(label));
    button.Height(Px{height})
        .Padding(Inset::Axis(size == ControlSize::Large ? 18.0f : 12.0f, 6.0f))
        .AlignItems(Align::Center)
        .JustifyItems(Justify::Center)
        .Background(Background(theme, tone))
        .Hover(theme.ColorOf("accent", Background(theme, tone)))
        .Pressed(theme.ColorOf("pressed", Background(theme, tone)))
        .Transitioned(theme.MotionOf("fast"))
        .Radius(theme.RadiusOf("control", 6.0f))
        .SemanticsOf(std::move(semantics))
        .OnEvent(std::move(handler));
    if (tone == Tone::Outline) {
        button.Border({theme.ColorOf("border", Color::rgba(0.35f, 0.35f, 0.38f)), 1.0f});
    }
    return button;
}

} // namespace woki::ui
