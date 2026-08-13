#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include "view.hpp"
#include "display.hpp"

namespace woki::ui {

enum class Dirty : u8 {
    None = 0,
    Build = 1 << 0,
    Layout = 1 << 1,
    Paint = 1 << 2,
    Semantics = 1 << 3,
    Hit = 1 << 4,
};

constexpr Dirty operator|(Dirty left, Dirty right) {
    return static_cast<Dirty>(static_cast<u8>(left) | static_cast<u8>(right));
}

constexpr Dirty operator&(Dirty left, Dirty right) {
    return static_cast<Dirty>(static_cast<u8>(left) & static_cast<u8>(right));
}

struct NodeStats {
    u32 builds{};
    u32 layouts{};
    u32 paints{};
};

class Element {
public:
    Element(View view, Element* parent = nullptr);

    void Update(View view);
    void Refresh();
    void Mark(Dirty dirty);
    void Clear(Dirty dirty);

    [[nodiscard]] bool Needs(Dirty dirty) const {
        return static_cast<u8>(dirty_ & dirty) != 0;
    }

    [[nodiscard]] Kind Type() const {
        return kind_;
    }

    [[nodiscard]] Key Identity() const {
        return key_;
    }

    [[nodiscard]] u64 Token() const {
        return token_;
    }

    [[nodiscard]] const Style& GetStyle() const {
        return style_;
    }

    [[nodiscard]] const Semantics& GetSemantics() const {
        return semantics_;
    }

    [[nodiscard]] const std::string& Content() const {
        return content_;
    }

    [[nodiscard]] const EventHandler& Handler() const {
        return handler_;
    }

    [[nodiscard]] ImageId ImageResource() const {
        return image_;
    }

    [[nodiscard]] ImageFit ImageSizing() const {
        return image_fit_;
    }

    [[nodiscard]] const std::vector<std::unique_ptr<Element>>& Children() const {
        return children_;
    }

    [[nodiscard]] std::vector<std::unique_ptr<Element>>& Children() {
        return children_;
    }

    [[nodiscard]] Element* Parent() const {
        return parent_;
    }

    [[nodiscard]] Rect Bounds() const {
        return bounds_;
    }

    [[nodiscard]] Size Measured() const {
        return measured_;
    }

    [[nodiscard]] const std::optional<Constraints>& LastConstraints() const {
        return constraints_;
    }

    [[nodiscard]] Point ScrollOffset() const {
        return scroll_;
    }

    [[nodiscard]] bool Hovered() const {
        return hovered_;
    }

    [[nodiscard]] bool Pressed() const {
        return pressed_;
    }

    [[nodiscard]] const NodeStats& Stats() const {
        return stats_;
    }

    [[nodiscard]] const std::vector<DisplayOp>& PaintCache() const {
        return paint_cache_;
    }

    [[nodiscard]] Dirty DirtyState() const {
        return dirty_;
    }

    void SetBounds(Rect bounds) {
        bounds_ = bounds;
    }

    void SetMeasured(Size size) {
        measured_ = size;
    }

    void SetConstraints(Constraints constraints) {
        constraints_ = constraints;
    }

    void ScrollBy(Point delta);
    void SetHovered(bool value);
    void SetPressed(bool value);

    void CountLayout() {
        ++stats_.layouts;
    }

    void CountPaint() {
        ++stats_.paints;
    }

    void SetPaintCache(std::vector<DisplayOp> cache) {
        paint_cache_ = std::move(cache);
    }

private:
    static inline std::atomic<u64> next_token_{1};

    Kind kind_;
    u64 token_{next_token_.fetch_add(1, std::memory_order_relaxed)};
    Key key_{};
    Style style_{};
    Semantics semantics_{};
    EventHandler handler_{};
    std::shared_ptr<Component> component_;
    u64 component_revision_{};
    std::string content_;
    ImageId image_{};
    ImageFit image_fit_{ImageFit::Fill};
    std::vector<std::unique_ptr<Element>> children_;
    Element* parent_{};
    Dirty dirty_{Dirty::Layout | Dirty::Paint | Dirty::Semantics | Dirty::Hit};
    Rect bounds_{};
    Size measured_{};
    std::optional<Constraints> constraints_;
    Point scroll_{};
    bool hovered_{};
    bool pressed_{};
    NodeStats stats_{};
    std::vector<DisplayOp> paint_cache_;
};

} // namespace woki::ui
