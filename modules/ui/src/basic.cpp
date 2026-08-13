#include <algorithm>

#include <woki/ui/widgets/basic.hpp>

namespace woki::ui {

namespace {

EventHandler Toggle(bool next, std::function<void(bool)> change) {
    return [next, change = std::move(change)](EventContext& context, const Event& event) {
        if (context.phase != Phase::Target)
            return;
        const auto* pointer = std::get_if<PointerEvent>(&event);
        const auto* key = std::get_if<KeyEvent>(&event);
        if ((pointer && pointer->type == PointerEvent::Type::Up)
            || (key && key->pressed && key->key == KeyCode::Space)) {
            if (change)
                change(next);
            context.Handle();
        }
    };
}

} // namespace

View Panel(View content, const Theme& theme) {
    return Box()
        .Children(std::move(content))
        .Padding(Inset::All(theme.Space("panel", 12.0f)))
        .Background(theme.ColorOf("card", Color::rgba(0.1f, 0.1f, 0.11f)))
        .Radius(theme.RadiusOf("panel", 8.0f))
        .Border({theme.ColorOf("border", Color::rgba(0.2f, 0.2f, 0.22f)), 1.0f});
}

View Badge(std::string label, Tone tone, const Theme& theme) {
    Color color = tone == Tone::Destructive ? theme.ColorOf("destructive") : theme.ColorOf("secondary");
    return Row(Text(std::move(label))).Padding(Inset::Axis(8, 2)).Background(color).Radius(4);
}

View Alert(std::string title, std::string description, Tone tone, const Theme& theme) {
    return Panel(
        Column(Text(std::move(title)), Text(std::move(description)))
            .Gap(4)
            .Border({tone == Tone::Destructive ? theme.ColorOf("destructive") : theme.ColorOf("border"), 1}),
        theme
    );
}

View Checkbox(std::string label, bool checked, std::function<void(bool)> change, const Theme& theme) {
    Semantics semantics;
    semantics.role = Role::Checkbox;
    semantics.label = label;
    semantics.focusable = true;
    semantics.checked = checked;
    View mark = Box().Size(16, 16).Radius(3).Border({theme.ColorOf("border"), 1});
    if (checked)
        mark.Background(theme.ColorOf("primary"));
    return Row(std::move(mark), Text(std::move(label)))
        .Gap(8)
        .AlignItems(Align::Center)
        .SemanticsOf(std::move(semantics))
        .OnEvent(Toggle(!checked, std::move(change)));
}

View Switch(std::string label, bool checked, std::function<void(bool)> change, const Theme& theme) {
    Semantics semantics;
    semantics.role = Role::Switch;
    semantics.label = label;
    semantics.focusable = true;
    semantics.checked = checked;
    View knob = Box().Size(16, 16).Radius(8).Background(theme.ColorOf("foreground", Color::rgba(1, 1, 1)));
    if (checked)
        knob.Margin({0, 0, 0, 16});
    View track = Row(std::move(knob))
                     .Size(34, 18)
                     .Padding(Inset::All(1))
                     .Radius(9)
                     .Background(theme.ColorOf(checked ? "primary" : "muted"));
    return Row(std::move(track), Text(std::move(label)))
        .Gap(8)
        .AlignItems(Align::Center)
        .SemanticsOf(std::move(semantics))
        .OnEvent(Toggle(!checked, std::move(change)));
}

View Progress(f32 value, const Theme& theme) {
    const f32 fraction = std::clamp(value, 0.0f, 1.0f);
    return Stack(
        Box().Height(Px{6}).Width(Percent{1}).Radius(3).Background(theme.ColorOf("muted")),
        Box().Height(Px{6}).Width(Percent{fraction}).Radius(3).Background(theme.ColorOf("primary"))
    )
        .Height(Px{6})
        .Width(Percent{1});
}

} // namespace woki::ui
