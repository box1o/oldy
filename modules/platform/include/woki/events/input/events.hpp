#pragma once

#include <string>
#include <string_view>

#include "woki/events/base.hpp"

namespace woki::events {

enum class KeyCode : u16 {
    kUnknown = 0,
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

enum class PointerKind : u8 { kUnknown, kMouse, kTouch, kPen };
enum class PointerButton : u8 { kPrimary = 0, kSecondary = 1, kMiddle = 2, kBack = 3, kForward = 4, kButton6 = 5, kButton7 = 6, kButton8 = 7, kNone = 255 };
using PointerButtons = u16;
enum class ScrollUnit : u8 { kPixel, kLine, kPage };
enum class ScrollPhase : u8 { kNone, kBegin, kUpdate, kEnd, kMomentum };

struct PointerData {
    PointerId pointer{0};
    PointerKind kind{PointerKind::kUnknown};
    bool primary{true};
    PointerButton button{PointerButton::kNone};
    PointerButtons buttons{0};
    f32 x{0};
    f32 y{0};
    f32 delta_x{0};
    f32 delta_y{0};
    f32 pressure{0};
    f32 contact_width{0};
    f32 contact_height{0};
    f32 tilt_x{0};
    f32 tilt_y{0};
    f32 twist{0};
};

#define WOKI_POINTER_EVENT(name, type, label)                                                                                                                                                                              \
    struct name final : TypedEvent<EventType::type, EventCategory::kPointer, EventCategory::kInput> {                                                                                                                      \
        PointerData pointer_data{};                                                                                                                                                                                        \
        name() = default;                                                                                                                                                                                                  \
        explicit name(PointerData value)                                                                                                                                                                                   \
            : pointer_data(value) {}                                                                                                                                                                                       \
        [[nodiscard]] std::string_view GetName() const noexcept override {                                                                                                                                                 \
            return label;                                                                                                                                                                                                  \
        }                                                                                                                                                                                                                  \
    }

WOKI_POINTER_EVENT(PointerDownEvent, kPointerDown, "PointerDown");
WOKI_POINTER_EVENT(PointerMoveEvent, kPointerMoved, "PointerMoved");
WOKI_POINTER_EVENT(PointerUpEvent, kPointerUp, "PointerUp");
WOKI_POINTER_EVENT(PointerCancelEvent, kPointerCancel, "PointerCancel");
WOKI_POINTER_EVENT(PointerEnterEvent, kPointerEntered, "PointerEntered");
WOKI_POINTER_EVENT(PointerLeaveEvent, kPointerLeft, "PointerLeft");
#undef WOKI_POINTER_EVENT

struct ScrollEvent final : TypedEvent<EventType::kScrolled, EventCategory::kPointer, EventCategory::kInput> {
    f32 delta_x{0};
    f32 delta_y{0};
    f32 x{0};
    f32 y{0};
    ScrollUnit unit{ScrollUnit::kLine};
    ScrollPhase phase{ScrollPhase::kNone};
    PointerKind kind{PointerKind::kMouse};
    bool precise{false};

    ScrollEvent(f32 x_delta, f32 y_delta)
        : delta_x(x_delta),
          delta_y(y_delta) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "Scrolled";
    }
};

struct KeyPressedEvent final : TypedEvent<EventType::kKeyPressed, EventCategory::kKeyboard, EventCategory::kInput> {
    KeyCode key{KeyCode::kUnknown};
    i32 scan_code{0};
    u32 repeat_count{0};

    KeyPressedEvent(KeyCode value, u32 repeats = 0, i32 scan = 0)
        : key(value),
          scan_code(scan),
          repeat_count(repeats) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "KeyPressed";
    }
};

struct KeyReleasedEvent final : TypedEvent<EventType::kKeyReleased, EventCategory::kKeyboard, EventCategory::kInput> {
    KeyCode key{KeyCode::kUnknown};
    i32 scan_code{0};

    explicit KeyReleasedEvent(KeyCode value, i32 scan = 0)
        : key(value),
          scan_code(scan) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "KeyReleased";
    }
};

struct TextInputEvent final : TypedEvent<EventType::kTextInput, EventCategory::kText, EventCategory::kInput> {
    std::string text;

    explicit TextInputEvent(std::string value)
        : text(std::move(value)) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "TextInput";
    }
};

enum class CompositionPhase : u8 { kStarted, kUpdated, kCommitted, kCanceled };

template <EventType Type, CompositionPhase Phase>
struct TextCompositionEvent final : TypedEvent<Type, EventCategory::kText, EventCategory::kInput> {
    std::string text;
    u32 selection_start{0};
    u32 selection_length{0};

    TextCompositionEvent(std::string value = {}, u32 start = 0, u32 length = 0)
        : text(std::move(value)),
          selection_start(start),
          selection_length(length) {}

    [[nodiscard]] std::string_view GetName() const noexcept override {
        if constexpr (Phase == CompositionPhase::kStarted)
            return "TextCompositionStarted";
        if constexpr (Phase == CompositionPhase::kUpdated)
            return "TextCompositionUpdated";
        if constexpr (Phase == CompositionPhase::kCommitted)
            return "TextCompositionCommitted";
        return "TextCompositionCanceled";
    }
};

using TextCompositionStartedEvent = TextCompositionEvent<EventType::kTextCompositionStarted, CompositionPhase::kStarted>;
using TextCompositionUpdatedEvent = TextCompositionEvent<EventType::kTextCompositionUpdated, CompositionPhase::kUpdated>;
using TextCompositionCommittedEvent = TextCompositionEvent<EventType::kTextCompositionCommitted, CompositionPhase::kCommitted>;
using TextCompositionCanceledEvent = TextCompositionEvent<EventType::kTextCompositionCanceled, CompositionPhase::kCanceled>;

} // namespace woki::events
