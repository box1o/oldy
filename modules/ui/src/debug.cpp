#include <woki/config.hpp>

#include <woki/ui/debug/export.hpp>

namespace woki::ui {

namespace {

const char* Name(Kind kind) {
    switch (kind) {
        case Kind::Box:
            return "box";
        case Kind::Text:
            return "text";
        case Kind::Component:
            return "component";
        case Kind::Portal:
            return "portal";
    }
    return "unknown";
}

void Encode(const Element& element, config::Writer& value, bool root = false) {
    const Rect bounds = element.Bounds();
    const NodeStats stats = element.Stats();
    value.BeginObject();
    if (root) {
        value.Key("$schema");
        value.String("https://schemas.woki.dev/ui.debug-tree/v1.schema.json");
    }
    value.Key("key");
    value.Unsigned(element.Identity().Value());
    value.Key("token");
    value.Unsigned(element.Token());
    value.Key("kind");
    value.String(Name(element.Type()));
    value.Key("content");
    value.String(element.Content());
    value.Key("bounds");
    value.BeginArray();
    value.Real(bounds.x);
    value.Real(bounds.y);
    value.Real(bounds.width);
    value.Real(bounds.height);
    value.EndArray();
    value.Key("dirty");
    value.Unsigned(static_cast<u8>(element.DirtyState()));
    value.Key("stats");
    value.BeginObject();
    value.Key("builds");
    value.Unsigned(stats.builds);
    value.Key("layouts");
    value.Unsigned(stats.layouts);
    value.Key("paints");
    value.Unsigned(stats.paints);
    value.EndObject();
    const auto& semantics = element.GetSemantics();
    value.Key("semantics");
    value.BeginObject();
    value.Key("role");
    value.Unsigned(static_cast<u8>(semantics.role));
    value.Key("label");
    value.String(semantics.label);
    value.Key("value");
    value.String(semantics.value);
    value.Key("focusable");
    value.Boolean(semantics.focusable);
    value.Key("disabled");
    value.Boolean(semantics.disabled);
    value.Key("selected");
    value.Boolean(semantics.selected);
    value.Key("checked");
    value.Boolean(semantics.checked);
    value.EndObject();
    value.Key("children");
    value.BeginArray();
    for (const auto& child : element.Children())
        Encode(*child, value);
    value.EndArray();
    value.EndObject();
}

} // namespace

std::string ExportTree(const Element& root) {
    config::Writer writer;
    Encode(root, writer, true);
    return writer.Str();
}

} // namespace woki::ui
