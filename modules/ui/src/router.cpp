#include <algorithm>

#include <woki/ui/runtime.hpp>

namespace woki::ui {

bool Runtime::HandleEvent(const Event& event) {
    if (!root_)
        return false;
    if (const auto* pointer = std::get_if<PointerEvent>(&event)) {
        pointer_ = pointer->position;
        UpdateHover(*root_, pointer->position);
        SyncMotion(*root_);
        Element* target{};
        if (pointer->type == PointerEvent::Type::Down) {
            Release(pointer->pointer);
            target = Hit(*root_, pointer->position);
        } else {
            const auto captured = captured_.find(pointer->pointer);
            target = captured != captured_.end() ? captured->second : Hit(*root_, pointer->position);
        }
        if (captured_.find(pointer->pointer) == captured_.end()) {
            const auto hit_portal = [&](const auto& self, Element& element) -> Element* {
                for (auto it = element.Children().rbegin(); it != element.Children().rend(); ++it) {
                    if ((*it)->Type() == Kind::Portal) {
                        for (auto child = (*it)->Children().rbegin(); child != (*it)->Children().rend(); ++child)
                            if (Element* hit = Hit(**child, pointer->position))
                                return hit;
                    }
                    if (Element* hit = self(self, **it))
                        return hit;
                }
                return nullptr;
            };
            if (Element* overlay = hit_portal(hit_portal, *root_))
                target = overlay;
        }
        if (!target)
            return false;

        if (pointer->type == PointerEvent::Type::Down) {
            Capture(pointer->pointer, *target);
            if (target->GetSemantics().focusable && !target->GetSemantics().disabled)
                focused_ = target;
        }
        bool handled = Route(*target, event);
        if (!handled && pointer->type == PointerEvent::Type::Wheel) {
            for (Element* node = target; node; node = node->Parent()) {
                if (node->GetStyle().overflow == Overflow::Scroll) {
                    node->ScrollBy(pointer->delta);
                    handled = true;
                    break;
                }
            }
        }
        if (pointer->type == PointerEvent::Type::Up || pointer->type == PointerEvent::Type::Cancel) {
            Release(pointer->pointer);
        }
        return handled;
    }
    if (const auto* key = std::get_if<KeyEvent>(&event); key && key->pressed && key->key == KeyCode::Tab) {
        FocusNext(key->shift);
        return true;
    }
    return focused_ ? Route(*focused_, event) : false;
}

Element* Runtime::Hit(Element& element, Point point) {
    if (element.Type() == Kind::Portal)
        return nullptr;
    if (element.GetSemantics().disabled)
        return nullptr;
    const bool inside = element.Bounds().Contains(point);
    if (!inside && element.GetStyle().overflow != Overflow::Visible)
        return nullptr;
    for (auto it = element.Children().rbegin(); it != element.Children().rend(); ++it) {
        if (Element* hit = Hit(**it, point))
            return hit;
    }
    return inside && (element.Handler() || element.GetSemantics().focusable) ? &element : nullptr;
}

Element* Runtime::Find(Element& element, Key key) {
    if (element.Identity() == key)
        return &element;
    for (const auto& child : element.Children()) {
        if (Element* found = Find(*child, key))
            return found;
    }
    return nullptr;
}

bool Runtime::Route(Element& target, const Event& event) {
    std::vector<Element*> path;
    for (Element* node = &target; node; node = node->Parent())
        path.push_back(node);
    EventContext context{.phase = Phase::Capture};
    for (auto it = path.rbegin(); it != path.rend() - 1 && !context.stopped; ++it) {
        if ((*it)->Handler())
            (*it)->Handler()(context, event);
    }
    if (!context.stopped && target.Handler()) {
        context.phase = Phase::Target;
        target.Handler()(context, event);
    }
    context.phase = Phase::Bubble;
    for (size_t index = 1; index < path.size() && !context.stopped; ++index) {
        if (path[index]->Handler())
            path[index]->Handler()(context, event);
    }
    return context.handled;
}

void Runtime::CollectFocus(Element& element, std::vector<Element*>& result) {
    if (element.GetSemantics().focusable && !element.GetSemantics().disabled)
        result.push_back(&element);
    for (const auto& child : element.Children())
        CollectFocus(*child, result);
}

void Runtime::FocusNext(bool reverse) {
    std::vector<Element*> nodes;
    CollectFocus(*root_, nodes);
    std::stable_sort(nodes.begin(), nodes.end(), [](const Element* left, const Element* right) {
        return left->GetSemantics().tab_index < right->GetSemantics().tab_index;
    });
    if (nodes.empty()) {
        focused_ = nullptr;
        return;
    }
    const auto current = std::find(nodes.begin(), nodes.end(), focused_);
    if (current == nodes.end()) {
        focused_ = reverse ? nodes.back() : nodes.front();
        return;
    }
    const size_t index = static_cast<size_t>(std::distance(nodes.begin(), current));
    focused_ = reverse ? nodes[(index + nodes.size() - 1) % nodes.size()] : nodes[(index + 1) % nodes.size()];
}

void Runtime::UpdateHover(Element& element, Point point) {
    element.SetHovered(element.Bounds().Contains(point));
    for (const auto& child : element.Children())
        UpdateHover(*child, point);
}

} // namespace woki::ui
