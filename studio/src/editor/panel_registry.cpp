#include "panel_registry.hpp"

namespace woki::studio {

void PanelRegistry::Register(ui::Key key, std::string title, Factory factory) {
    entries_.insert_or_assign(key.Value(), Entry{std::move(title), std::move(factory)});
}

ui::View PanelRegistry::Build(ui::Key key) const {
    const auto found = entries_.find(key.Value());
    return found == entries_.end() ? ui::Text("Missing panel") : found->second.factory();
}

std::string_view PanelRegistry::Title(ui::Key key) const {
    const auto found = entries_.find(key.Value());
    return found == entries_.end() ? std::string_view{"Unknown"} : found->second.title;
}

} // namespace woki::studio
