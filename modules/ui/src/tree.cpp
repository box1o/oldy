#include <unordered_map>

#include <woki/ui/tree.hpp>

namespace woki::ui {

namespace {

bool LayoutChanged(const Style& before, const Style& after) {
    return before.flow != after.flow || before.position != after.position || before.align != after.align
           || before.justify != after.justify || before.overflow != after.overflow || before.width != after.width
           || before.height != after.height || before.min_width != after.min_width
           || before.min_height != after.min_height || before.max_width != after.max_width
           || before.max_height != after.max_height || before.gap != after.gap || before.aspect != after.aspect
           || before.padding != after.padding || before.columns != after.columns || before.span != after.span
           || before.margin != after.margin || before.font_size != after.font_size
           || before.line_height != after.line_height || before.center_x != after.center_x
           || before.center_y != after.center_y || before.top != after.top || before.right != after.right
           || before.bottom != after.bottom || before.left != after.left;
}

} // namespace

Element::Element(View view, Element* parent)
    : kind_(view.Type()),
      parent_(parent) {
    Update(std::move(view));
}

void Element::Update(View view) {
    const Style next_style = view.GetStyle();
    const bool first = stats_.builds == 0;
    const bool layout_changed = first || kind_ != view.Type() || content_ != view.Content()
                                || LayoutChanged(style_, next_style);
    const bool paint_changed = layout_changed || style_ != next_style || content_ != view.Content()
                               || image_ != view.ImageResource() || image_fit_ != view.ImageSizing();
    const bool semantics_changed = semantics_.role != view.GetSemantics().role
                                   || semantics_.label != view.GetSemantics().label
                                   || semantics_.value != view.GetSemantics().value
                                   || semantics_.focusable != view.GetSemantics().focusable
                                   || semantics_.disabled != view.GetSemantics().disabled
                                   || semantics_.selected != view.GetSemantics().selected
                                   || semantics_.checked != view.GetSemantics().checked
                                   || semantics_.tab_index != view.GetSemantics().tab_index;

    kind_ = view.Type();
    key_ = view.Identity();
    style_ = next_style;
    semantics_ = view.GetSemantics();
    handler_ = view.Handler();
    content_ = view.Content();
    image_ = view.ImageResource();
    image_fit_ = view.ImageSizing();
    component_ = view.Instance();
    if (kind_ == Kind::Component && component_) {
        component_revision_ = component_->Revision();
        view.Add(component_->Build());
    }

    auto previous = std::move(children_);
    children_.clear();
    children_.reserve(view.ChildViews().size());

    std::vector<bool> used(previous.size(), false);
    bool structure_changed = view.ChildViews().size() != previous.size();
    for (size_t index = 0; index < view.ChildViews().size(); ++index) {
        View& declaration = view.ChildViews()[index];
        size_t match = previous.size();

        if (declaration.Identity()) {
            for (size_t candidate = 0; candidate < previous.size(); ++candidate) {
                if (!used[candidate] && previous[candidate]->Identity() == declaration.Identity()
                    && previous[candidate]->Type() == declaration.Type()) {
                    match = candidate;
                    break;
                }
            }
        } else if (index < previous.size() && !used[index] && !previous[index]->Identity()
                   && previous[index]->Type() == declaration.Type()) {
            match = index;
        }

        structure_changed = structure_changed || match != index;
        View child = std::move(declaration);
        if (match < previous.size()) {
            used[match] = true;
            previous[match]->parent_ = this;
            previous[match]->Update(std::move(child));
            children_.push_back(std::move(previous[match]));
        } else {
            children_.push_back(std::make_unique<Element>(std::move(child), this));
        }
    }

    ++stats_.builds;
    if (layout_changed || structure_changed) {
        Mark(Dirty::Layout | Dirty::Paint | Dirty::Hit);
    } else if (paint_changed) {
        Mark(Dirty::Paint);
    }
    if (semantics_changed) {
        Mark(Dirty::Semantics | Dirty::Hit);
    }
}

void Element::Refresh() {
    if (component_ && component_revision_ != component_->Revision()) {
        View declaration = Mount(component_);
        declaration.Id(key_);
        Update(std::move(declaration));
    }
    for (auto& child : children_) {
        child->Refresh();
    }
}

void Element::Mark(Dirty dirty) {
    dirty_ = dirty_ | dirty;
    if (parent_ && static_cast<u8>(dirty & Dirty::Layout) != 0) {
        parent_->Mark(Dirty::Layout | Dirty::Paint | Dirty::Hit);
    } else if (parent_ && static_cast<u8>(dirty & Dirty::Paint) != 0) {
        parent_->Mark(Dirty::Paint);
    }
}

void Element::Clear(Dirty dirty) {
    dirty_ = static_cast<Dirty>(static_cast<u8>(dirty_) & ~static_cast<u8>(dirty));
}

void Element::ScrollBy(Point delta) {
    scroll_.x = std::max(0.0f, scroll_.x + delta.x);
    scroll_.y = std::max(0.0f, scroll_.y + delta.y);
    Mark(Dirty::Layout | Dirty::Paint | Dirty::Hit);
}

void Element::SetHovered(bool value) {
    if (hovered_ == value)
        return;
    hovered_ = value;
    Mark(Dirty::Paint);
}

void Element::SetPressed(bool value) {
    if (pressed_ == value)
        return;
    pressed_ = value;
    Mark(Dirty::Paint);
}

} // namespace woki::ui
