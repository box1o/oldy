#include <woki/events/events.hpp>

#include "extension_event_adapter.hpp"

namespace woki {
namespace {
template <typename T>
const T& As(const events::Event& event) noexcept {
    return static_cast<const T&>(event);
}

ext::ApplicationEventMetadata Metadata(const events::Event& event) noexcept {
    auto device = event.metadata.device;
    if (device == 0 && event.GetEventType() == events::EventType::kGamepadDisconnected)
        device = As<events::GamepadDisconnectedEvent>(event).device;
    if (device == 0 && event.GetEventType() == events::EventType::kGamepadConnected)
        device = As<events::GamepadConnectedEvent>(event).state.device;
    return {.timestamp = event.metadata.timestamp,
        .sequence = event.metadata.sequence,
        .window = event.metadata.window,
        .device = device,
        .modifiers = event.metadata.modifiers,
        .source = static_cast<u8>(event.metadata.source),
        .synthetic = event.metadata.source == events::EventSource::kSynthetic};
}

ext::ApplicationEventEncoder Encoder(const events::Event& event) {
    return {static_cast<ext::ApplicationEventType>(static_cast<u32>(event.GetEventType())), Metadata(event)};
}

void EncodePointer(ext::ApplicationEventEncoder& out, const events::PointerData& value) {
    out.U64(value.pointer);
    out.U8(static_cast<u8>(value.kind));
    out.U8(value.primary);
    out.U8(static_cast<u8>(value.button));
    out.Pad(1);
    out.U16(value.buttons);
    out.F32(value.x);
    out.F32(value.y);
    out.F32(value.delta_x);
    out.F32(value.delta_y);
    out.F32(value.pressure);
    out.F32(value.contact_width);
    out.F32(value.contact_height);
    out.F32(value.tilt_x);
    out.F32(value.tilt_y);
    out.F32(value.twist);
}

void EncodeGesture(ext::ApplicationEventEncoder& out, const events::GestureData& value) {
    out.U8(static_cast<u8>(value.phase));
    out.U8(value.pointer_count);
    out.Pad(2);
    out.F32(value.center_x);
    out.F32(value.center_y);
    out.F32(value.delta_x);
    out.F32(value.delta_y);
    out.F32(value.total_x);
    out.F32(value.total_y);
    out.F32(value.velocity_x);
    out.F32(value.velocity_y);
}
} // namespace

std::optional<ext::EncodedApplicationEvent> EncodeExtensionEvent(const events::Event& event) noexcept {
    auto out = Encoder(event);
    using enum events::EventType;
    switch (event.GetEventType()) {
        case kWindowClosed:
        case kWindowFocused:
        case kWindowLostFocus:
        case kWindowMinimized:
        case kWindowMaximized:
        case kWindowRestored:
        case kAppShutdown:
        case kAppSuspend:
        case kAppResume:
        case kWindowRefreshRequested:
            break;
        case kWindowResized: {
            const auto& v = As<events::WindowResizeEvent>(event);
            out.U32(v.width);
            out.U32(v.height);
            break;
        }
        case kFramebufferResized: {
            const auto& v = As<events::FramebufferResizeEvent>(event);
            out.U32(v.width);
            out.U32(v.height);
            break;
        }
        case kViewportResized: {
            const auto& v = As<events::ViewportResizeEvent>(event);
            out.U32(v.width);
            out.U32(v.height);
            break;
        }
        case kWindowMoved: {
            const auto& v = As<events::WindowMovedEvent>(event);
            out.I32(v.x);
            out.I32(v.y);
            break;
        }
        case kWindowScaleChanged: {
            const auto& v = As<events::WindowScaleChangedEvent>(event);
            out.F32(v.x);
            out.F32(v.y);
            break;
        }
        case kKeyPressed: {
            const auto& v = As<events::KeyPressedEvent>(event);
            out.U16(static_cast<u16>(v.key));
            out.I32(v.scan_code);
            out.U32(v.repeat_count);
            break;
        }
        case kKeyReleased: {
            const auto& v = As<events::KeyReleasedEvent>(event);
            out.U16(static_cast<u16>(v.key));
            out.I32(v.scan_code);
            break;
        }
        case kPointerMoved:
            EncodePointer(out, As<events::PointerMoveEvent>(event).pointer_data);
            break;
        case kPointerDown:
            EncodePointer(out, As<events::PointerDownEvent>(event).pointer_data);
            break;
        case kPointerUp:
            EncodePointer(out, As<events::PointerUpEvent>(event).pointer_data);
            break;
        case kPointerCancel:
            EncodePointer(out, As<events::PointerCancelEvent>(event).pointer_data);
            break;
        case kPointerEntered:
            EncodePointer(out, As<events::PointerEnterEvent>(event).pointer_data);
            break;
        case kPointerLeft:
            EncodePointer(out, As<events::PointerLeaveEvent>(event).pointer_data);
            break;
        case kScrolled: {
            const auto& v = As<events::ScrollEvent>(event);
            out.F32(v.delta_x);
            out.F32(v.delta_y);
            out.F32(v.x);
            out.F32(v.y);
            out.U8(static_cast<u8>(v.unit));
            out.U8(static_cast<u8>(v.phase));
            out.U8(static_cast<u8>(v.kind));
            out.U8(v.precise);
            break;
        }
        case kTextInput:
            out.String(As<events::TextInputEvent>(event).text);
            break;
        case kTextCompositionStarted: {
            const auto& v = As<events::TextCompositionStartedEvent>(event);
            out.U32(v.selection_start);
            out.U32(v.selection_length);
            out.String(v.text);
            break;
        }
        case kTextCompositionUpdated: {
            const auto& v = As<events::TextCompositionUpdatedEvent>(event);
            out.U32(v.selection_start);
            out.U32(v.selection_length);
            out.String(v.text);
            break;
        }
        case kTextCompositionCommitted: {
            const auto& v = As<events::TextCompositionCommittedEvent>(event);
            out.U32(v.selection_start);
            out.U32(v.selection_length);
            out.String(v.text);
            break;
        }
        case kTextCompositionCanceled: {
            const auto& v = As<events::TextCompositionCanceledEvent>(event);
            out.U32(v.selection_start);
            out.U32(v.selection_length);
            out.String(v.text);
            break;
        }
        case kTap:
            EncodeGesture(out, As<events::TapEvent>(event).gesture);
            break;
        case kDoubleTap:
            EncodeGesture(out, As<events::DoubleTapEvent>(event).gesture);
            break;
        case kLongPress:
            EncodeGesture(out, As<events::LongPressEvent>(event).gesture);
            break;
        case kPan:
            EncodeGesture(out, As<events::PanEvent>(event).gesture);
            break;
        case kPinch: {
            const auto& v = As<events::PinchEvent>(event);
            EncodeGesture(out, v.gesture);
            out.F32(v.scale_delta);
            out.F32(v.scale);
            break;
        }
        case kRotate: {
            const auto& v = As<events::RotateEvent>(event);
            EncodeGesture(out, v.gesture);
            out.F32(v.radians_delta);
            out.F32(v.radians);
            break;
        }
        case kGamepadConnected: {
            const auto& v = As<events::GamepadConnectedEvent>(event).state;
            out.U8(v.mapped);
            out.Pad(3);
            u16 buttons = 0;
            for (std::size_t i = 0; i < v.buttons.size(); ++i)
                if (v.buttons[i])
                    buttons |= static_cast<u16>(1u << i);
            out.U16(buttons);
            out.Pad(2);
            for (f32 axis : v.axes)
                out.F32(axis);
            out.U32(static_cast<u32>(v.name.size()));
            out.U32(static_cast<u32>(v.guid.size()));
            out.U16(static_cast<u16>(v.raw_axes.size()));
            out.U16(static_cast<u16>(v.raw_buttons.size()));
            out.U16(static_cast<u16>(v.raw_hats.size()));
            out.Pad(2);
            out.Bytes(v.name);
            out.Bytes(v.guid);
            for (f32 axis : v.raw_axes)
                out.F32(axis);
            for (bool button : v.raw_buttons)
                out.U8(button);
            for (u8 hat : v.raw_hats)
                out.U8(hat);
            break;
        }
        case kGamepadDisconnected:
            break;
        case kGamepadButtonChanged: {
            const auto& v = As<events::GamepadButtonChangedEvent>(event);
            out.U8(static_cast<u8>(v.button));
            out.U8(v.pressed);
            out.Pad(2);
            out.F32(v.value);
            break;
        }
        case kGamepadAxisChanged: {
            const auto& v = As<events::GamepadAxisChangedEvent>(event);
            out.U8(static_cast<u8>(v.axis));
            out.Pad(3);
            out.F32(v.value);
            break;
        }
        case kJoystickButtonChanged: {
            const auto& v = As<events::JoystickButtonChangedEvent>(event);
            out.U16(v.index);
            out.U8(v.pressed);
            out.Pad(1);
            break;
        }
        case kJoystickAxisChanged: {
            const auto& v = As<events::JoystickAxisChangedEvent>(event);
            out.U16(v.index);
            out.Pad(2);
            out.F32(v.value);
            break;
        }
        case kJoystickHatChanged: {
            const auto& v = As<events::JoystickHatChangedEvent>(event);
            out.U16(v.index);
            out.U8(v.value);
            out.Pad(1);
            break;
        }
        case kMonitorConnected:
            out.String(As<events::MonitorConnectedEvent>(event).name);
            break;
        case kMonitorDisconnected:
            out.String(As<events::MonitorDisconnectedEvent>(event).name);
            break;
        case kFilesDropped: {
            const auto& paths = As<events::FilesDroppedEvent>(event).paths;
            out.U32(static_cast<u32>(paths.size()));
            u32 size = 0;
            for (const auto& path : paths)
                size += static_cast<u32>(path.size() + 1);
            out.U32(size);
            for (const auto& path : paths) {
                out.Bytes(path);
                out.U8(0);
            }
            break;
        }
        case kPlatformError: {
            const auto& v = As<events::PlatformErrorEvent>(event);
            out.I32(v.code);
            out.String(v.description);
            break;
        }
        default:
            return std::nullopt;
    }
    return out.Finish();
}
} // namespace woki
