#pragma once

#include <string_view>

#include "woki/events/input/events.hpp"

namespace woki::events {

enum class GesturePhase : u8 { kBegin, kUpdate, kEnd, kCancel };

struct GestureData {
    GesturePhase phase{GesturePhase::kBegin};
    f32 center_x{0};
    f32 center_y{0};
    u8 pointer_count{0};
    f32 delta_x{0};
    f32 delta_y{0};
    f32 total_x{0};
    f32 total_y{0};
    f32 velocity_x{0};
    f32 velocity_y{0};
};

struct TapEvent final : TypedEvent<EventType::kTap, EventCategory::kGesture, EventCategory::kInput> {
    GestureData gesture{};

    explicit TapEvent(GestureData value = {})
        : gesture(value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "Tap";
    }
};

struct DoubleTapEvent final : TypedEvent<EventType::kDoubleTap, EventCategory::kGesture, EventCategory::kInput> {
    GestureData gesture{};

    explicit DoubleTapEvent(GestureData value = {})
        : gesture(value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "DoubleTap";
    }
};

struct LongPressEvent final : TypedEvent<EventType::kLongPress, EventCategory::kGesture, EventCategory::kInput> {
    GestureData gesture{};

    explicit LongPressEvent(GestureData value = {})
        : gesture(value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "LongPress";
    }
};

struct PanEvent final : TypedEvent<EventType::kPan, EventCategory::kGesture, EventCategory::kInput> {
    GestureData gesture{};

    explicit PanEvent(GestureData value = {})
        : gesture(value) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "Pan";
    }
};

struct PinchEvent final : TypedEvent<EventType::kPinch, EventCategory::kGesture, EventCategory::kInput> {
    GestureData gesture{};
    f32 scale_delta{1};
    f32 scale{1};

    explicit PinchEvent(GestureData value = {}, f32 delta = 1, f32 accumulated = 1)
        : gesture(value),
          scale_delta(delta),
          scale(accumulated) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "Pinch";
    }
};

struct RotateEvent final : TypedEvent<EventType::kRotate, EventCategory::kGesture, EventCategory::kInput> {
    GestureData gesture{};
    f32 radians_delta{0};
    f32 radians{0};

    explicit RotateEvent(GestureData value = {}, f32 delta = 0, f32 accumulated = 0)
        : gesture(value),
          radians_delta(delta),
          radians(accumulated) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "Rotate";
    }
};

} // namespace woki::events
