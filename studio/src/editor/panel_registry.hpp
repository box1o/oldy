#pragma once

#include <functional>
#include <map>

#include <woki/ui.hpp>

namespace woki::studio {

class PanelRegistry final {
public:
    using Factory = std::function<ui::View()>;
    void Register(ui::Key key, std::string title, Factory factory);
    [[nodiscard]] ui::View Build(ui::Key key) const;
    [[nodiscard]] std::string_view Title(ui::Key key) const;

private:
    struct Entry {
        std::string title;
        Factory factory;
    };

    std::map<u64, Entry> entries_;
};

} // namespace woki::studio
