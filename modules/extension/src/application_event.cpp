#include <bit>
#include <string>
#include <algorithm>

#include "woki/ext/application_event.hpp"

namespace woki::ext {
namespace {

struct ApplicationEventName {
    ApplicationEventType type;
    std::string_view name;
};

constexpr std::array<ApplicationEventName, 22> kApplicationEventNames{{
    {ApplicationEventType::WindowClosed, "window.closed"},
    {ApplicationEventType::WindowResized, "window.resized"},
    {ApplicationEventType::WindowFocused, "window.focused"},
    {ApplicationEventType::WindowLostFocus, "window.lost-focus"},
    {ApplicationEventType::WindowMoved, "window.moved"},
    {ApplicationEventType::WindowMinimized, "window.minimized"},
    {ApplicationEventType::WindowMaximized, "window.maximized"},
    {ApplicationEventType::WindowRestored, "window.restored"},
    {ApplicationEventType::KeyPressed, "key.pressed"},
    {ApplicationEventType::KeyReleased, "key.released"},
    {ApplicationEventType::KeyTyped, "key.typed"},
    {ApplicationEventType::MouseScrolled, "mouse.scrolled"},
    {ApplicationEventType::MouseButtonPressed, "mouse-button.pressed"},
    {ApplicationEventType::MouseButtonReleased, "mouse-button.released"},
    {ApplicationEventType::MouseButtonClicked, "mouse-button.clicked"},
    {ApplicationEventType::MouseEntered, "mouse.entered"},
    {ApplicationEventType::MouseLeft, "mouse.left"},
    {ApplicationEventType::WindowScaleChanged, "window.scale-changed"},
    {ApplicationEventType::ViewportResized, "viewport.resized"},
    {ApplicationEventType::AppShutdown, "app.shutdown"},
    {ApplicationEventType::AppSuspend, "app.suspend"},
    {ApplicationEventType::AppResume, "app.resume"},
}};

void WriteU16(std::array<u8, 9>& out, std::size_t offset, u16 value) noexcept {
    out[offset] = static_cast<u8>(value);
    out[offset + 1] = static_cast<u8>(value >> 8u);
}

void WriteU32(std::array<u8, 9>& out, std::size_t offset, u32 value) noexcept {
    out[offset] = static_cast<u8>(value);
    out[offset + 1] = static_cast<u8>(value >> 8u);
    out[offset + 2] = static_cast<u8>(value >> 16u);
    out[offset + 3] = static_cast<u8>(value >> 24u);
}

void WriteF32(std::array<u8, 9>& out, std::size_t offset, f32 value) noexcept {
    WriteU32(out, offset, std::bit_cast<u32>(value));
}

EncodedApplicationEvent EncodeEmpty(ApplicationEventType type) noexcept {
    return {.type = type};
}

EncodedApplicationEvent EncodeSize(ApplicationEventType type, u32 width, u32 height) noexcept {
    EncodedApplicationEvent result{.type = type, .size = 8};
    WriteU32(result.storage, 0, width);
    WriteU32(result.storage, 4, height);
    return result;
}

} // namespace

std::string_view ToString(ApplicationEventType type) noexcept {
    const auto found = std::ranges::find(kApplicationEventNames, type, &ApplicationEventName::type);
    return found == kApplicationEventNames.end() ? "unknown" : found->name;
}

Result<ApplicationEventType> ParseApplicationEventType(std::string_view name) {
    const auto found = std::ranges::find(kApplicationEventNames, name, &ApplicationEventName::name);
    if (found == kApplicationEventNames.end())
        return Err(ErrorCode::ValidationInvalidState, "Unknown application activation event '" + std::string(name) + "'.");
    return Ok(found->type);
}

EncodedApplicationEvent EncodeApplicationEvent(WindowMovedPayload payload) noexcept {
    EncodedApplicationEvent result{.type = ApplicationEventType::WindowMoved, .size = 8};
    WriteU32(result.storage, 0, static_cast<u32>(payload.x));
    WriteU32(result.storage, 4, static_cast<u32>(payload.y));
    return result;
}

namespace {

EncodedApplicationEvent EncodeScale(ApplicationEventType type, f32 x, f32 y) noexcept {
    EncodedApplicationEvent result{.type = type, .size = 8};
    WriteF32(result.storage, 0, x);
    WriteF32(result.storage, 4, y);
    return result;
}

} // namespace

EncodedApplicationEvent EncodeApplicationEvent(KeyPressedPayload payload) noexcept {
    EncodedApplicationEvent result{.type = ApplicationEventType::KeyPressed, .size = 6};
    WriteU16(result.storage, 0, payload.key);
    WriteU32(result.storage, 2, payload.repeat_count);
    return result;
}

EncodedApplicationEvent EncodeApplicationEvent(KeyReleasedPayload payload) noexcept {
    EncodedApplicationEvent result{.type = ApplicationEventType::KeyReleased, .size = 2};
    WriteU16(result.storage, 0, payload.key);
    return result;
}

EncodedApplicationEvent EncodeApplicationEvent(KeyTypedPayload payload) noexcept {
    EncodedApplicationEvent result{.type = ApplicationEventType::KeyTyped, .size = 4};
    WriteU32(result.storage, 0, payload.character);
    return result;
}

EncodedApplicationEvent EncodeApplicationEvent(MouseScrolledPayload payload) noexcept {
    EncodedApplicationEvent result{.type = ApplicationEventType::MouseScrolled, .size = 8};
    WriteF32(result.storage, 0, payload.offset_x);
    WriteF32(result.storage, 4, payload.offset_y);
    return result;
}

namespace {

EncodedApplicationEvent EncodeMouseButton(ApplicationEventType type, u8 button, f32 x, f32 y) noexcept {
    EncodedApplicationEvent result{.type = type, .size = 9};
    result.storage[0] = button;
    WriteF32(result.storage, 1, x);
    WriteF32(result.storage, 5, y);
    return result;
}

} // namespace

#define WOKI_ENCODE_EMPTY(payload_type, event_type)                                                                                                                                                                        \
    EncodedApplicationEvent EncodeApplicationEvent(payload_type) noexcept {                                                                                                                                                \
        return EncodeEmpty(ApplicationEventType::event_type);                                                                                                                                                              \
    }

WOKI_ENCODE_EMPTY(WindowClosedPayload, WindowClosed)
WOKI_ENCODE_EMPTY(WindowFocusedPayload, WindowFocused)
WOKI_ENCODE_EMPTY(WindowLostFocusPayload, WindowLostFocus)
WOKI_ENCODE_EMPTY(WindowMinimizedPayload, WindowMinimized)
WOKI_ENCODE_EMPTY(WindowMaximizedPayload, WindowMaximized)
WOKI_ENCODE_EMPTY(WindowRestoredPayload, WindowRestored)
WOKI_ENCODE_EMPTY(MouseEnteredPayload, MouseEntered)
WOKI_ENCODE_EMPTY(MouseLeftPayload, MouseLeft)
WOKI_ENCODE_EMPTY(AppShutdownPayload, AppShutdown)
WOKI_ENCODE_EMPTY(AppSuspendPayload, AppSuspend)
WOKI_ENCODE_EMPTY(AppResumePayload, AppResume)

#undef WOKI_ENCODE_EMPTY

EncodedApplicationEvent EncodeApplicationEvent(WindowResizedPayload payload) noexcept {
    return EncodeSize(ApplicationEventType::WindowResized, payload.width, payload.height);
}

EncodedApplicationEvent EncodeApplicationEvent(ViewportResizedPayload payload) noexcept {
    return EncodeSize(ApplicationEventType::ViewportResized, payload.width, payload.height);
}

EncodedApplicationEvent EncodeApplicationEvent(WindowScaleChangedPayload payload) noexcept {
    return EncodeScale(ApplicationEventType::WindowScaleChanged, payload.x, payload.y);
}

EncodedApplicationEvent EncodeApplicationEvent(MouseButtonPressedPayload payload) noexcept {
    return EncodeMouseButton(ApplicationEventType::MouseButtonPressed, payload.button, payload.x, payload.y);
}

EncodedApplicationEvent EncodeApplicationEvent(MouseButtonReleasedPayload payload) noexcept {
    return EncodeMouseButton(ApplicationEventType::MouseButtonReleased, payload.button, payload.x, payload.y);
}

EncodedApplicationEvent EncodeApplicationEvent(MouseButtonClickedPayload payload) noexcept {
    return EncodeMouseButton(ApplicationEventType::MouseButtonClicked, payload.button, payload.x, payload.y);
}

} // namespace woki::ext
