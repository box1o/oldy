#pragma once

#include <array>
#include <string>
#include <string_view>

#include <woki/core.hpp>
#include "woki/events/window/events.hpp"
#include "woki/events/gamepad/events.hpp"
#include "woki/events/gesture/events.hpp"
#include "woki/events/renderer/events.hpp"
#include "woki/events/application/events.hpp"

namespace woki::events {

[[nodiscard]] constexpr std::string_view ToString(EventType type) noexcept {
    switch (type) {
#define WOKI_EVENT_NAME(value, name)                                                                                                                                                                                       \
    case EventType::value:                                                                                                                                                                                                 \
        return name
        WOKI_EVENT_NAME(kNone, "None");
        WOKI_EVENT_NAME(kWindowClosed, "WindowClosed");
        WOKI_EVENT_NAME(kWindowResized, "WindowResized");
        WOKI_EVENT_NAME(kWindowFocused, "WindowFocused");
        WOKI_EVENT_NAME(kWindowLostFocus, "WindowLostFocus");
        WOKI_EVENT_NAME(kWindowMoved, "WindowMoved");
        WOKI_EVENT_NAME(kWindowMinimized, "WindowMinimized");
        WOKI_EVENT_NAME(kWindowMaximized, "WindowMaximized");
        WOKI_EVENT_NAME(kWindowRestored, "WindowRestored");
        WOKI_EVENT_NAME(kFramebufferResized, "FramebufferResized");
        WOKI_EVENT_NAME(kKeyPressed, "KeyPressed");
        WOKI_EVENT_NAME(kKeyReleased, "KeyReleased");
        WOKI_EVENT_NAME(kPointerMoved, "PointerMoved");
        WOKI_EVENT_NAME(kScrolled, "Scrolled");
        WOKI_EVENT_NAME(kPointerDown, "PointerDown");
        WOKI_EVENT_NAME(kPointerUp, "PointerUp");
        WOKI_EVENT_NAME(kPointerCancel, "PointerCancel");
        WOKI_EVENT_NAME(kPointerEntered, "PointerEntered");
        WOKI_EVENT_NAME(kPointerLeft, "PointerLeft");
        WOKI_EVENT_NAME(kWindowScaleChanged, "WindowScaleChanged");
        WOKI_EVENT_NAME(kTextInput, "TextInput");
        WOKI_EVENT_NAME(kTextCompositionStarted, "TextCompositionStarted");
        WOKI_EVENT_NAME(kTextCompositionUpdated, "TextCompositionUpdated");
        WOKI_EVENT_NAME(kTextCompositionCommitted, "TextCompositionCommitted");
        WOKI_EVENT_NAME(kTextCompositionCanceled, "TextCompositionCanceled");
        WOKI_EVENT_NAME(kTap, "Tap");
        WOKI_EVENT_NAME(kDoubleTap, "DoubleTap");
        WOKI_EVENT_NAME(kLongPress, "LongPress");
        WOKI_EVENT_NAME(kPan, "Pan");
        WOKI_EVENT_NAME(kPinch, "Pinch");
        WOKI_EVENT_NAME(kRotate, "Rotate");
        WOKI_EVENT_NAME(kFrameBegin, "FrameBegin");
        WOKI_EVENT_NAME(kFrameEnd, "FrameEnd");
        WOKI_EVENT_NAME(kRenderBegin, "RenderBegin");
        WOKI_EVENT_NAME(kRenderEnd, "RenderEnd");
        WOKI_EVENT_NAME(kViewportResized, "ViewportResized");
        WOKI_EVENT_NAME(kSwapBuffers, "SwapBuffers");
        WOKI_EVENT_NAME(kAppTick, "AppTick");
        WOKI_EVENT_NAME(kAppUpdate, "AppUpdate");
        WOKI_EVENT_NAME(kAppRender, "AppRender");
        WOKI_EVENT_NAME(kAppShutdown, "AppShutdown");
        WOKI_EVENT_NAME(kAppSuspend, "AppSuspend");
        WOKI_EVENT_NAME(kAppResume, "AppResume");
        WOKI_EVENT_NAME(kGamepadConnected, "GamepadConnected");
        WOKI_EVENT_NAME(kGamepadDisconnected, "GamepadDisconnected");
        WOKI_EVENT_NAME(kGamepadButtonChanged, "GamepadButtonChanged");
        WOKI_EVENT_NAME(kGamepadAxisChanged, "GamepadAxisChanged");
        WOKI_EVENT_NAME(kJoystickButtonChanged, "JoystickButtonChanged");
        WOKI_EVENT_NAME(kJoystickAxisChanged, "JoystickAxisChanged");
        WOKI_EVENT_NAME(kJoystickHatChanged, "JoystickHatChanged");
        WOKI_EVENT_NAME(kMonitorConnected, "MonitorConnected");
        WOKI_EVENT_NAME(kMonitorDisconnected, "MonitorDisconnected");
        WOKI_EVENT_NAME(kFilesDropped, "FilesDropped");
        WOKI_EVENT_NAME(kWindowRefreshRequested, "WindowRefreshRequested");
        WOKI_EVENT_NAME(kPlatformError, "PlatformError");
        WOKI_EVENT_NAME(kCustom, "Custom");
#undef WOKI_EVENT_NAME
    }
    return "Unknown";
}

[[nodiscard]] inline std::string CategoryFlagsToString(u16 flags) {
    struct Entry {
        EventCategory category;
        std::string_view name;
    };

    constexpr std::array entries{
        Entry{EventCategory::kWindow, "Window"},
        Entry{EventCategory::kInput, "Input"},
        Entry{EventCategory::kKeyboard, "Keyboard"},
        Entry{EventCategory::kPointer, "Pointer"},
        Entry{EventCategory::kGesture, "Gesture"},
        Entry{EventCategory::kRender, "Render"},
        Entry{EventCategory::kApplication, "Application"},
        Entry{EventCategory::kGamepad, "Gamepad"},
        Entry{EventCategory::kText, "Text"},
        Entry{EventCategory::kDevice, "Device"},
    };
    std::string result;
    for (const auto& entry : entries) {
        if ((flags & entry.category) == 0)
            continue;
        if (!result.empty())
            result += '|';
        result += entry.name;
    }
    return result.empty() ? "None" : result;
}

[[nodiscard]] constexpr bool ShouldForwardToExtensions(EventType type) noexcept {
    switch (type) {
        case EventType::kAppTick:
        case EventType::kAppUpdate:
        case EventType::kAppRender:
        case EventType::kPointerMoved:
        case EventType::kFrameBegin:
        case EventType::kFrameEnd:
        case EventType::kRenderBegin:
        case EventType::kRenderEnd:
        case EventType::kSwapBuffers:
            return false;
        default:
            return type != EventType::kNone;
    }
}

[[nodiscard]] inline std::string ToJson(const Event& event) {
    std::string output = "{\"type\":\"" + std::string(ToString(event.GetEventType())) + "\",\"sequence\":" + std::to_string(event.metadata.sequence);
    output += ",\"timestamp\":" + std::to_string(event.metadata.timestamp);
    output += ",\"window\":" + std::to_string(event.metadata.window) + ",\"device\":" + std::to_string(event.metadata.device);
    output += ",\"handled\":";
    output += event.handled ? "true" : "false";
    output += '}';
    return output;
}

[[nodiscard]] inline std::string ToString(const Event& event) {
    std::string output(event.GetName());
    output += " [category=" + CategoryFlagsToString(event.GetCategoryFlags());
    output += ", handled=";
    output += event.handled ? "true" : "false";
    output += ", sequence=" + std::to_string(event.metadata.sequence) + ']';
    return output;
}

inline void LogEvent(const Event& event) {
    slog::Debug("Event: {}", ToString(event));
}

} // namespace woki::events
