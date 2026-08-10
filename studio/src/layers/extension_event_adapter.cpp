#include <woki/events/events.hpp>

#include "extension_event_adapter.hpp"

namespace woki {
namespace {

template <typename T>
[[nodiscard]] const T& As(const events::Event& event) noexcept {
    return static_cast<const T&>(event);
}

} // namespace

std::optional<ext::EncodedApplicationEvent> EncodeExtensionEvent(const events::Event& event) noexcept {
    using enum events::EventType;
    switch (event.GetEventType()) {
        case kWindowClosed:
            return ext::EncodeApplicationEvent(ext::WindowClosedPayload{});
        case kWindowResized: {
            const auto& value = As<events::WindowResizeEvent>(event);
            return ext::EncodeApplicationEvent(ext::WindowResizedPayload{value.width, value.height});
        }
        case kWindowFocused:
            return ext::EncodeApplicationEvent(ext::WindowFocusedPayload{});
        case kWindowLostFocus:
            return ext::EncodeApplicationEvent(ext::WindowLostFocusPayload{});
        case kWindowMoved: {
            const auto& value = As<events::WindowMovedEvent>(event);
            return ext::EncodeApplicationEvent(ext::WindowMovedPayload{value.x, value.y});
        }
        case kWindowMinimized:
            return ext::EncodeApplicationEvent(ext::WindowMinimizedPayload{});
        case kWindowMaximized:
            return ext::EncodeApplicationEvent(ext::WindowMaximizedPayload{});
        case kWindowRestored:
            return ext::EncodeApplicationEvent(ext::WindowRestoredPayload{});
        case kKeyPressed: {
            const auto& value = As<events::KeyPressedEvent>(event);
            return ext::EncodeApplicationEvent(ext::KeyPressedPayload{static_cast<u16>(value.key), value.repeat_count});
        }
        case kKeyReleased: {
            const auto& value = As<events::KeyReleasedEvent>(event);
            return ext::EncodeApplicationEvent(ext::KeyReleasedPayload{static_cast<u16>(value.key)});
        }
        case kKeyTyped:
            return ext::EncodeApplicationEvent(ext::KeyTypedPayload{As<events::KeyTypedEvent>(event).character});
        case kMouseScrolled: {
            const auto& value = As<events::MouseScrolledEvent>(event);
            return ext::EncodeApplicationEvent(ext::MouseScrolledPayload{value.offset_x, value.offset_y});
        }
        case kMouseButtonPressed: {
            const auto& value = As<events::MouseButtonPressedEvent>(event);
            return ext::EncodeApplicationEvent(ext::MouseButtonPressedPayload{static_cast<u8>(value.button), value.x, value.y});
        }
        case kMouseButtonReleased: {
            const auto& value = As<events::MouseButtonReleasedEvent>(event);
            return ext::EncodeApplicationEvent(ext::MouseButtonReleasedPayload{static_cast<u8>(value.button), value.x, value.y});
        }
        case kMouseButtonClicked: {
            const auto& value = As<events::MouseButtonClickedEvent>(event);
            return ext::EncodeApplicationEvent(ext::MouseButtonClickedPayload{static_cast<u8>(value.button), value.x, value.y});
        }
        case kMouseEntered:
            return ext::EncodeApplicationEvent(ext::MouseEnteredPayload{});
        case kMouseLeft:
            return ext::EncodeApplicationEvent(ext::MouseLeftPayload{});
        case kWindowScaleChanged: {
            const auto& value = As<events::WindowScaleChangedEvent>(event);
            return ext::EncodeApplicationEvent(ext::WindowScaleChangedPayload{value.x, value.y});
        }
        case kViewportResized: {
            const auto& value = As<events::ViewportResizeEvent>(event);
            return ext::EncodeApplicationEvent(ext::ViewportResizedPayload{value.width, value.height});
        }
        case kAppShutdown:
            return ext::EncodeApplicationEvent(ext::AppShutdownPayload{});
        case kAppSuspend:
            return ext::EncodeApplicationEvent(ext::AppSuspendPayload{});
        case kAppResume:
            return ext::EncodeApplicationEvent(ext::AppResumePayload{});
        default:
            return std::nullopt;
    }
}

} // namespace woki
