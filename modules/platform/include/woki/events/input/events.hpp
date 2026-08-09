#pragma once

#include <string_view>

#include "woki/events/base.hpp"

namespace woki::events {

enum class KeyCode : u16 {
    kSpace = 32,
    kApostrophe = 39,
    kComma = 44,
    kMinus = 45,
    kPeriod = 46,
    kSlash = 47,

    kD0 = 48,
    kD1,
    kD2,
    kD3,
    kD4,
    kD5,
    kD6,
    kD7,
    kD8,
    kD9,

    kSemicolon = 59,
    kEqual = 61,

    kA = 65,
    kB,
    kC,
    kD,
    kE,
    kF,
    kG,
    kH,
    kI,
    kJ,
    kK,
    kL,
    kM,
    kN,
    kO,
    kP,
    kQ,
    kR,
    kS,
    kT,
    kU,
    kV,
    kW,
    kX,
    kY,
    kZ,

    kLeftBracket = 91,
    kBackslash = 92,
    kRightBracket = 93,
    kGraveAccent = 96,

    kEscape = 256,
    kEnter,
    kTab,
    kBackspace,
    kInsert,
    kDelete,
    kRight,
    kLeft,
    kDown,
    kUp,
    kPageUp,
    kPageDown,
    kHome,
    kEnd,

    kCapsLock = 280,
    kScrollLock,
    kNumLock,
    kPrintScreen,
    kPause,

    kF1 = 290,
    kF2,
    kF3,
    kF4,
    kF5,
    kF6,
    kF7,
    kF8,
    kF9,
    kF10,
    kF11,
    kF12,

    kLeftShift = 340,
    kLeftControl,
    kLeftAlt,
    kLeftSuper,
    kRightShift,
    kRightControl,
    kRightAlt,
    kRightSuper,

    kMenu = 348,
};

enum class MouseButton : u8 {
    kLeft = 0,
    kRight = 1,
    kMiddle = 2,
    kButton4 = 3,
    kButton5 = 4,
    kButton6 = 5,
    kButton7 = 6,
    kButton8 = 7,
};

struct KeyPressedEvent final : TypedEvent<EventType::kKeyPressed, EventCategory::kKeyboard, EventCategory::kInput> {
    KeyCode key{KeyCode::kSpace};
    u32 repeat_count{0};

    KeyPressedEvent(KeyCode key_value, u32 repeat_count_value = 0)
        : key(key_value),
          repeat_count(repeat_count_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "KeyPressed";
    }
};

struct KeyReleasedEvent final : TypedEvent<EventType::kKeyReleased, EventCategory::kKeyboard, EventCategory::kInput> {
    KeyCode key{KeyCode::kSpace};

    explicit KeyReleasedEvent(KeyCode key_value)
        : key(key_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "KeyReleased";
    }
};

struct KeyTypedEvent final : TypedEvent<EventType::kKeyTyped, EventCategory::kKeyboard, EventCategory::kInput> {
    u32 character{0};

    explicit KeyTypedEvent(u32 character_value)
        : character(character_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "KeyTyped";
    }
};

struct MouseMovedEvent final : TypedEvent<EventType::kMouseMoved, EventCategory::kMouse, EventCategory::kInput> {
    f32 x{0.0f};
    f32 y{0.0f};
    f32 delta_x{0.0f};
    f32 delta_y{0.0f};

    MouseMovedEvent(f32 x_value, f32 y_value, f32 delta_x_value, f32 delta_y_value)
        : x(x_value),
          y(y_value),
          delta_x(delta_x_value),
          delta_y(delta_y_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "MouseMoved";
    }
};

struct MouseScrolledEvent final : TypedEvent<EventType::kMouseScrolled, EventCategory::kMouse, EventCategory::kInput> {
    f32 offset_x{0.0f};
    f32 offset_y{0.0f};

    MouseScrolledEvent(f32 offset_x_value, f32 offset_y_value)
        : offset_x(offset_x_value),
          offset_y(offset_y_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "MouseScrolled";
    }
};

struct MouseButtonPressedEvent final : TypedEvent<EventType::kMouseButtonPressed, EventCategory::kMouse, EventCategory::kMouseButton, EventCategory::kInput> {
    MouseButton button{MouseButton::kLeft};
    f32 x{0.0f};
    f32 y{0.0f};

    MouseButtonPressedEvent(MouseButton button_value, f32 x_value, f32 y_value)
        : button(button_value),
          x(x_value),
          y(y_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "MouseButtonPressed";
    }
};

struct MouseButtonReleasedEvent final : TypedEvent<EventType::kMouseButtonReleased, EventCategory::kMouse, EventCategory::kMouseButton, EventCategory::kInput> {
    MouseButton button{MouseButton::kLeft};
    f32 x{0.0f};
    f32 y{0.0f};

    MouseButtonReleasedEvent(MouseButton button_value, f32 x_value, f32 y_value)
        : button(button_value),
          x(x_value),
          y(y_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "MouseButtonReleased";
    }
};

struct MouseButtonClickedEvent final : TypedEvent<EventType::kMouseButtonClicked, EventCategory::kMouse, EventCategory::kMouseButton, EventCategory::kInput> {
    MouseButton button{MouseButton::kLeft};
    f32 x{0.0f};
    f32 y{0.0f};

    MouseButtonClickedEvent(MouseButton button_value, f32 x_value, f32 y_value)
        : button(button_value),
          x(x_value),
          y(y_value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "MouseButtonClicked";
    }
};

struct MouseEnteredEvent final : TypedEvent<EventType::kMouseEntered, EventCategory::kMouse, EventCategory::kInput> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "MouseEntered";
    }
};

struct MouseLeftEvent final : TypedEvent<EventType::kMouseLeft, EventCategory::kMouse, EventCategory::kInput> {
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "MouseLeft";
    }
};

} // namespace woki::events
