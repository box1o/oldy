#include <woki/events/events.hpp>

#include "render.hpp"

namespace woki {

void RenderLayer::OnAttach(Context& ctx) {
    WOKI_ASSERT(ctx.window != nullptr);
    if (!renderer_.Initialize(*ctx.window)) {
        slog::Warn("RenderLayer running without RHI renderer");
    }
}

void RenderLayer::OnDetach(Context& ctx) {
    (void)ctx;
    renderer_.Shutdown();
}

void RenderLayer::OnUpdate(Context& ctx, f64 delta_ms) {
    (void)ctx;

    if (!renderer_.IsReady()) {
        return;
    }

    if (!renderer_.RenderFrame(delta_ms)) {
        slog::Warn("Render frame failed");
    }
}

void RenderLayer::OnEvent(Context& ctx, events::Event& event) {
    (void)ctx;

    if (!renderer_.IsReady()) {
        return;
    }

    renderer_.HandleEvent(event);

    if (event.GetEventType() != events::EventType::kWindowResized) {
        return;
    }

    const auto& resize_event = static_cast<const events::WindowResizeEvent&>(event);
    renderer_.Resize(resize_event.width, resize_event.height);
}

} // namespace woki
