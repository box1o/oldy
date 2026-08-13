#include "scene_panel.hpp"

namespace woki::studio {

ui::View ScenePanel(std::string status) {
    ui::Semantics semantics{
        .role = ui::Role::Item,
        .label = "Animated scene viewport",
        .value = {},
        .focusable = true,
        .disabled = false,
        .selected = false,
        .checked = false,
        .tab_index = 0,
    };
    return ui::Stack(ui::Box().Id(kSceneContent).Grow().Image(kSceneImage, ui::ImageFit::Cover).SemanticsOf(std::move(semantics)),
        ui::Text(std::move(status)).Absolute().Left(10).Bottom(8).FontSize(12).Foreground(ui::Color::rgba(0.9f, 0.92f, 0.96f)))
        .Id(kScenePanel)
        .Grow()
        .Overflowed(ui::Overflow::Clip)
        .Background(ui::Color::rgba(0.025f, 0.03f, 0.04f));
}

} // namespace woki::studio
