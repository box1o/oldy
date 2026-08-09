#pragma once

#include <string>
#include <string_view>

#include <woki/core.hpp>
#include "woki/events/base.hpp"
#include "woki/events/type.hpp"
#include "woki/events/category.hpp"
#include "woki/events/input/events.hpp"
#include "woki/events/window/events.hpp"
#include "woki/events/renderer/events.hpp"
#include "woki/events/application/events.hpp"

namespace woki::events {

namespace detail {

[[nodiscard]] inline bool ShouldLogAsDebug(EventType type) noexcept {
    switch (type) {
        case EventType::kFrameBegin:
        case EventType::kFrameEnd:
        case EventType::kRenderBegin:
        case EventType::kRenderEnd:
        case EventType::kSwapBuffers:
        case EventType::kAppTick:
        case EventType::kAppUpdate:
        case EventType::kAppRender:
        case EventType::kMouseMoved:
        case EventType::kMouseScrolled:
        case EventType::kMouseButtonPressed:
        case EventType::kMouseButtonReleased:
        case EventType::kMouseButtonClicked:
        case EventType::kMouseEntered:
        case EventType::kMouseLeft:
        case EventType::kKeyPressed:
        case EventType::kKeyReleased:
        case EventType::kKeyTyped:
            return true;
        default:
            return false;
    }
}

inline void AppendEventPayload(std::string& output, const Event& event) {
    switch (event.GetEventType()) {
        case EventType::kWindowResized: {
            const auto* typed_event = dynamic_cast<const WindowResizeEvent*>(&event);
            if (typed_event != nullptr) {
                output += " (" + std::to_string(typed_event->width) + "x" + std::to_string(typed_event->height) + ")";
            }
            return;
        }
        case EventType::kWindowMoved: {
            const auto* typed_event = dynamic_cast<const WindowMovedEvent*>(&event);
            if (typed_event != nullptr) {
                output += " (x=" + std::to_string(typed_event->x) + ", y=" + std::to_string(typed_event->y) + ")";
            }
            return;
        }
        case EventType::kWindowScaleChanged: {
            const auto* typed_event = dynamic_cast<const WindowScaleChangedEvent*>(&event);
            if (typed_event != nullptr) {
                output += " (x=" + std::to_string(typed_event->x) + ", y=" + std::to_string(typed_event->y) + ")";
            }
            return;
        }
        case EventType::kKeyPressed: {
            const auto* typed_event = dynamic_cast<const KeyPressedEvent*>(&event);
            if (typed_event != nullptr) {
                output += " (key=" + std::to_string(static_cast<u16>(typed_event->key)) + ", repeat=" + std::to_string(typed_event->repeat_count) + ")";
            }
            return;
        }
        case EventType::kKeyReleased: {
            const auto* typed_event = dynamic_cast<const KeyReleasedEvent*>(&event);
            if (typed_event != nullptr) {
                output += " (key=" + std::to_string(static_cast<u16>(typed_event->key)) + ")";
            }
            return;
        }
        case EventType::kKeyTyped: {
            const auto* typed_event = dynamic_cast<const KeyTypedEvent*>(&event);
            if (typed_event != nullptr) {
                output += " (character=" + std::to_string(typed_event->character) + ")";
            }
            return;
        }
        case EventType::kMouseMoved: {
            const auto* typed_event = dynamic_cast<const MouseMovedEvent*>(&event);
            if (typed_event != nullptr) {
                output += " (x=" + std::to_string(typed_event->x) + ", y=" + std::to_string(typed_event->y) + ", dx=" + std::to_string(typed_event->delta_x) + ", dy=" + std::to_string(typed_event->delta_y) + ")";
            }
            return;
        }
        case EventType::kMouseScrolled: {
            const auto* typed_event = dynamic_cast<const MouseScrolledEvent*>(&event);
            if (typed_event != nullptr) {
                output += " (x=" + std::to_string(typed_event->offset_x) + ", y=" + std::to_string(typed_event->offset_y) + ")";
            }
            return;
        }
        case EventType::kMouseButtonPressed: {
            const auto* typed_event = dynamic_cast<const MouseButtonPressedEvent*>(&event);
            if (typed_event != nullptr) {
                output += " (button=" + std::to_string(static_cast<u8>(typed_event->button)) + ", x=" + std::to_string(typed_event->x) + ", y=" + std::to_string(typed_event->y) + ")";
            }
            return;
        }
        case EventType::kMouseButtonReleased: {
            const auto* typed_event = dynamic_cast<const MouseButtonReleasedEvent*>(&event);
            if (typed_event != nullptr) {
                output += " (button=" + std::to_string(static_cast<u8>(typed_event->button)) + ", x=" + std::to_string(typed_event->x) + ", y=" + std::to_string(typed_event->y) + ")";
            }
            return;
        }
        case EventType::kMouseButtonClicked: {
            const auto* typed_event = dynamic_cast<const MouseButtonClickedEvent*>(&event);
            if (typed_event != nullptr) {
                output += " (button=" + std::to_string(static_cast<u8>(typed_event->button)) + ", x=" + std::to_string(typed_event->x) + ", y=" + std::to_string(typed_event->y) + ")";
            }
            return;
        }
        case EventType::kFrameBegin: {
            const auto* typed_event = dynamic_cast<const FrameBeginEvent*>(&event);
            if (typed_event != nullptr) {
                output += " (dt=" + std::to_string(typed_event->delta_time) + ")";
            }
            return;
        }
        case EventType::kViewportResized: {
            const auto* typed_event = dynamic_cast<const ViewportResizeEvent*>(&event);
            if (typed_event != nullptr) {
                output += " (" + std::to_string(typed_event->width) + "x" + std::to_string(typed_event->height) + ")";
            }
            return;
        }
        case EventType::kAppTick: {
            const auto* typed_event = dynamic_cast<const AppTickEvent*>(&event);
            if (typed_event != nullptr) {
                output += " (dt=" + std::to_string(typed_event->delta_time) + ")";
            }
            return;
        }
        case EventType::kAppUpdate: {
            const auto* typed_event = dynamic_cast<const AppUpdateEvent*>(&event);
            if (typed_event != nullptr) {
                output += " (dt=" + std::to_string(typed_event->delta_time) + ")";
            }
            return;
        }
        default:
            return;
    }
}

} // namespace detail

[[nodiscard]] constexpr std::string_view ToString(EventType type) noexcept {
    switch (type) {
        case EventType::kNone:
            return "None";
        case EventType::kWindowClosed:
            return "WindowClosed";
        case EventType::kWindowResized:
            return "WindowResized";
        case EventType::kWindowFocused:
            return "WindowFocused";
        case EventType::kWindowLostFocus:
            return "WindowLostFocus";
        case EventType::kWindowMoved:
            return "WindowMoved";
        case EventType::kWindowMinimized:
            return "WindowMinimized";
        case EventType::kWindowMaximized:
            return "WindowMaximized";
        case EventType::kWindowRestored:
            return "WindowRestored";
        case EventType::kKeyPressed:
            return "KeyPressed";
        case EventType::kKeyReleased:
            return "KeyReleased";
        case EventType::kKeyTyped:
            return "KeyTyped";
        case EventType::kMouseMoved:
            return "MouseMoved";
        case EventType::kMouseScrolled:
            return "MouseScrolled";
        case EventType::kMouseButtonPressed:
            return "MouseButtonPressed";
        case EventType::kMouseButtonReleased:
            return "MouseButtonReleased";
        case EventType::kMouseButtonClicked:
            return "MouseButtonClicked";
        case EventType::kMouseEntered:
            return "MouseEntered";
        case EventType::kMouseLeft:
            return "MouseLeft";
        case EventType::kWindowScaleChanged:
            return "WindowScaleChanged";
        case EventType::kFrameBegin:
            return "FrameBegin";
        case EventType::kFrameEnd:
            return "FrameEnd";
        case EventType::kRenderBegin:
            return "RenderBegin";
        case EventType::kRenderEnd:
            return "RenderEnd";
        case EventType::kViewportResized:
            return "ViewportResized";
        case EventType::kSwapBuffers:
            return "SwapBuffers";
        case EventType::kAppTick:
            return "AppTick";
        case EventType::kAppUpdate:
            return "AppUpdate";
        case EventType::kAppRender:
            return "AppRender";
        case EventType::kAppShutdown:
            return "AppShutdown";
        case EventType::kAppSuspend:
            return "AppSuspend";
        case EventType::kAppResume:
            return "AppResume";
        case EventType::kCustom:
            return "Custom";
    }

    return "Unknown";
}

[[nodiscard]] inline std::string CategoryFlagsToString(u16 flags) {
    if (flags == static_cast<u16>(EventCategory::kNone)) {
        return "None";
    }

    struct Entry {
        EventCategory category;
        std::string_view name;
    };

    constexpr Entry entries[] = {
        {EventCategory::kWindow, "Window"},
        {EventCategory::kInput, "Input"},
        {EventCategory::kKeyboard, "Keyboard"},
        {EventCategory::kMouse, "Mouse"},
        {EventCategory::kMouseButton, "MouseButton"},
        {EventCategory::kRender, "Render"},
        {EventCategory::kApplication, "Application"},
        {EventCategory::kScene, "Scene"},
        {EventCategory::kPhysics, "Physics"},
        {EventCategory::kAudio, "Audio"},
        {EventCategory::kCustom, "Custom"},
    };

    std::string output;
    bool first = true;
    for (const auto& entry : entries) {
        if ((flags & entry.category) == 0) {
            continue;
        }

        if (!first) {
            output += '|';
        }

        output += entry.name;
        first = false;
    }

    return output.empty() ? std::string{"None"} : output;
}

[[nodiscard]] constexpr bool ShouldForwardToExtensions(EventType type) noexcept {
    switch (type) {
        case EventType::kAppTick:
        case EventType::kAppUpdate:
        case EventType::kAppRender:
        case EventType::kMouseMoved:
        case EventType::kFrameBegin:
        case EventType::kFrameEnd:
        case EventType::kRenderBegin:
        case EventType::kRenderEnd:
        case EventType::kSwapBuffers:
            return false;
        default:
            return true;
    }
}

namespace detail {

inline void AppendJsonString(std::string& output, std::string_view value) {
    output.push_back('"');
    for (const char ch : value) {
        switch (ch) {
            case '"':
                output += "\\\"";
                break;
            case '\\':
                output += "\\\\";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                output.push_back(ch);
                break;
        }
    }
    output.push_back('"');
}

inline void AppendJsonKey(std::string& output, std::string_view key) {
    if (!output.empty() && output.back() != '{') {
        output.push_back(',');
    }
    AppendJsonString(output, key);
    output.push_back(':');
}

template <typename T>
void AppendMouseButtonJson(std::string& output, const Event& event) {
    const auto* typed_event = dynamic_cast<const T*>(&event);
    if (typed_event == nullptr) {
        return;
    }

    AppendJsonKey(output, "button");
    output += std::to_string(static_cast<u8>(typed_event->button));
    AppendJsonKey(output, "x");
    output += std::to_string(typed_event->x);
    AppendJsonKey(output, "y");
    output += std::to_string(typed_event->y);
}

} // namespace detail

[[nodiscard]] inline std::string ToJson(const Event& event) {
    std::string output{"{"};
    detail::AppendJsonKey(output, "type");
    detail::AppendJsonString(output, ToString(event.GetEventType()));

    switch (event.GetEventType()) {
        case EventType::kKeyPressed: {
            const auto* typed_event = dynamic_cast<const KeyPressedEvent*>(&event);
            if (typed_event == nullptr) {
                break;
            }
            detail::AppendJsonKey(output, "key");
            output += std::to_string(static_cast<u16>(typed_event->key));
            detail::AppendJsonKey(output, "repeat");
            output += std::to_string(typed_event->repeat_count);
            break;
        }
        case EventType::kKeyReleased: {
            const auto* typed_event = dynamic_cast<const KeyReleasedEvent*>(&event);
            if (typed_event == nullptr) {
                break;
            }
            detail::AppendJsonKey(output, "key");
            output += std::to_string(static_cast<u16>(typed_event->key));
            break;
        }
        case EventType::kKeyTyped: {
            const auto* typed_event = dynamic_cast<const KeyTypedEvent*>(&event);
            if (typed_event == nullptr) {
                break;
            }
            detail::AppendJsonKey(output, "character");
            output += std::to_string(typed_event->character);
            break;
        }
        case EventType::kWindowResized: {
            const auto* typed_event = dynamic_cast<const WindowResizeEvent*>(&event);
            if (typed_event == nullptr) {
                break;
            }
            detail::AppendJsonKey(output, "width");
            output += std::to_string(typed_event->width);
            detail::AppendJsonKey(output, "height");
            output += std::to_string(typed_event->height);
            break;
        }
        case EventType::kWindowMoved: {
            const auto* typed_event = dynamic_cast<const WindowMovedEvent*>(&event);
            if (typed_event == nullptr) {
                break;
            }
            detail::AppendJsonKey(output, "x");
            output += std::to_string(typed_event->x);
            detail::AppendJsonKey(output, "y");
            output += std::to_string(typed_event->y);
            break;
        }
        case EventType::kWindowScaleChanged: {
            const auto* typed_event = dynamic_cast<const WindowScaleChangedEvent*>(&event);
            if (typed_event == nullptr) {
                break;
            }
            detail::AppendJsonKey(output, "x");
            output += std::to_string(typed_event->x);
            detail::AppendJsonKey(output, "y");
            output += std::to_string(typed_event->y);
            break;
        }
        case EventType::kMouseMoved: {
            const auto* typed_event = dynamic_cast<const MouseMovedEvent*>(&event);
            if (typed_event == nullptr) {
                break;
            }
            detail::AppendJsonKey(output, "x");
            output += std::to_string(typed_event->x);
            detail::AppendJsonKey(output, "y");
            output += std::to_string(typed_event->y);
            detail::AppendJsonKey(output, "deltaX");
            output += std::to_string(typed_event->delta_x);
            detail::AppendJsonKey(output, "deltaY");
            output += std::to_string(typed_event->delta_y);
            break;
        }
        case EventType::kMouseScrolled: {
            const auto* typed_event = dynamic_cast<const MouseScrolledEvent*>(&event);
            if (typed_event == nullptr) {
                break;
            }
            detail::AppendJsonKey(output, "offsetX");
            output += std::to_string(typed_event->offset_x);
            detail::AppendJsonKey(output, "offsetY");
            output += std::to_string(typed_event->offset_y);
            break;
        }
        case EventType::kMouseButtonPressed: {
            detail::AppendMouseButtonJson<MouseButtonPressedEvent>(output, event);
            break;
        }
        case EventType::kMouseButtonReleased: {
            detail::AppendMouseButtonJson<MouseButtonReleasedEvent>(output, event);
            break;
        }
        case EventType::kMouseButtonClicked: {
            detail::AppendMouseButtonJson<MouseButtonClickedEvent>(output, event);
            break;
        }
        case EventType::kFrameBegin: {
            const auto* typed_event = dynamic_cast<const FrameBeginEvent*>(&event);
            if (typed_event == nullptr) {
                break;
            }
            detail::AppendJsonKey(output, "deltaTime");
            output += std::to_string(typed_event->delta_time);
            break;
        }
        case EventType::kViewportResized: {
            const auto* typed_event = dynamic_cast<const ViewportResizeEvent*>(&event);
            if (typed_event == nullptr) {
                break;
            }
            detail::AppendJsonKey(output, "width");
            output += std::to_string(typed_event->width);
            detail::AppendJsonKey(output, "height");
            output += std::to_string(typed_event->height);
            break;
        }
        case EventType::kAppTick: {
            const auto* typed_event = dynamic_cast<const AppTickEvent*>(&event);
            if (typed_event == nullptr) {
                break;
            }
            detail::AppendJsonKey(output, "deltaTime");
            output += std::to_string(typed_event->delta_time);
            break;
        }
        case EventType::kAppUpdate: {
            const auto* typed_event = dynamic_cast<const AppUpdateEvent*>(&event);
            if (typed_event == nullptr) {
                break;
            }
            detail::AppendJsonKey(output, "deltaTime");
            output += std::to_string(typed_event->delta_time);
            break;
        }
        default:
            break;
    }

    output.push_back('}');
    return output;
}

[[nodiscard]] inline std::string ToString(const Event& event) {
    std::string output(event.GetName());
    output += " [category=";
    output += CategoryFlagsToString(event.GetCategoryFlags());
    output += ", handled=";
    output += event.handled ? "true" : "false";
    output += "]";

    detail::AppendEventPayload(output, event);

    return output;
}

inline void LogEvent(const Event& event) {
    const auto message = ToString(event);

    if (detail::ShouldLogAsDebug(event.GetEventType())) {
        slog::Debug("Event: {}", message);
        return;
    }

    slog::Info("Event: {}", message);
}

} // namespace woki::events
