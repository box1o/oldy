#pragma once

#include <string>
#include <vector>
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

struct FramebufferResizeEvent final : TypedEvent<EventType::kFramebufferResized, EventCategory::kWindow, EventCategory::kRender> {
    u32 width{0};
    u32 height{0};

    FramebufferResizeEvent(u32 width_value, u32 height_value)
        : width(width_value),
          height(height_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "FramebufferResized";
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

struct WindowRefreshEvent final : TypedEvent<EventType::kWindowRefreshRequested, EventCategory::kWindow> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "WindowRefreshRequested";
    }
};

struct FilesDroppedEvent final : TypedEvent<EventType::kFilesDropped, EventCategory::kWindow, EventCategory::kInput> {
    std::vector<std::string> paths;

    explicit FilesDroppedEvent(std::vector<std::string> values)
        : paths(std::move(values)) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "FilesDropped";
    }
};

struct MonitorConnectedEvent final : TypedEvent<EventType::kMonitorConnected, EventCategory::kDevice> {
    std::string name;

    explicit MonitorConnectedEvent(std::string value)
        : name(std::move(value)) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "MonitorConnected";
    }
};

struct MonitorDisconnectedEvent final : TypedEvent<EventType::kMonitorDisconnected, EventCategory::kDevice> {
    std::string name;

    explicit MonitorDisconnectedEvent(std::string value)
        : name(std::move(value)) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "MonitorDisconnected";
    }
};

struct PlatformErrorEvent final : TypedEvent<EventType::kPlatformError, EventCategory::kApplication> {
    i32 code{0};
    std::string description;

    PlatformErrorEvent(i32 value, std::string message)
        : code(value),
          description(std::move(message)) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "PlatformError";
    }
};

} // namespace woki::events
