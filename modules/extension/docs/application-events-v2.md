# Application event ABI v2

The raw extension function signatures remain ABI v1. The payload delivered to `ext_on_event` is now application-event ABI v2 and starts with a fixed 40-byte little-endian header:

`u16 schema_version, u16 header_size, u32 payload_size, f64 timestamp, u64 sequence, u32 window, u64 device, u16 modifiers, u8 source, u8 flags`.

`schema_version` is 2 and `header_size` is 40. Decoders reject mismatched versions, sizes, truncated fixed payloads, and inconsistent variable-length fields. Flag bit zero marks a synthesized event.

The canonical ID and layout registry is `sdk/event_schema.def`. It covers window and framebuffer lifecycle, keys, pointer/scroll, UTF-8 text and composition, gestures, gamepads and raw joysticks, monitor changes, file drops, refresh requests, and platform errors. Mouse aliases and `KeyTyped` are intentionally not emitted.

Use the generated C decoders in `events.h` or the typed C++ dispatcher in `woki/extension/events.hpp`. String fields are UTF-8 byte views into the callback payload and remain valid only for the duration of `OnEvent`.
