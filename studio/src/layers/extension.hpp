#pragma once

#include <woki/ext/ext.hpp>

#include "core/layer.hpp"

namespace woki {

class StudioExtensionEventBus final : public ext::host::EventBus {
public:
    void Bind(ext::ExtensionManager* manager) noexcept;
    void Publish(const ext::host::Event& event) override;

private:
    ext::ExtensionManager* manager_{};
};

class ExtensionLayer final : public Layer {
public:
    void OnAttach(Context& ctx) override;
    void OnDetach(Context& ctx) override;
    void OnUpdate(Context& ctx, f64 delta_ms) override;
    void OnEvent(Context& ctx, events::Event& event) override;
    void ObserveEvent(Context& ctx, const events::Event& event) override;

private:
    void LoadInstalledExtensions();
    void LoadSourceExtensions();
    void ExecuteRegisteredCommands();
    void DispatchEventToExtensions(const events::Event& event);

    StudioExtensionEventBus event_bus_;
    scope<ext::ExtensionManager> extensions_;
};

} // namespace woki
