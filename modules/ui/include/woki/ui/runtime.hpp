#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "frame.hpp"
#include "layout.hpp"

namespace woki::ui {

struct RuntimeStats {
    u32 reconciled{};
    u32 painted{};
    u32 culled{};
    LayoutStats layout{};
};

struct SemanticNode {
    Key key;
    Key parent;
    Rect bounds;
    Semantics semantics;
    bool visible{};
    bool focused{};
};

using SemanticSnapshot = std::vector<SemanticNode>;

class Runtime {
public:
    void SetContent(View root);
    bool HandleEvent(const Event& event);
    void Prepare(const Frame& frame);

    void SetTextEngine(const TextEngine* text) {
        layout_.SetTextEngine(text);
    }

    [[nodiscard]] const DisplayList& Display() const {
        return display_;
    }

    [[nodiscard]] const Element* Root() const {
        return root_.get();
    }

    [[nodiscard]] const RuntimeStats& Stats() const {
        return stats_;
    }

    [[nodiscard]] Color Background(const Element& element) const;
    [[nodiscard]] std::optional<Rect> Bounds(Key key) const;
    [[nodiscard]] bool Visible(Key key) const;
    [[nodiscard]] bool Focused(Key key) const;

    [[nodiscard]] const SemanticSnapshot& SemanticsSnapshot() const {
        return semantics_;
    }

    [[nodiscard]] bool HasTextOrModalFocus() const;
    void CancelCapture();

private:
    Element* Hit(Element& element, Point point);
    Element* Find(Element& element, Key key);
    bool Route(Element& target, const Event& event);
    void Paint(Element& element, Canvas& canvas, Rect viewport);
    void CollectFocus(Element& element, std::vector<Element*>& result);
    void FocusNext(bool reverse);
    void UpdateHover(Element& element, Point point);
    void Capture(u64 pointer, Element& target);
    void Release(u64 pointer);
    void ReleaseAll();
    [[nodiscard]] bool IsCaptured(const Element& element) const;
    void MarkPaint(Element& element);
    void SyncMotion(Element& element);
    void CollectKeys(Element& element, std::unordered_set<u64>& keys) const;
    void CollectSemantics(const Element& element, Key parent);
    const Element* Find(const Element& element, Key key) const;
    [[nodiscard]] u64 MotionKey(const Element& element) const;

    std::unique_ptr<Element> root_;
    Element* focused_{};
    std::unordered_map<u64, Element*> captured_;
    Point pointer_{};
    DisplayList display_;
    LayoutEngine layout_;
    RuntimeStats stats_{};
    Motion motion_;
    Motion::Clock::time_point time_{};
    std::unordered_map<u64, Color> backgrounds_;
    Size viewport_{};
    SemanticSnapshot semantics_;
};

} // namespace woki::ui
