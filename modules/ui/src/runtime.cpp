#include <algorithm>
#include <functional>

#include <woki/ui/runtime.hpp>

namespace woki::ui {

void Runtime::SetContent(View root) {
    const Key focused_key = focused_ ? focused_->Identity() : Key{};
    ReleaseAll();
    if (!root_) {
        root_ = std::make_unique<Element>(std::move(root));
    } else {
        root_->Update(std::move(root));
    }
    focused_ = focused_key ? Find(*root_, focused_key) : nullptr;
    ++stats_.reconciled;
}

void Runtime::Capture(u64 pointer, Element& target) {
    Release(pointer);
    captured_.insert_or_assign(pointer, &target);
    target.SetPressed(true);
}

void Runtime::Release(u64 pointer) {
    const auto active = captured_.find(pointer);
    if (active == captured_.end())
        return;

    Element* target = active->second;
    captured_.erase(active);
    if (target && !IsCaptured(*target))
        target->SetPressed(false);
}

void Runtime::ReleaseAll() {
    for (const auto& capture : captured_) {
        if (capture.second)
            capture.second->SetPressed(false);
    }
    captured_.clear();
}

bool Runtime::IsCaptured(const Element& element) const {
    return std::ranges::any_of(captured_, [&element](const auto& capture) { return capture.second == &element; });
}

void Runtime::Prepare(const Frame& frame) {
    display_.Clear();
    time_ = frame.time;
    stats_.painted = 0;
    stats_.culled = 0;
    if (!root_)
        return;

    if (viewport_ != frame.viewport) {
        viewport_ = frame.viewport;
        MarkPaint(*root_);
    }
    root_->Refresh();
    const Rect viewport{0.0f, 0.0f, frame.viewport.width, frame.viewport.height};
    layout_.Run(*root_, viewport);
    stats_.layout = layout_.Stats();
    semantics_.clear();
    CollectSemantics(*root_, {});

    std::unordered_set<u64> live_keys;
    CollectKeys(*root_, live_keys);
    for (auto it = backgrounds_.begin(); it != backgrounds_.end();) {
        if (!live_keys.contains(it->first)) {
            motion_.Clear(Key{it->first});
            it = backgrounds_.erase(it);
        } else {
            ++it;
        }
    }

    SyncMotion(*root_);
    if (motion_.Active(time_))
        MarkPaint(*root_);
    Canvas canvas{display_};
    Paint(*root_, canvas, viewport);
    std::vector<Element*> portals;
    const auto collect_portals = [&](const auto& self, Element& element) -> void {
        if (element.Type() == Kind::Portal)
            portals.push_back(&element);
        for (const auto& child : element.Children())
            self(self, *child);
    };
    collect_portals(collect_portals, *root_);
    for (Element* portal : portals)
        for (const auto& child : portal->Children())
            Paint(*child, canvas, viewport);
}

const Element* Runtime::Find(const Element& element, Key key) const {
    if (element.Identity() == key)
        return &element;
    for (const auto& child : element.Children())
        if (const Element* found = Find(*child, key))
            return found;
    return nullptr;
}

std::optional<Rect> Runtime::Bounds(Key key) const {
    const Element* element = root_ && key ? Find(*root_, key) : nullptr;
    return element ? std::optional{element->Bounds()} : std::nullopt;
}

bool Runtime::Visible(Key key) const {
    const auto bounds = Bounds(key);
    return bounds && bounds->width > 0 && bounds->height > 0
           && bounds->Intersects({0, 0, viewport_.width, viewport_.height});
}

bool Runtime::Focused(Key key) const {
    return focused_ && focused_->Identity() == key;
}

bool Runtime::HasTextOrModalFocus() const {
    return focused_ && (focused_->GetSemantics().role == Role::Input || focused_->GetSemantics().role == Role::Dialog);
}

void Runtime::CancelCapture() {
    ReleaseAll();
}

void Runtime::CollectSemantics(const Element& element, Key parent) {
    const Key key = element.Identity();
    if (key || element.GetSemantics().role != Role::None)
        semantics_.push_back(
            {key,
                parent,
                element.Bounds(),
                element.GetSemantics(),
                element.Bounds().width > 0 && element.Bounds().height > 0,
                &element == focused_}
        );
    for (const auto& child : element.Children())
        CollectSemantics(*child, key ? key : parent);
}

} // namespace woki::ui
