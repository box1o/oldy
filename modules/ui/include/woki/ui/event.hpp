#pragma once

#include <string>
#include <variant>
#include <functional>

#include "geometry.hpp"

namespace woki::ui {

enum class Phase : u8 { Capture, Target, Bubble };
enum class PointerButton : u8 { None, Primary, Secondary, Middle };
enum class PointerKind : u8 { Unknown, Mouse, Touch, Pen };
enum class KeyCode : u16 {
    Unknown,
    Tab,
    Enter,
    Space,
    Escape,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    Backspace,
    Delete,
    F,
    R,
    Minus,
    Equal
};
enum class Modifier : u8 { None = 0, Shift = 1, Control = 2, Alt = 4, Super = 8 };
using Modifiers = u8;
using PointerButtons = u16;

struct PointerEvent {
    enum class Type : u8 { Move, Down, Up, Cancel, Wheel };

    Type type{Type::Move};
    Point position{};
    Point delta{};
    PointerButton button{PointerButton::None};
    PointerKind kind{PointerKind::Unknown};
    PointerButtons buttons{};
    Modifiers modifiers{};
    f32 pressure{};
    f32 contact_width{};
    f32 contact_height{};
    f32 tilt_x{};
    f32 tilt_y{};
    u64 pointer{};
};

struct KeyEvent {
    KeyCode key{KeyCode::Unknown};
    bool pressed{};
    bool shift{};
    bool control{};
    bool alt{};
    bool super{};
    u32 repeat{};
};

struct TextEvent {
    std::string text;
};

struct CompositionEvent {
    enum class Type : u8 { Start, Update, Commit, Cancel };
    Type type{Type::Start};
    std::string text;
    u32 selection_start{};
    u32 selection_length{};
};

struct GestureEvent {
    enum class Type : u8 { Tap, DoubleTap, LongPress, Pan, Pinch, Rotate };
    enum class Phase : u8 { Begin, Update, End, Cancel };
    Type type{Type::Tap};
    Phase phase{Phase::Begin};
    Point center{};
    Point delta{};
    Point velocity{};
    f32 scale{1.0f};
    f32 rotation{};
    u8 pointer_count{};
    Modifiers modifiers{};
};

using Event = std::variant<PointerEvent, KeyEvent, TextEvent, CompositionEvent, GestureEvent>;

struct EventContext {
    Phase phase{Phase::Target};
    bool handled{};
    bool stopped{};

    void Handle() {
        handled = true;
    }

    void Stop() {
        handled = true;
        stopped = true;
    }
};

using EventHandler = std::function<void(EventContext&, const Event&)>;

} // namespace woki::ui
