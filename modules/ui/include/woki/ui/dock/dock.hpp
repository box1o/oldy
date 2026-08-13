#pragma once

#include <memory>
#include <string>
#include <vector>
#include <expected>

#include "../key.hpp"
#include "../geometry.hpp"

namespace woki::ui {

enum class Axis : u8 { Horizontal, Vertical };

struct DockNode {
    enum class Type : u8 { Leaf, Split };

    u64 id{};
    Type type{Type::Leaf};
    Axis axis{Axis::Horizontal};
    f32 ratio{0.5f};
    f32 min_first{120.0f};
    f32 min_second{120.0f};
    Rect bounds{};
    std::vector<Key> tabs;
    size_t active{};
    std::unique_ptr<DockNode> first;
    std::unique_ptr<DockNode> second;

    [[nodiscard]] bool Leaf() const {
        return type == Type::Leaf;
    }
};

class Dock {
public:
    Dock();

    [[nodiscard]] DockNode& Root() {
        return *root_;
    }

    [[nodiscard]] const DockNode& Root() const {
        return *root_;
    }

    [[nodiscard]] DockNode* Find(u64 id);

    bool Add(u64 leaf, Key panel);
    bool Remove(Key panel);
    bool Split(u64 leaf, Axis axis, f32 ratio, Key panel, bool before = false);
    bool Activate(u64 leaf, size_t tab);
    bool Move(Key panel, u64 leaf, size_t index);
    bool SetRatio(u64 split, f32 ratio);
    void Layout(Rect bounds, f32 gap = 4.0f);
    void Collapse();

    [[nodiscard]] std::string Serialize() const;
    [[nodiscard]] static std::expected<Dock, std::string> Parse(std::string_view json);

private:
    explicit Dock(std::unique_ptr<DockNode> root);
    [[nodiscard]] u64 Next();

    std::unique_ptr<DockNode> root_;
    u64 next_{1};
};

} // namespace woki::ui
