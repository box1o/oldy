#pragma once

#include <string_view>

#include "woki/events/base.hpp"

namespace woki::events {

struct FrameBeginEvent final : TypedEvent<EventType::kFrameBegin, EventCategory::kRender> {
    f32 delta_time{0.0f};

    explicit FrameBeginEvent(f32 delta_time_value)
        : delta_time(delta_time_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "FrameBegin";
    }
};

struct FrameEndEvent final : TypedEvent<EventType::kFrameEnd, EventCategory::kRender> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "FrameEnd";
    }
};

struct RenderBeginEvent final : TypedEvent<EventType::kRenderBegin, EventCategory::kRender> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "RenderBegin";
    }
};

struct RenderEndEvent final : TypedEvent<EventType::kRenderEnd, EventCategory::kRender> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "RenderEnd";
    }
};

struct ViewportResizeEvent final : TypedEvent<EventType::kViewportResized, EventCategory::kRender> {
    u32 width{0};
    u32 height{0};

    ViewportResizeEvent(u32 width_value, u32 height_value)
        : width(width_value),
          height(height_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "ViewportResized";
    }
};

struct SwapBuffersEvent final : TypedEvent<EventType::kSwapBuffers, EventCategory::kRender> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "SwapBuffers";
    }
};

} // namespace woki::events
