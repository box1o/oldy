#pragma once

#include <map>
#include <array>
#include <unordered_set>

#include "woki/events/events.hpp"

namespace woki {

class InputState final {
public:
    void Apply(const events::Event& event) {
        switch (event.GetEventType()) {
            case events::EventType::kKeyPressed:
                keys_.insert(static_cast<u16>(static_cast<const events::KeyPressedEvent&>(event).key));
                break;
            case events::EventType::kKeyReleased:
                keys_.erase(static_cast<u16>(static_cast<const events::KeyReleasedEvent&>(event).key));
                break;
            case events::EventType::kPointerDown:
            case events::EventType::kPointerMoved:
            case events::EventType::kPointerEntered: {
                const auto* data = Pointer(event);
                if (data != nullptr)
                    pointers_[data->pointer] = *data;
                break;
            }
            case events::EventType::kPointerUp:
            case events::EventType::kPointerCancel:
            case events::EventType::kPointerLeft: {
                const auto* data = Pointer(event);
                if (data != nullptr)
                    pointers_.erase(data->pointer);
                break;
            }
            case events::EventType::kGamepadConnected: {
                const auto& value = static_cast<const events::GamepadConnectedEvent&>(event).state;
                gamepads_[value.device] = value;
                break;
            }
            case events::EventType::kGamepadDisconnected:
                gamepads_.erase(static_cast<const events::GamepadDisconnectedEvent&>(event).device);
                break;
            case events::EventType::kGamepadButtonChanged: {
                auto found = gamepads_.find(event.metadata.device);
                if (found != gamepads_.end()) {
                    const auto& value = static_cast<const events::GamepadButtonChangedEvent&>(event);
                    found->second.buttons[static_cast<std::size_t>(value.button)] = value.pressed;
                }
                break;
            }
            case events::EventType::kGamepadAxisChanged: {
                auto found = gamepads_.find(event.metadata.device);
                if (found != gamepads_.end()) {
                    const auto& value = static_cast<const events::GamepadAxisChangedEvent&>(event);
                    found->second.axes[static_cast<std::size_t>(value.axis)] = value.value;
                }
                break;
            }
            case events::EventType::kJoystickButtonChanged: {
                auto found = gamepads_.find(event.metadata.device);
                const auto& value = static_cast<const events::JoystickButtonChangedEvent&>(event);
                if (found != gamepads_.end() && value.index < found->second.raw_buttons.size())
                    found->second.raw_buttons[value.index] = value.pressed;
                break;
            }
            case events::EventType::kJoystickAxisChanged: {
                auto found = gamepads_.find(event.metadata.device);
                const auto& value = static_cast<const events::JoystickAxisChangedEvent&>(event);
                if (found != gamepads_.end() && value.index < found->second.raw_axes.size())
                    found->second.raw_axes[value.index] = value.value;
                break;
            }
            case events::EventType::kJoystickHatChanged: {
                auto found = gamepads_.find(event.metadata.device);
                const auto& value = static_cast<const events::JoystickHatChangedEvent&>(event);
                if (found != gamepads_.end() && value.index < found->second.raw_hats.size())
                    found->second.raw_hats[value.index] = value.value;
                break;
            }
            case events::EventType::kWindowLostFocus:
            case events::EventType::kAppSuspend:
                keys_.clear();
                pointers_.clear();
                break;
            default:
                break;
        }
        modifiers_ = event.metadata.modifiers;
    }

    [[nodiscard]] bool IsKeyDown(events::KeyCode key) const {
        return keys_.contains(static_cast<u16>(key));
    }

    [[nodiscard]] const std::map<events::PointerId, events::PointerData>& Pointers() const noexcept {
        return pointers_;
    }

    [[nodiscard]] const std::map<events::DeviceId, events::GamepadState>& Gamepads() const noexcept {
        return gamepads_;
    }

    [[nodiscard]] events::ModifierFlags Modifiers() const noexcept {
        return modifiers_;
    }

private:
    static const events::PointerData* Pointer(const events::Event& event) {
        switch (event.GetEventType()) {
            case events::EventType::kPointerDown:
                return &static_cast<const events::PointerDownEvent&>(event).pointer_data;
            case events::EventType::kPointerMoved:
                return &static_cast<const events::PointerMoveEvent&>(event).pointer_data;
            case events::EventType::kPointerUp:
                return &static_cast<const events::PointerUpEvent&>(event).pointer_data;
            case events::EventType::kPointerCancel:
                return &static_cast<const events::PointerCancelEvent&>(event).pointer_data;
            case events::EventType::kPointerEntered:
                return &static_cast<const events::PointerEnterEvent&>(event).pointer_data;
            case events::EventType::kPointerLeft:
                return &static_cast<const events::PointerLeaveEvent&>(event).pointer_data;
            default:
                return nullptr;
        }
    }

    std::unordered_set<u16> keys_;
    std::map<events::PointerId, events::PointerData> pointers_;
    std::map<events::DeviceId, events::GamepadState> gamepads_;
    events::ModifierFlags modifiers_{0};
};

} // namespace woki
