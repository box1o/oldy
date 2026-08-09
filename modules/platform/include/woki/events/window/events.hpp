#pragma once

#include <string_view>

#include "woki/events/base.hpp"

namespace woki::events {

struct WindowCloseEvent final : TypedEvent<EventType::kWindowClosed, EventCategory::kWindow> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "WindowClosed";
    }
};

struct WindowFocusEvent final : TypedEvent<EventType::kWindowFocused, EventCategory::kWindow> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "WindowFocused";
    }
};

struct WindowLostFocusEvent final : TypedEvent<EventType::kWindowLostFocus, EventCategory::kWindow> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "WindowLostFocus";
    }
};

struct WindowResizeEvent final : TypedEvent<EventType::kWindowResized, EventCategory::kWindow> {
    u32 width{0};
    u32 height{0};

    WindowResizeEvent(u32 width_value, u32 height_value)
        : width(width_value),
          height(height_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "WindowResized";
    }
};

struct WindowMovedEvent final : TypedEvent<EventType::kWindowMoved, EventCategory::kWindow> {
    i32 x{0};
    i32 y{0};

    WindowMovedEvent(i32 x_value, i32 y_value)
        : x(x_value),
          y(y_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "WindowMoved";
    }
};

struct WindowScaleChangedEvent final : TypedEvent<EventType::kWindowScaleChanged, EventCategory::kWindow> {
    f32 x{1.0f};
    f32 y{1.0f};

    WindowScaleChangedEvent(f32 x_value, f32 y_value)
        : x(x_value),
          y(y_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "WindowScaleChanged";
    }
};

struct WindowMinimizedEvent final : TypedEvent<EventType::kWindowMinimized, EventCategory::kWindow> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "WindowMinimized";
    }
};

struct WindowMaximizedEvent final : TypedEvent<EventType::kWindowMaximized, EventCategory::kWindow> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "WindowMaximized";
    }
};

struct WindowRestoredEvent final : TypedEvent<EventType::kWindowRestored, EventCategory::kWindow> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "WindowRestored";
    }
};

} // namespace woki::events
