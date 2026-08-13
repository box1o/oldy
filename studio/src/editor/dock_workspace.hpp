#pragma once

#include <string_view>
#include <optional>

#include "panel_registry.hpp"

namespace woki::studio {

class DockWorkspace final {
public:
    explicit DockWorkspace(std::string_view layout = {});
    [[nodiscard]] ui::View Build(const PanelRegistry& panels, ui::Rect bounds);
    [[nodiscard]] bool PanelVisible(ui::Key panel) const;

    [[nodiscard]] std::string Serialize() const {
        return dock_.Serialize();
    }

private:
    ui::View BuildNode(ui::DockNode& node, const PanelRegistry& panels);
    ui::DockNode* LeafAt(ui::Point point);
    ui::Dock dock_;
    std::optional<ui::Key> dragging_panel_;
    ui::Point drag_point_{};
    u64 dragging_split_{};
};

} // namespace woki::studio
