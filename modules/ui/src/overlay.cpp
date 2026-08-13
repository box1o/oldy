#include <woki/ui/widgets/overlay.hpp>

namespace woki::ui {

View Dialog(View content, bool open, const Theme& theme) {
    if (!open)
        return Box();
    Semantics semantics;
    semantics.role = Role::Dialog;
    semantics.focusable = true;
    View surface = Panel(std::move(content), theme).Absolute().Center().Width(Px{480});
    return Portal(Stack(
        Box().Absolute().Top(0).Right(0).Bottom(0).Left(0).Background(Color::rgba(0, 0, 0, 0.5f)),
        std::move(surface)
    )
            .Width(Percent{1})
            .Height(Percent{1})
            .SemanticsOf(std::move(semantics)));
}

View Sheet(View content, bool open, bool right, f32 width, const Theme& theme) {
    if (!open)
        return Box();
    View surface = Panel(std::move(content), theme).Absolute().Top(0).Bottom(0).Width(Px{width});
    if (right)
        surface.Right(0);
    else
        surface.Left(0);
    return Portal(Stack(
        Box().Absolute().Top(0).Right(0).Bottom(0).Left(0).Background(Color::rgba(0, 0, 0, 0.35f)),
        std::move(surface)
    )
            .Width(Percent{1})
            .Height(Percent{1}));
}

} // namespace woki::ui
