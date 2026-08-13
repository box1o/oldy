#pragma once

#include <optional>

#include <woki/platform.hpp>
#include <woki/ui.hpp>

namespace woki::studio {

class PlatformUiAdapter final {
public:
    [[nodiscard]] std::optional<ui::Event> Convert(const events::Event& event);

    void Reset() noexcept {
        modifiers_ = 0;
    }

private:
    ui::Modifiers modifiers_{};
};

} // namespace woki::studio
