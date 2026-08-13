#include <cmath>
#include <vector>
#include <algorithm>

#include <woki/ui/layout.hpp>

namespace woki::ui {

Size LayoutEngine::MeasureGrid(Element& element, Constraints constraints) {
    const Style& style = element.GetStyle();
    const u32 columns = std::max(1u, style.columns);
    const f32 finite_width = std::isfinite(constraints.max.width) ? constraints.max.width : 0.0f;
    const f32 cell_width = std::max(
        0.0f,
        (finite_width - style.gap * static_cast<f32>(columns - 1)) / static_cast<f32>(columns)
    );
    std::vector<f32> rows(1, 0.0f);
    u32 column{};
    f32 widest{};

    for (const auto& child : element.Children()) {
        if (child->GetStyle().position == Position::Absolute)
            continue;
        const u32 span = std::min(columns, std::max(1u, child->GetStyle().span));
        if (column + span > columns) {
            column = 0;
            rows.push_back(0.0f);
        }
        const f32 width = finite_width > 0.0f
                              ? cell_width * static_cast<f32>(span) + style.gap * static_cast<f32>(span - 1)
                              : constraints.max.width;
        const Size size = Measure(*child, {{}, {width, constraints.max.height}});
        rows.back() = std::max(rows.back(), size.height);
        widest = std::max(widest, size.width);
        column += span;
        if (column == columns) {
            column = 0;
            if (child.get() != element.Children().back().get())
                rows.push_back(0.0f);
        }
    }
    f32 height{};
    for (f32 row : rows)
        height += row;
    if (rows.size() > 1)
        height += style.gap * static_cast<f32>(rows.size() - 1);
    const f32 width = finite_width > 0.0f
                          ? finite_width
                          : widest * static_cast<f32>(columns) + style.gap * static_cast<f32>(columns - 1);
    return {width, height};
}

void LayoutEngine::PlaceGrid(Element& element, Rect content) {
    const Style& style = element.GetStyle();
    const u32 columns = std::max(1u, style.columns);
    const f32 cell_width = std::max(
        0.0f,
        (content.width - style.gap * static_cast<f32>(columns - 1)) / static_cast<f32>(columns)
    );
    u32 column{};
    f32 x = content.x;
    f32 y = content.y;
    f32 row_height{};

    for (auto& child : element.Children()) {
        if (child->GetStyle().position == Position::Absolute) {
            PlaceAbsolute(*child, content);
            continue;
        }
        const u32 span = std::min(columns, std::max(1u, child->GetStyle().span));
        if (column + span > columns) {
            column = 0;
            x = content.x;
            y += row_height + style.gap;
            row_height = 0.0f;
        }
        const f32 width = cell_width * static_cast<f32>(span) + style.gap * static_cast<f32>(span - 1);
        const f32 height = child->Measured().height;
        Place(*child, {x, y, width, height});
        row_height = std::max(row_height, height);
        column += span;
        x += width + style.gap;
        if (column == columns) {
            column = 0;
            x = content.x;
            y += row_height + style.gap;
            row_height = 0.0f;
        }
    }
}

} // namespace woki::ui
