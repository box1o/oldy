#pragma once

#include <woki/ui.hpp>

namespace woki::studio {

inline const ui::Key kScenePanel = ui::Key::From("studio.panel.scene");
inline const ui::Key kSceneContent = ui::Key::From("studio.panel.scene.content");
inline constexpr ui::ImageId kSceneImage{0x5343454e45ULL};

ui::View ScenePanel(std::string status);

} // namespace woki::studio
