#pragma once

#include <vector>

#include "layer.hpp"

namespace woki {

class LayerStack final {
public:
    LayerStack() = default;
    ~LayerStack() = default;

    LayerStack(const LayerStack&) = delete;
    LayerStack& operator=(const LayerStack&) = delete;
    LayerStack(LayerStack&&) = delete;
    LayerStack& operator=(LayerStack&&) = delete;

    void PushLayer(scope<Layer> layer);
    void PushOverlay(scope<Layer> overlay);

    void AttachAll(Context& ctx);
    void DetachAll(Context& ctx) noexcept;
    void UpdateAll(Context& ctx, f64 delta_ms);
    void DispatchEvent(Context& ctx, events::Event& event);
    void DrawUi(Context& ctx);

    [[nodiscard]] bool Empty() const noexcept;
    [[nodiscard]] bool IsAttached() const noexcept;

private:
    std::vector<scope<Layer>> layers_;
    std::size_t overlay_begin_{0};
    bool attached_{false};
};

} // namespace woki
