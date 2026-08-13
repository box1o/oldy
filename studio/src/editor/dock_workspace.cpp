#include "dock_workspace.hpp"
#include "scene_panel.hpp"

#include <algorithm>

namespace woki::studio {
namespace {
const ui::Key kInspector = ui::Key::From("studio.panel.inspector");
const ui::Key kAssets = ui::Key::From("studio.panel.assets");
const ui::Key kDiagnostics = ui::Key::From("studio.panel.diagnostics");
} // namespace

DockWorkspace::DockWorkspace(std::string_view layout) {
    if (!layout.empty()) {
        auto parsed = ui::Dock::Parse(layout);
        if (parsed && (parsed->Root().type == ui::DockNode::Type::Split || !parsed->Root().tabs.empty())) {
            dock_ = std::move(*parsed);
            return;
        }
    }
    const u64 root = dock_.Root().id;
    dock_.Add(root, kScenePanel);
    dock_.Split(root, ui::Axis::Horizontal, 0.72f, kInspector);
    const auto* split = dock_.Find(root);
    dock_.Split(split->first->id, ui::Axis::Vertical, 0.75f, kAssets);
    dock_.Add(split->second->id, kDiagnostics);
}

ui::View DockWorkspace::Build(const PanelRegistry& panels, ui::Rect bounds) {
    dock_.Layout(bounds, 4.0f);
    ui::View content = BuildNode(dock_.Root(), panels).Size(bounds.width, bounds.height).Background(ui::Color::rgba(0.04f, 0.045f, 0.055f));
    if (dragging_panel_)
        if (ui::DockNode* leaf = LeafAt(drag_point_))
            content = ui::Stack(std::move(content), ui::Portal(ui::Box()
                                                            .Absolute()
                                                            .Left(leaf->bounds.x + 6)
                                                            .Top(leaf->bounds.y + 6)
                                                            .Size(leaf->bounds.width - 12, leaf->bounds.height - 12)
                                                            .Background(ui::Color::rgba(0.18f, 0.42f, 0.85f, 0.2f))
                                                            .Border({ui::Color::rgba(0.32f, 0.58f, 1.0f), 2})));
    return content;
}

ui::View DockWorkspace::BuildNode(ui::DockNode& node, const PanelRegistry& panels) {
    if (!node.Leaf()) {
        const bool horizontal = node.axis == ui::Axis::Horizontal;
        const f32 x = horizontal ? node.first->bounds.x + node.first->bounds.width : node.bounds.x;
        const f32 y = horizontal ? node.bounds.y : node.first->bounds.y + node.first->bounds.height;
        ui::View splitter = ui::Box()
                                .Absolute()
                                .Left(x)
                                .Top(y)
                                .Size(horizontal ? 4 : node.bounds.width, horizontal ? node.bounds.height : 4)
                                .Background(ui::Color::rgba(0.12f, 0.13f, 0.16f))
                                .Hover(ui::Color::rgba(0.28f, 0.45f, 0.75f))
                                .OnEvent([this, id = node.id, horizontal, bounds = node.bounds](ui::EventContext& context, const ui::Event& event) {
                                    const auto* pointer = std::get_if<ui::PointerEvent>(&event);
                                    if (!pointer)
                                        return;
                                    if (pointer->type == ui::PointerEvent::Type::Down)
                                        dragging_split_ = id;
                                    if (pointer->type == ui::PointerEvent::Type::Move && dragging_split_ == id) {
                                        const f32 ratio = horizontal ? (pointer->position.x - bounds.x) / bounds.width : (pointer->position.y - bounds.y) / bounds.height;
                                        dock_.SetRatio(id, std::clamp(ratio, 0.05f, 0.95f));
                                    }
                                    if (pointer->type == ui::PointerEvent::Type::Up || pointer->type == ui::PointerEvent::Type::Cancel)
                                        dragging_split_ = 0;
                                    context.Handle();
                                });
        return ui::Stack(BuildNode(*node.first, panels), BuildNode(*node.second, panels), std::move(splitter));
    }
    ui::View tabs = ui::Row().Height(ui::Px{28}).Background(ui::Color::rgba(0.075f, 0.08f, 0.095f));
    for (size_t index = 0; index < node.tabs.size(); ++index) {
        const ui::Key panel = node.tabs[index];
        tabs.Add(ui::Text(std::string(panels.Title(panel)))
                .Padding({10, 7, 10, 5})
                .Background(index == node.active ? ui::Color::rgba(0.13f, 0.15f, 0.19f) : ui::Color{})
                .OnEvent([this, panel, leaf = node.id, index](ui::EventContext& context, const ui::Event& event) {
                    const auto* pointer = std::get_if<ui::PointerEvent>(&event);
                    if (!pointer)
                        return;
                    if (pointer->type == ui::PointerEvent::Type::Down && pointer->button == ui::PointerButton::Primary) {
                        dock_.Activate(leaf, index);
                        dragging_panel_ = panel;
                        drag_point_ = pointer->position;
                        context.Handle();
                    }
                    if (pointer->type == ui::PointerEvent::Type::Move && dragging_panel_) {
                        drag_point_ = pointer->position;
                        context.Handle();
                    }
                    if (pointer->type == ui::PointerEvent::Type::Up && dragging_panel_) {
                        if (ui::DockNode* target = LeafAt(pointer->position)) {
                            const f32 x = (pointer->position.x - target->bounds.x) / std::max(target->bounds.width, 1.0f);
                            const f32 y = (pointer->position.y - target->bounds.y) / std::max(target->bounds.height, 1.0f);
                            if (x < 0.2f)
                                dock_.Split(target->id, ui::Axis::Horizontal, 0.35f, *dragging_panel_, true);
                            else if (x > 0.8f)
                                dock_.Split(target->id, ui::Axis::Horizontal, 0.65f, *dragging_panel_);
                            else if (y < 0.2f)
                                dock_.Split(target->id, ui::Axis::Vertical, 0.35f, *dragging_panel_, true);
                            else if (y > 0.8f)
                                dock_.Split(target->id, ui::Axis::Vertical, 0.65f, *dragging_panel_);
                            else
                                dock_.Move(*dragging_panel_, target->id, target->tabs.size());
                        }
                        dragging_panel_.reset();
                        dock_.Collapse();
                        context.Handle();
                    }
                    if (pointer->type == ui::PointerEvent::Type::Down && pointer->button == ui::PointerButton::Secondary) {
                        dock_.Remove(panel);
                        dock_.Collapse();
                        dragging_panel_.reset();
                        context.Handle();
                    }
                }));
    }
    ui::View content = node.tabs.empty() ? ui::Text("Drop a panel here") : panels.Build(node.tabs[node.active]);
    return ui::Column(std::move(tabs), std::move(content).Grow())
        .Absolute()
        .Left(node.bounds.x)
        .Top(node.bounds.y)
        .Size(node.bounds.width, node.bounds.height)
        .MinSize(120, 80)
        .Overflowed(ui::Overflow::Clip)
        .Border({ui::Color::rgba(0.13f, 0.14f, 0.17f), 1});
}

ui::DockNode* DockWorkspace::LeafAt(ui::Point point) {
    const auto find = [&](const auto& self, ui::DockNode& node) -> ui::DockNode* {
        if (!node.bounds.Contains(point))
            return nullptr;
        if (node.Leaf())
            return &node;
        if (auto* first = self(self, *node.first))
            return first;
        return self(self, *node.second);
    };
    return find(find, dock_.Root());
}

bool DockWorkspace::PanelVisible(ui::Key panel) const {
    const auto visit = [&](const auto& self, const ui::DockNode& node) -> bool {
        if (node.Leaf())
            return !node.tabs.empty() && node.tabs[node.active] == panel;
        return self(self, *node.first) || self(self, *node.second);
    };
    return visit(visit, dock_.Root());
}

} // namespace woki::studio
