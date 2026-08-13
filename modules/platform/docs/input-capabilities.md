# Input capability matrix

The public event model is platform-neutral. A backend reports what the current runtime can actually provide through `InputCapabilities`; unsupported values are never fabricated.

| Capability | Windows/macOS/Linux GLFW | Desktop web | Mobile web | Future native mobile |
| --- | --- | --- | --- | --- |
| Keyboard and UTF text | Implemented | Implemented | Hardware dependent | Backend contract |
| Mouse pointer | Implemented | Implemented by DOM bridge | Browser dependent | Backend contract |
| Touch and pen | Not exposed by GLFW | Implemented by DOM bridge | Implemented by DOM bridge | Backend contract |
| Pressure/contact/tilt | Not exposed by GLFW | Browser/device dependent | Browser/device dependent | Backend contract |
| Precise wheel/trackpad scroll | Floating GLFW deltas | Implemented by Wheel Events | Browser/device dependent | Backend contract |
| Tap/pan/pinch/rotate/long press | Shared recognizer ready; native touch adapters remain | Shared recognizer | Shared recognizer | Shared recognizer |
| Standard gamepads | GLFW mapped gamepads | GLFW/browser mapping | Browser dependent | Backend contract |
| Raw joysticks | Implemented through GLFW axes/buttons/hats | Browser API limitation | Browser API limitation | Backend contract |
| IME composition | Backend work remaining | DOM composition events | DOM composition events | Backend contract |
| Sensors and haptics | Not implemented | Not implemented | Not implemented | Reserved only |

Android and iOS builds are deliberately outside the current implementation. A future backend must translate native input into `PlatformEventSink`, provide truthful capabilities, and pass the same input-state and gesture conformance tests without adding platform-specific public events.

The GLFW backend also translates close, logical resize, framebuffer resize, focus, move, minimize/restore, maximize/restore, content-scale, refresh, file-drop, monitor, and platform-error callbacks. These are part of the same ordered event queue; the rewrite is not limited to touch input.
