#pragma once

#include <array>
#include <string>
#include <vector>
#include <string_view>

#include "woki/events/base.hpp"

namespace woki::events {

enum class GamepadButton : u8 { kA, kB, kX, kY, kLeftBumper, kRightBumper, kBack, kStart, kGuide, kLeftStick, kRightStick, kDpadUp, kDpadRight, kDpadDown, kDpadLeft, kCount };
enum class GamepadAxis : u8 { kLeftX, kLeftY, kRightX, kRightY, kLeftTrigger, kRightTrigger, kCount };

struct GamepadState final {
    DeviceId device{0};
    bool connected{false};
    bool mapped{false};
    std::string name;
    std::string guid;
    std::array<bool, static_cast<std::size_t>(GamepadButton::kCount)> buttons{};
    std::array<f32, static_cast<std::size_t>(GamepadAxis::kCount)> axes{};
    std::vector<bool> raw_buttons;
    std::vector<f32> raw_axes;
    std::vector<u8> raw_hats;
};

struct GamepadConnectedEvent final : TypedEvent<EventType::kGamepadConnected, EventCategory::kGamepad, EventCategory::kDevice, EventCategory::kInput> {
    GamepadState state;

    explicit GamepadConnectedEvent(GamepadState value)
        : state(std::move(value)) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "GamepadConnected";
    }
};

struct GamepadDisconnectedEvent final : TypedEvent<EventType::kGamepadDisconnected, EventCategory::kGamepad, EventCategory::kDevice, EventCategory::kInput> {
    DeviceId device{0};

    explicit GamepadDisconnectedEvent(DeviceId value)
        : device(value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "GamepadDisconnected";
    }
};

struct GamepadButtonChangedEvent final : TypedEvent<EventType::kGamepadButtonChanged, EventCategory::kGamepad, EventCategory::kInput> {
    GamepadButton button{GamepadButton::kA};
    bool pressed{false};
    f32 value{0};

    GamepadButtonChangedEvent(GamepadButton key, bool down, f32 amount)
        : button(key),
          pressed(down),
          value(amount) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "GamepadButtonChanged";
    }
};

struct GamepadAxisChangedEvent final : TypedEvent<EventType::kGamepadAxisChanged, EventCategory::kGamepad, EventCategory::kInput> {
    GamepadAxis axis{GamepadAxis::kLeftX};
    f32 value{0};

    GamepadAxisChangedEvent(GamepadAxis key, f32 amount)
        : axis(key),
          value(amount) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "GamepadAxisChanged";
    }
};

struct JoystickButtonChangedEvent final : TypedEvent<EventType::kJoystickButtonChanged, EventCategory::kGamepad, EventCategory::kInput> {
    u16 index{0};
    bool pressed{false};

    JoystickButtonChangedEvent(u16 value, bool down)
        : index(value),
          pressed(down) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "JoystickButtonChanged";
    }
};

struct JoystickAxisChangedEvent final : TypedEvent<EventType::kJoystickAxisChanged, EventCategory::kGamepad, EventCategory::kInput> {
    u16 index{0};
    f32 value{0};

    JoystickAxisChangedEvent(u16 key, f32 amount)
        : index(key),
          value(amount) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "JoystickAxisChanged";
    }
};

struct JoystickHatChangedEvent final : TypedEvent<EventType::kJoystickHatChanged, EventCategory::kGamepad, EventCategory::kInput> {
    u16 index{0};
    u8 value{0};

    JoystickHatChangedEvent(u16 key, u8 amount)
        : index(key),
          value(amount) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "JoystickHatChanged";
    }
};

} // namespace woki::events
