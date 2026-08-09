#pragma once

#include <woki/core.hpp>

namespace woki::events {

enum class EventType : u16 {
    kNone = 0,

    kWindowClosed = 1,
    kWindowResized = 2,
    kWindowFocused = 3,
    kWindowLostFocus = 4,
    kWindowMoved = 5,
    kWindowMinimized = 6,
    kWindowMaximized = 7,
    kWindowRestored = 8,

    kKeyPressed = 100,
    kKeyReleased = 101,
    kKeyTyped = 102,

    kMouseMoved = 150,
    kMouseScrolled = 151,
    kMouseButtonPressed = 152,
    kMouseButtonReleased = 153,
    kMouseButtonClicked = 154,
    kMouseEntered = 155,
    kMouseLeft = 156,
    kWindowScaleChanged = 157,

    kFrameBegin = 200,
    kFrameEnd = 201,
    kRenderBegin = 202,
    kRenderEnd = 203,
    kViewportResized = 204,
    kSwapBuffers = 205,

    kAppTick = 300,
    kAppUpdate = 301,
    kAppRender = 302,
    kAppShutdown = 303,
    kAppSuspend = 304,
    kAppResume = 305,

    kCustom = 10000,
};

} // namespace woki::events
