#include <cmath>
#include <algorithm>

#include <woki/ui/layout.hpp>

namespace woki::ui {

namespace {

f32 Main(Size size, Flow flow) {
    return flow == Flow::Row ? size.width : size.height;
}

f32 Cross(Size size, Flow flow) {
    return flow == Flow::Row ? size.height : size.width;
}

f32 MainMargin(const Inset& margin, Flow flow) {
    return flow == Flow::Row ? margin.left + margin.right : margin.top + margin.bottom;
}

f32 CrossMargin(const Inset& margin, Flow flow) {
    return flow == Flow::Row ? margin.top + margin.bottom : margin.left + margin.right;
}

Size WithAxes(f32 main, f32 cross, Flow flow) {
    return flow == Flow::Row ? Size{main, cross} : Size{cross, main};
}

Constraints ChildConstraints(const Style& style, Constraints parent) {
    return {
        .min = {style.min_width, style.min_height},
        .max =
            {
                std::min(parent.max.width, style.max_width),
                std::min(parent.max.height, style.max_height),
            },
    };
}

} // namespace

void LayoutEngine::Run(Element& root, Rect viewport) {
    stats_ = {};
    Measure(root, {{}, {viewport.width, viewport.height}});
    Place(root, viewport);
}

Size LayoutEngine::Measure(Element& element, Constraints constraints) {
    if (!element.Needs(Dirty::Layout) && element.LastConstraints() == constraints) {
        ++stats_.cached;
        return element.Measured();
    }

    ++stats_.measured;
    const Style& style = element.GetStyle();
    const f32 horizontal = style.padding.left + style.padding.right;
    const f32 vertical = style.padding.top + style.padding.bottom;
    Size intrinsic{};

    for (const auto& child : element.Children()) {
        if (child->GetStyle().position == Position::Absolute) {
            Measure(*child, ChildConstraints(child->GetStyle(), constraints));
        }
    }

    if (element.Type() == Kind::Text) {
        static const SimpleText fallback;
        const TextEngine& text = text_ ? *text_ : fallback;
        intrinsic = text.Measure(
            element.Content(),
            constraints.max.width,
            {.size = style.font_size, .line = style.line_height}
        );
    } else if (style.flow == Flow::Grid) {
        intrinsic = MeasureGrid(element, constraints);
    } else if (style.flow == Flow::Stack) {
        for (const auto& child : element.Children()) {
            if (child->GetStyle().position == Position::Absolute) {
                continue;
            }
            const Size size = Measure(*child, ChildConstraints(child->GetStyle(), constraints));
            intrinsic.width = std::max(intrinsic.width, size.width);
            intrinsic.height = std::max(intrinsic.height, size.height);
        }
    } else {
        f32 main{};
        f32 cross{};
        u32 count{};
        for (const auto& child : element.Children()) {
            if (child->GetStyle().position == Position::Absolute) {
                continue;
            }
            const Size size = Measure(*child, ChildConstraints(child->GetStyle(), constraints));
            main += Main(size, style.flow) + MainMargin(child->GetStyle().margin, style.flow);
            cross = std::max(cross, Cross(size, style.flow) + CrossMargin(child->GetStyle().margin, style.flow));
            ++count;
        }
        if (count > 1) {
            main += style.gap * static_cast<f32>(count - 1);
        }
        intrinsic = WithAxes(main, cross, style.flow);
    }

    intrinsic.width += horizontal;
    intrinsic.height += vertical;
    const f32 available_width = std::isfinite(constraints.max.width) ? constraints.max.width : intrinsic.width;
    const f32 available_height = std::isfinite(constraints.max.height) ? constraints.max.height : intrinsic.height;
    Size measured{
        style.width.Resolve(available_width, intrinsic.width),
        style.height.Resolve(available_height, intrinsic.height),
    };
    if (style.aspect > 0.0f) {
        if (style.width.IsAuto() && !style.height.IsAuto()) {
            measured.width = measured.height * style.aspect;
        } else {
            measured.height = measured.width / style.aspect;
        }
    }
    measured = ChildConstraints(style, constraints).Clamp(measured);
    element.SetMeasured(measured);
    element.SetConstraints(constraints);
    element.Clear(Dirty::Layout);
    element.CountLayout();
    return measured;
}

void LayoutEngine::Place(Element& element, Rect bounds) {
    ++stats_.placed;
    element.SetBounds(bounds);
    Rect content = InsetRect(bounds, element.GetStyle().padding);
    content.x -= element.ScrollOffset().x;
    content.y -= element.ScrollOffset().y;
    if (element.GetStyle().flow == Flow::Grid) {
        PlaceGrid(element, content);
    } else if (element.GetStyle().flow == Flow::Stack) {
        PlaceStack(element, content);
    } else {
        PlaceFlow(element, content);
    }
    element.Clear(Dirty::Hit);
}

void LayoutEngine::PlaceFlow(Element& element, Rect content) {
    const Style& style = element.GetStyle();
    const f32 available_main = style.flow == Flow::Row ? content.width : content.height;
    f32 fixed{};
    f32 grow{};
    u32 count{};

    for (const auto& child : element.Children()) {
        if (child->GetStyle().position == Position::Absolute) {
            continue;
        }
        const Length& length = style.flow == Flow::Row ? child->GetStyle().width : child->GetStyle().height;
        fixed += MainMargin(child->GetStyle().margin, style.flow);
        if (length.IsGrow()) {
            grow += length.Factor();
        } else {
            fixed += Main(child->Measured(), style.flow);
        }
        ++count;
    }
    fixed += count > 1 ? style.gap * static_cast<f32>(count - 1) : 0.0f;
    const f32 remaining = std::max(0.0f, available_main - fixed);
    const f32 free = grow > 0.0f ? 0.0f : std::max(0.0f, available_main - fixed);
    f32 between = style.gap;
    f32 offset{};
    if (style.justify == Justify::Center)
        offset = free * 0.5f;
    if (style.justify == Justify::End)
        offset = free;
    if (style.justify == Justify::SpaceBetween && count > 1)
        between += free / static_cast<f32>(count - 1);
    if (style.justify == Justify::SpaceAround && count > 0) {
        between += free / static_cast<f32>(count);
        offset = between * 0.5f;
    }
    f32 cursor = (style.flow == Flow::Row ? content.x : content.y) + offset;

    for (auto& child : element.Children()) {
        if (child->GetStyle().position == Position::Absolute) {
            PlaceAbsolute(*child, content);
            continue;
        }
        Size size = child->Measured();
        const Inset margin = child->GetStyle().margin;
        const Length& length = style.flow == Flow::Row ? child->GetStyle().width : child->GetStyle().height;
        if (length.IsGrow() && grow > 0.0f) {
            if (style.flow == Flow::Row)
                size.width = remaining * length.Factor() / grow;
            else
                size.height = remaining * length.Factor() / grow;
        }
        if (style.align == Align::Stretch) {
            if (style.flow == Flow::Row)
                size.height = std::max(0.0f, content.height - CrossMargin(margin, style.flow));
            else
                size.width = std::max(0.0f, content.width - CrossMargin(margin, style.flow));
        }
        const f32 main_before = style.flow == Flow::Row ? margin.left : margin.top;
        const f32 cross_before = style.flow == Flow::Row ? margin.top : margin.left;
        f32 cross = (style.flow == Flow::Row ? content.y : content.x) + cross_before;
        const f32 room = (style.flow == Flow::Row ? content.height : content.width) - Cross(size, style.flow)
                         - CrossMargin(margin, style.flow);
        if (style.align == Align::Center)
            cross += room * 0.5f;
        if (style.align == Align::End)
            cross += room;
        cursor += main_before;
        Rect bounds = style.flow == Flow::Row ? Rect{cursor, cross, size.width, size.height}
                                              : Rect{cross, cursor, size.width, size.height};
        Place(*child, bounds);
        cursor += Main(size, style.flow) + (style.flow == Flow::Row ? margin.right : margin.bottom) + between;
    }
}

void LayoutEngine::PlaceStack(Element& element, Rect content) {
    const Style& style = element.GetStyle();
    for (auto& child : element.Children()) {
        if (child->GetStyle().position == Position::Absolute) {
            PlaceAbsolute(*child, content);
        } else {
            const Inset margin = child->GetStyle().margin;
            const Rect available = InsetRect(content, margin);
            Size size = child->Measured();
            if (style.align == Align::Stretch)
                size.width = available.width;

            f32 x = available.x;
            f32 y = available.y;
            if (style.align == Align::Center)
                x += (available.width - size.width) * 0.5f;
            else if (style.align == Align::End)
                x += available.width - size.width;
            if (style.justify == Justify::Center)
                y += (available.height - size.height) * 0.5f;
            else if (style.justify == Justify::End)
                y += available.height - size.height;
            Place(*child, {x, y, size.width, size.height});
        }
    }
}

void LayoutEngine::PlaceAbsolute(Element& child, Rect content) {
    const Style& style = child.GetStyle();
    Size size = child.Measured();
    if (style.left && style.right)
        size.width = std::max(0.0f, content.width - *style.left - *style.right);
    if (style.top && style.bottom)
        size.height = std::max(0.0f, content.height - *style.top - *style.bottom);
    f32 x = content.x;
    f32 y = content.y;
    if (style.left)
        x += *style.left;
    else if (style.right)
        x += content.width - *style.right - size.width;
    if (style.top)
        y += *style.top;
    else if (style.bottom)
        y += content.height - *style.bottom - size.height;
    if (style.center_x)
        x = content.x + (content.width - size.width) * 0.5f;
    if (style.center_y)
        y = content.y + (content.height - size.height) * 0.5f;
    Place(child, {x, y, size.width, size.height});
}

} // namespace woki::ui
