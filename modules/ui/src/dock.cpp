#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>
#include <woki/config.hpp>

#include <woki/ui/dock/dock.hpp>

namespace woki::ui {

namespace {

DockNode* FindNode(DockNode* node, u64 id) {
    if (!node || node->id == id)
        return node;
    if (auto* found = FindNode(node->first.get(), id))
        return found;
    return FindNode(node->second.get(), id);
}

void LayoutNode(DockNode& node, Rect bounds, f32 gap) {
    node.bounds = bounds;
    if (node.Leaf() || !node.first || !node.second)
        return;
    const f32 ratio = std::clamp(node.ratio, 0.05f, 0.95f);
    if (node.axis == Axis::Horizontal) {
        const f32 available = std::max(0.0f, bounds.width - gap);
        const f32 lower = std::min(node.min_first, available);
        const f32 upper = std::max(lower, available - node.min_second);
        const f32 first = std::clamp(available * ratio, lower, upper);
        LayoutNode(*node.first, {bounds.x, bounds.y, first, bounds.height}, gap);
        LayoutNode(*node.second, {bounds.x + first + gap, bounds.y, available - first, bounds.height}, gap);
    } else {
        const f32 available = std::max(0.0f, bounds.height - gap);
        const f32 lower = std::min(node.min_first, available);
        const f32 upper = std::max(lower, available - node.min_second);
        const f32 first = std::clamp(available * ratio, lower, upper);
        LayoutNode(*node.first, {bounds.x, bounds.y, bounds.width, first}, gap);
        LayoutNode(*node.second, {bounds.x, bounds.y + first + gap, bounds.width, available - first}, gap);
    }
}

bool RemovePanel(DockNode& node, Key panel) {
    const auto before = node.tabs.size();
    std::erase(node.tabs, panel);
    node.active = node.tabs.empty() ? 0 : std::min(node.active, node.tabs.size() - 1);
    return before != node.tabs.size() || (node.first && RemovePanel(*node.first, panel))
           || (node.second && RemovePanel(*node.second, panel));
}

void CollapseNode(DockNode& node) {
    if (node.Leaf())
        return;
    CollapseNode(*node.first);
    CollapseNode(*node.second);
    const bool first_empty = node.first->Leaf() && node.first->tabs.empty();
    const bool second_empty = node.second->Leaf() && node.second->tabs.empty();
    if (first_empty != second_empty) {
        auto keep = first_empty ? std::move(node.second) : std::move(node.first);
        node = std::move(*keep);
    }
}

void Encode(const DockNode& node, config::Writer& value) {
    value.BeginObject();
    value.Key("id");
    value.Unsigned(node.id);
    value.Key("type");
    value.String(node.Leaf() ? "leaf" : "split");
    if (node.Leaf()) {
        value.Key("active");
        value.Unsigned(node.active);
        value.Key("tabs");
        value.BeginArray();
        for (Key tab : node.tabs)
            value.Unsigned(tab.Value());
        value.EndArray();
    } else {
        value.Key("axis");
        value.String(node.axis == Axis::Horizontal ? "horizontal" : "vertical");
        value.Key("ratio");
        value.Real(node.ratio);
        value.Key("minFirst");
        value.Real(node.min_first);
        value.Key("minSecond");
        value.Real(node.min_second);
        value.Key("first");
        Encode(*node.first, value);
        value.Key("second");
        Encode(*node.second, value);
    }
    value.EndObject();
}

std::unique_ptr<DockNode> Decode(
    const config::Json& value,
    std::unordered_set<u64>& ids,
    std::unordered_set<u64>& tabs,
    u64& next,
    size_t& nodes,
    u32 depth
) {
    if (!value.is_object() || depth > 32 || ++nodes > 4096)
        throw std::runtime_error("dock document exceeds its object/depth policy");
    auto node = std::make_unique<DockNode>();
    node->id = value["id"].get<u64>();
    if (node->id == 0 || !ids.insert(node->id).second)
        throw std::runtime_error("dock IDs must be unique and non-zero");
    next = std::max(next, node->id + 1);
    const std::string type = value["type"].get<std::string>();
    if (type == "leaf") {
        constexpr std::array<std::string_view, 4> keys{"id", "type", "active", "tabs"};
        for (const auto& [key, unused] : value.items()) {
            static_cast<void>(unused);
            if (std::ranges::find(keys, key) == keys.end())
                throw std::runtime_error("leaf contains unknown field: " + key);
        }
        if (!value["tabs"].is_array() || value["tabs"].size() > 1024)
            throw std::runtime_error("dock tabs must be a bounded array");
        for (config::Json item : value["tabs"]) {
            const u64 tab = item.get<u64>();
            if (tab == 0)
                throw std::runtime_error("panel keys must be non-zero");
            if (!tabs.insert(tab).second)
                throw std::runtime_error("panel keys must not be duplicated across dock leaves");
            node->tabs.emplace_back(tab);
        }
        node->active = value.value("active", 0u);
        if (!node->tabs.empty() && node->active >= node->tabs.size())
            throw std::runtime_error("active tab is out of range");
    } else if (type == "split") {
        constexpr std::array<std::string_view, 8>
            keys{"id", "type", "axis", "ratio", "minFirst", "minSecond", "first", "second"};
        for (const auto& [key, unused] : value.items()) {
            static_cast<void>(unused);
            if (std::ranges::find(keys, key) == keys.end())
                throw std::runtime_error("split contains unknown field: " + key);
        }
        node->type = DockNode::Type::Split;
        if (value["axis"] != "horizontal" && value["axis"] != "vertical")
            throw std::runtime_error("split axis must be horizontal or vertical");
        node->axis = value["axis"] == "horizontal" ? Axis::Horizontal : Axis::Vertical;
        node->ratio = value["ratio"].get<f32>();
        if (!std::isfinite(node->ratio) || node->ratio <= 0.0f || node->ratio >= 1.0f)
            throw std::runtime_error("split ratio must be between zero and one");
        node->min_first = value.value("minFirst", 120.0f);
        node->min_second = value.value("minSecond", 120.0f);
        if (!std::isfinite(node->min_first) || !std::isfinite(node->min_second) || node->min_first < 0
            || node->min_second < 0)
            throw std::runtime_error("split minimum sizes must be finite and non-negative");
        node->first = Decode(value["first"], ids, tabs, next, nodes, depth + 1);
        node->second = Decode(value["second"], ids, tabs, next, nodes, depth + 1);
    } else {
        throw std::runtime_error("unknown dock node type");
    }
    return node;
}

} // namespace

Dock::Dock()
    : root_(std::make_unique<DockNode>()) {
    root_->id = Next();
}

Dock::Dock(std::unique_ptr<DockNode> root)
    : root_(std::move(root)) {}

u64 Dock::Next() {
    return next_++;
}

DockNode* Dock::Find(u64 id) {
    return FindNode(root_.get(), id);
}

bool Dock::Add(u64 leaf, Key panel) {
    DockNode* node = Find(leaf);
    if (!node || !node->Leaf() || !panel)
        return false;
    Remove(panel);
    node->tabs.push_back(panel);
    node->active = node->tabs.size() - 1;
    return true;
}

bool Dock::Remove(Key panel) {
    return RemovePanel(*root_, panel);
}

bool Dock::Split(u64 leaf, Axis axis, f32 ratio, Key panel, bool before) {
    DockNode* node = Find(leaf);
    if (!node || !node->Leaf() || !panel || !std::isfinite(ratio) || ratio <= 0.0f || ratio >= 1.0f)
        return false;
    Remove(panel);
    auto existing = std::make_unique<DockNode>(std::move(*node));
    existing->id = Next();
    auto added = std::make_unique<DockNode>();
    added->id = Next();
    added->tabs.push_back(panel);
    node->id = leaf;
    node->type = DockNode::Type::Split;
    node->axis = axis;
    node->ratio = ratio;
    node->tabs.clear();
    node->active = 0;
    node->first = before ? std::move(added) : std::move(existing);
    node->second = before ? std::move(existing) : std::move(added);
    return true;
}

bool Dock::Activate(u64 leaf, size_t tab) {
    DockNode* node = Find(leaf);
    if (!node || !node->Leaf() || tab >= node->tabs.size())
        return false;
    node->active = tab;
    return true;
}

bool Dock::Move(Key panel, u64 leaf, size_t index) {
    DockNode* destination = Find(leaf);
    if (!destination || !destination->Leaf() || !panel)
        return false;
    Remove(panel);
    index = std::min(index, destination->tabs.size());
    destination->tabs.insert(destination->tabs.begin() + static_cast<ptrdiff_t>(index), panel);
    destination->active = index;
    return true;
}

bool Dock::SetRatio(u64 split, f32 ratio) {
    DockNode* node = Find(split);
    if (!node || node->Leaf() || !std::isfinite(ratio) || ratio <= 0 || ratio >= 1)
        return false;
    node->ratio = ratio;
    return true;
}

void Dock::Layout(Rect bounds, f32 gap) {
    LayoutNode(*root_, bounds, std::max(0.0f, gap));
}

void Dock::Collapse() {
    CollapseNode(*root_);
}

std::string Dock::Serialize() const {
    config::Writer writer(false);
    writer.BeginObject();
    writer.Key("$schema");
    writer.String("https://schemas.woki.dev/ui.dock/v1.schema.json");
    writer.Key("schema");
    writer.Unsigned(1);
    writer.Key("name");
    writer.String("ui.dock");
    writer.Key("root");
    Encode(*root_, writer);
    writer.EndObject();
    return writer.Str();
}

std::expected<Dock, std::string> Dock::Parse(std::string_view json) {
    try {
        auto parsed = config::Json::Parse(json, "ui.dock");
        if (!parsed)
            return std::unexpected(config::FormatDiagnostics(parsed.error()));
        if (!parsed->is_object() || parsed->value("schema", 0u) != 1 || parsed->value("name", "") != "ui.dock")
            return std::unexpected("dock document requires schema 1 and name ui.dock");
        constexpr std::array<std::string_view, 4> root_keys{"$schema", "schema", "name", "root"};
        for (const auto& [key, unused] : parsed->items()) {
            static_cast<void>(unused);
            if (std::ranges::find(root_keys, key) == root_keys.end())
                return std::unexpected("dock document contains unknown field: " + key);
        }
        std::unordered_set<u64> ids;
        std::unordered_set<u64> tabs;
        u64 next = 1;
        size_t nodes = 0;
        const config::Json root_value = parsed->contains("root") ? (*parsed)["root"] : *parsed;
        auto root = Decode(root_value, ids, tabs, next, nodes, 0);
        Dock dock{std::move(root)};
        dock.next_ = next;
        return dock;
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}

} // namespace woki::ui
