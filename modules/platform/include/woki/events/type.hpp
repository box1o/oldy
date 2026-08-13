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
    kFramebufferResized = 9,

    kKeyPressed = 100,
    kKeyReleased = 101,

    kPointerMoved = 150,
    kScrolled = 151,
    kPointerDown = 152,
    kPointerUp = 153,
    kPointerCancel = 154,
    kPointerEntered = 155,
    kPointerLeft = 156,
    kWindowScaleChanged = 157,

    kTextInput = 160,
    kTextCompositionStarted = 161,
    kTextCompositionUpdated = 162,
    kTextCompositionCommitted = 163,
    kTextCompositionCanceled = 164,

    kTap = 180,
    kDoubleTap = 181,
    kLongPress = 182,
    kPan = 183,
    kPinch = 184,
    kRotate = 185,

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

    kGamepadConnected = 400,
    kGamepadDisconnected = 401,
    kGamepadButtonChanged = 402,
    kGamepadAxisChanged = 403,
    kJoystickButtonChanged = 404,
    kJoystickAxisChanged = 405,
    kJoystickHatChanged = 406,

    kMonitorConnected = 450,
    kMonitorDisconnected = 451,
    kFilesDropped = 452,
    kWindowRefreshRequested = 453,
    kPlatformError = 454,

    kCustom = 10000,
};

} // namespace woki::events
