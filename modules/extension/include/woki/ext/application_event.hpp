#pragma once

#include <span>
#include <array>
#include <string_view>

#include <woki/core.hpp>
#include <woki/ext/sdk/events.h>

namespace woki::ext {

enum class ApplicationEventType : u32 {
#define WOKI_EXT_EVENT(cpp_name, c_name, name, id, layout) cpp_name = WOKI_EXT_EVENT_##c_name,
#include <woki/ext/sdk/event_schema.def>
#undef WOKI_EXT_EVENT
};

[[nodiscard]] constexpr std::array<ApplicationEventType, 22> AllApplicationEventTypes() noexcept {
    using enum ApplicationEventType;
    return {WindowClosed, WindowResized, WindowFocused, WindowLostFocus, WindowMoved, WindowMinimized, WindowMaximized, WindowRestored, KeyPressed, KeyReleased, KeyTyped, MouseScrolled, MouseButtonPressed,
        MouseButtonReleased, MouseButtonClicked, MouseEntered, MouseLeft, WindowScaleChanged, ViewportResized, AppShutdown, AppSuspend, AppResume};
}

[[nodiscard]] std::string_view ToString(ApplicationEventType type) noexcept;
[[nodiscard]] Result<ApplicationEventType> ParseApplicationEventType(std::string_view name);

struct WindowClosedPayload {};

struct WindowResizedPayload {
    u32 width;
    u32 height;
};

struct WindowFocusedPayload {};

struct WindowLostFocusPayload {};

struct WindowMovedPayload {
    i32 x;
    i32 y;
};

struct WindowMinimizedPayload {};

struct WindowMaximizedPayload {};

struct WindowRestoredPayload {};

struct KeyPressedPayload {
    u16 key;
    u32 repeat_count;
};

struct KeyReleasedPayload {
    u16 key;
};

struct KeyTypedPayload {
    u32 character;
};

struct MouseScrolledPayload {
    f32 offset_x;
    f32 offset_y;
};

struct MouseButtonPressedPayload {
    u8 button;
    f32 x;
    f32 y;
};

struct MouseButtonReleasedPayload {
    u8 button;
    f32 x;
    f32 y;
};

struct MouseButtonClickedPayload {
    u8 button;
    f32 x;
    f32 y;
};

struct MouseEnteredPayload {};

struct MouseLeftPayload {};

struct WindowScaleChangedPayload {
    f32 x;
    f32 y;
};

struct ViewportResizedPayload {
    u32 width;
    u32 height;
};

struct AppShutdownPayload {};

struct AppSuspendPayload {};

struct AppResumePayload {};

struct EncodedApplicationEvent {
    ApplicationEventType type{};
    std::array<u8, 9> storage{};
    u8 size{};

    [[nodiscard]] std::span<const u8> Payload() const noexcept {
        return {storage.data(), size};
    }
};

[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(WindowClosedPayload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(WindowResizedPayload payload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(WindowFocusedPayload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(WindowLostFocusPayload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(WindowMovedPayload payload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(WindowMinimizedPayload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(WindowMaximizedPayload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(WindowRestoredPayload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(KeyPressedPayload payload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(KeyReleasedPayload payload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(KeyTypedPayload payload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(MouseScrolledPayload payload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(MouseButtonPressedPayload payload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(MouseButtonReleasedPayload payload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(MouseButtonClickedPayload payload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(MouseEnteredPayload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(MouseLeftPayload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(WindowScaleChangedPayload payload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(ViewportResizedPayload payload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(AppShutdownPayload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(AppSuspendPayload) noexcept;
[[nodiscard]] EncodedApplicationEvent EncodeApplicationEvent(AppResumePayload) noexcept;

[[nodiscard]] constexpr u32 ExtensionEventId(u32 local_id) noexcept {
    return WOKI_EXT_EVENT_EXTENSION_ID(local_id);
}

} // namespace woki::ext
