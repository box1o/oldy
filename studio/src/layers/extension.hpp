#pragma once

#include <woki/ext/ext.hpp>

#include "core/layer.hpp"

namespace woki {

class ExtensionLayer final : public Layer {
public:
    void OnAttach(Context& ctx) override;
    void OnDetach(Context& ctx) override;
    void OnUpdate(Context& ctx, f64 delta_ms) override;
    void OnEvent(Context& ctx, events::Event& event) override;

private:
    void LoadInstalledExtensions();
    void LoadSourceExtensions();
    void ExecuteRegisteredCommands();
    void DispatchEventToExtensions(const events::Event& event);

    scope<ext::Manager> extensions_;
};

} // namespace woki
