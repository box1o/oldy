#pragma once

#include <woki/core.hpp>

namespace woki::events {

enum class EventCategory : u16 {
    kNone = 0,
    kWindow = 1 << 0,
    kInput = 1 << 1,
    kKeyboard = 1 << 2,
    kMouse = 1 << 3,
    kMouseButton = 1 << 4,
    kRender = 1 << 5,
    kApplication = 1 << 6,
    kScene = 1 << 7,
    kPhysics = 1 << 8,
    kAudio = 1 << 9,
    kCustom = 1 << 15,
};

[[nodiscard]] constexpr EventCategory operator|(EventCategory lhs, EventCategory rhs) noexcept {
    return static_cast<EventCategory>(static_cast<u16>(lhs) | static_cast<u16>(rhs));
}

[[nodiscard]] constexpr u16 operator&(EventCategory lhs, EventCategory rhs) noexcept {
    return static_cast<u16>(lhs) & static_cast<u16>(rhs);
}

[[nodiscard]] constexpr u16 operator&(u16 lhs, EventCategory rhs) noexcept {
    return lhs & static_cast<u16>(rhs);
}

} // namespace woki::events
