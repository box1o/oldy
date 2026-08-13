#include "platform_ui_adapter.hpp"

namespace woki::studio {
namespace {
ui::PointerKind Kind(events::PointerKind value) {
    switch (value) {
        case events::PointerKind::kMouse:
            return ui::PointerKind::Mouse;
        case events::PointerKind::kTouch:
            return ui::PointerKind::Touch;
        case events::PointerKind::kPen:
            return ui::PointerKind::Pen;
        default:
            return ui::PointerKind::Unknown;
    }
}

ui::PointerButton Button(events::PointerButton value) {
    switch (value) {
        case events::PointerButton::kPrimary:
            return ui::PointerButton::Primary;
        case events::PointerButton::kSecondary:
            return ui::PointerButton::Secondary;
        case events::PointerButton::kMiddle:
            return ui::PointerButton::Middle;
        default:
            return ui::PointerButton::None;
    }
}

ui::KeyCode Key(events::KeyCode value) {
    switch (value) {
        case events::KeyCode::kTab:
            return ui::KeyCode::Tab;
        case events::KeyCode::kEnter:
            return ui::KeyCode::Enter;
        case events::KeyCode::kSpace:
            return ui::KeyCode::Space;
        case events::KeyCode::kEscape:
            return ui::KeyCode::Escape;
        case events::KeyCode::kLeft:
            return ui::KeyCode::Left;
        case events::KeyCode::kRight:
            return ui::KeyCode::Right;
        case events::KeyCode::kUp:
            return ui::KeyCode::Up;
        case events::KeyCode::kDown:
            return ui::KeyCode::Down;
        case events::KeyCode::kHome:
            return ui::KeyCode::Home;
        case events::KeyCode::kEnd:
            return ui::KeyCode::End;
        case events::KeyCode::kBackspace:
            return ui::KeyCode::Backspace;
        case events::KeyCode::kDelete:
            return ui::KeyCode::Delete;
        case events::KeyCode::kF:
            return ui::KeyCode::F;
        case events::KeyCode::kR:
            return ui::KeyCode::R;
        case events::KeyCode::kMinus:
            return ui::KeyCode::Minus;
        case events::KeyCode::kEqual:
            return ui::KeyCode::Equal;
        default:
            return ui::KeyCode::Unknown;
    }
}

ui::GestureEvent::Phase Phase(events::GesturePhase value) {
    switch (value) {
        case events::GesturePhase::kUpdate:
            return ui::GestureEvent::Phase::Update;
        case events::GesturePhase::kEnd:
            return ui::GestureEvent::Phase::End;
        case events::GesturePhase::kCancel:
            return ui::GestureEvent::Phase::Cancel;
        default:
            return ui::GestureEvent::Phase::Begin;
    }
}
} // namespace

std::optional<ui::Event> PlatformUiAdapter::Convert(const events::Event& event) {
    const auto pointer = [&](const events::PointerData& data, ui::PointerEvent::Type type) -> ui::Event {
        return ui::PointerEvent{.type = type,
            .position = {data.x, data.y},
            .delta = {data.delta_x, data.delta_y},
            .button = Button(data.button),
            .kind = Kind(data.kind),
            .buttons = data.buttons,
            .modifiers = modifiers_,
            .pressure = data.pressure,
            .contact_width = data.contact_width,
            .contact_height = data.contact_height,
            .tilt_x = data.tilt_x,
            .tilt_y = data.tilt_y,
            .pointer = data.pointer};
    };
    switch (event.GetEventType()) {
        case events::EventType::kPointerDown:
            return pointer(static_cast<const events::PointerDownEvent&>(event).pointer_data, ui::PointerEvent::Type::Down);
        case events::EventType::kPointerMoved:
            return pointer(static_cast<const events::PointerMoveEvent&>(event).pointer_data, ui::PointerEvent::Type::Move);
        case events::EventType::kPointerUp:
            return pointer(static_cast<const events::PointerUpEvent&>(event).pointer_data, ui::PointerEvent::Type::Up);
        case events::EventType::kPointerCancel:
            return pointer(static_cast<const events::PointerCancelEvent&>(event).pointer_data, ui::PointerEvent::Type::Cancel);
        case events::EventType::kScrolled: {
            const auto& value = static_cast<const events::ScrollEvent&>(event);
            return ui::PointerEvent{.type = ui::PointerEvent::Type::Wheel, .position = {value.x, value.y}, .delta = {value.delta_x, value.delta_y}, .kind = Kind(value.kind), .modifiers = modifiers_};
        }
        case events::EventType::kKeyPressed:
        case events::EventType::kKeyReleased: {
            const bool pressed = event.GetEventType() == events::EventType::kKeyPressed;
            const auto key = pressed ? static_cast<const events::KeyPressedEvent&>(event).key : static_cast<const events::KeyReleasedEvent&>(event).key;
            const auto set = [&](events::KeyCode left, events::KeyCode right, ui::Modifier modifier) {
                if (key == left || key == right) {
                    if (pressed)
                        modifiers_ |= static_cast<ui::Modifiers>(modifier);
                    else
                        modifiers_ &= ~static_cast<ui::Modifiers>(modifier);
                }
            };
            set(events::KeyCode::kLeftShift, events::KeyCode::kRightShift, ui::Modifier::Shift);
            set(events::KeyCode::kLeftControl, events::KeyCode::kRightControl, ui::Modifier::Control);
            set(events::KeyCode::kLeftAlt, events::KeyCode::kRightAlt, ui::Modifier::Alt);
            set(events::KeyCode::kLeftSuper, events::KeyCode::kRightSuper, ui::Modifier::Super);
            const u32 repeat = pressed ? static_cast<const events::KeyPressedEvent&>(event).repeat_count : 0;
            return ui::KeyEvent{.key = Key(key), .pressed = pressed, .shift = (modifiers_ & 1) != 0, .control = (modifiers_ & 2) != 0, .alt = (modifiers_ & 4) != 0, .super = (modifiers_ & 8) != 0, .repeat = repeat};
        }
        case events::EventType::kTextInput:
            return ui::TextEvent{static_cast<const events::TextInputEvent&>(event).text};
        case events::EventType::kTextCompositionStarted: {
            const auto& value = static_cast<const events::TextCompositionStartedEvent&>(event);
            return ui::CompositionEvent{ui::CompositionEvent::Type::Start, value.text, value.selection_start, value.selection_length};
        }
        case events::EventType::kTextCompositionUpdated: {
            const auto& value = static_cast<const events::TextCompositionUpdatedEvent&>(event);
            return ui::CompositionEvent{ui::CompositionEvent::Type::Update, value.text, value.selection_start, value.selection_length};
        }
        case events::EventType::kTextCompositionCommitted: {
            const auto& value = static_cast<const events::TextCompositionCommittedEvent&>(event);
            return ui::CompositionEvent{ui::CompositionEvent::Type::Commit, value.text, value.selection_start, value.selection_length};
        }
        case events::EventType::kTextCompositionCanceled: {
            const auto& value = static_cast<const events::TextCompositionCanceledEvent&>(event);
            return ui::CompositionEvent{ui::CompositionEvent::Type::Cancel, value.text, value.selection_start, value.selection_length};
        }
        case events::EventType::kPinch: {
            const auto& value = static_cast<const events::PinchEvent&>(event);
            return ui::GestureEvent{.type = ui::GestureEvent::Type::Pinch,
                .phase = Phase(value.gesture.phase),
                .center = {value.gesture.center_x, value.gesture.center_y},
                .delta = {value.gesture.delta_x, value.gesture.delta_y},
                .velocity = {value.gesture.velocity_x, value.gesture.velocity_y},
                .scale = value.scale_delta,
                .pointer_count = value.gesture.pointer_count,
                .modifiers = modifiers_};
        }
        case events::EventType::kPan: {
            const auto& value = static_cast<const events::PanEvent&>(event);
            return ui::GestureEvent{.type = ui::GestureEvent::Type::Pan,
                .phase = Phase(value.gesture.phase),
                .center = {value.gesture.center_x, value.gesture.center_y},
                .delta = {value.gesture.delta_x, value.gesture.delta_y},
                .velocity = {value.gesture.velocity_x, value.gesture.velocity_y},
                .pointer_count = value.gesture.pointer_count,
                .modifiers = modifiers_};
        }
        default:
            return std::nullopt;
    }
}

} // namespace woki::studio
