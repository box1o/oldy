#include <woki/ui/widgets/input.hpp>

namespace woki::ui {

View Input(Edit& edit, std::string placeholder, std::function<void(std::string_view)> change, const Theme& theme) {
    Semantics semantics;
    semantics.role = Role::Input;
    semantics.label = placeholder;
    semantics.value = edit.Value();
    semantics.focusable = true;

    auto handler = [&edit, change = std::move(change)](EventContext& context, const Event& event) {
        if (context.phase != Phase::Target)
            return;
        if (const auto* text = std::get_if<TextEvent>(&event)) {
            edit.Insert(text->text);
        } else if (const auto* key = std::get_if<KeyEvent>(&event); key && key->pressed) {
            switch (key->key) {
                case KeyCode::Left:
                    edit.Move(-1, key->shift);
                    break;
                case KeyCode::Right:
                    edit.Move(1, key->shift);
                    break;
                case KeyCode::Home:
                    edit.Home(key->shift);
                    break;
                case KeyCode::End:
                    edit.End(key->shift);
                    break;
                case KeyCode::Backspace:
                    edit.Backspace();
                    break;
                case KeyCode::Delete:
                    edit.Delete();
                    break;
                default:
                    return;
            }
        } else {
            return;
        }
        if (change)
            change(edit.Value());
        context.Handle();
    };

    const std::string text = edit.Value().empty() ? std::move(placeholder) : edit.Value();
    return Row(Text(text))
        .Height(Px{34})
        .Width(Percent{1})
        .Padding(Inset::Axis(10, 6))
        .AlignItems(Align::Center)
        .Background(theme.ColorOf("input", Color::rgba(0.08f, 0.08f, 0.09f)))
        .Radius(theme.RadiusOf("control", 6))
        .Border({theme.ColorOf("border"), 1})
        .SemanticsOf(std::move(semantics))
        .OnEvent(std::move(handler));
}

} // namespace woki::ui
