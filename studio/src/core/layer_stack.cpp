
#include <ranges>

#include "layer_stack.hpp"

namespace woki {

void LayerStack::PushLayer(scope<Layer> layer) {
    if (layer == nullptr) {
        return;
    }
    layers_.insert(layers_.begin() + static_cast<std::ptrdiff_t>(overlay_begin_), std::move(layer));
    ++overlay_begin_;
}

void LayerStack::PushOverlay(scope<Layer> overlay) {
    if (overlay == nullptr) {
        return;
    }
    layers_.push_back(std::move(overlay));
}

void LayerStack::AttachAll(Context& ctx) {
    if (attached_) {
        return;
    }
    for (const auto& layer : layers_) {
        layer->OnAttach(ctx);
    }
    attached_ = true;
}

void LayerStack::DetachAll(Context& ctx) noexcept {
    if (!attached_) {
        return;
    }

    for (auto& layer : std::views::reverse(layers_)) {
        try {
            layer->OnDetach(ctx);
        } catch (const std::exception& error) {
            slog::Error("Layer detach failed: {}", error.what());
        } catch (...) {
            slog::Error("Layer detach failed with an unknown error");
        }
    }
    attached_ = false;
}

void LayerStack::UpdateAll(Context& ctx, f64 delta_ms) {
    for (const auto& layer : layers_) {
        layer->OnUpdate(ctx, delta_ms);
    }
}

void LayerStack::DispatchEvent(Context& ctx, events::Event& event) {
    for (auto& layer : std::views::reverse(layers_)) {
        layer->OnEvent(ctx, event);
        if (event.handled) {
            break;
        }
    }
}

void LayerStack::DrawUi(Context& ctx) {
    for (const auto& layer : layers_) {
        layer->OnUi(ctx);
    }
}

bool LayerStack::Empty() const noexcept {
    return layers_.empty();
}

bool LayerStack::IsAttached() const noexcept {
    return attached_;
}

} // namespace woki
