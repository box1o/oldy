#pragma once

#include "tree.hpp"
#include "text/text.hpp"

namespace woki::ui {

struct LayoutStats {
    u32 measured{};
    u32 placed{};
    u32 cached{};
};

class LayoutEngine {
public:
    explicit LayoutEngine(const TextEngine* text = nullptr)
        : text_(text) {}

    void SetTextEngine(const TextEngine* text) {
        text_ = text;
    }

    void Run(Element& root, Rect viewport);

    [[nodiscard]] const LayoutStats& Stats() const {
        return stats_;
    }

private:
    Size Measure(Element& element, Constraints constraints);
    void Place(Element& element, Rect bounds);
    void PlaceFlow(Element& element, Rect content);
    void PlaceStack(Element& element, Rect content);
    Size MeasureGrid(Element& element, Constraints constraints);
    void PlaceGrid(Element& element, Rect content);
    void PlaceAbsolute(Element& child, Rect content);

    LayoutStats stats_{};
    const TextEngine* text_{};
};

} // namespace woki::ui
