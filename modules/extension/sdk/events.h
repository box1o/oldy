#pragma once

#include <stdint.h>
#if !defined(__clang__) && !defined(__GNUC__)
#include <string.h>
#endif

#include "types.h"

// Stable application event IDs delivered to ext_on_event in raw ABI v1.
enum {

#define WOKI_EXT_EVENT(cpp_name, c_name, name, id, layout) WOKI_EXT_EVENT_##c_name = id,
#include "event_schema.def"
#undef WOKI_EXT_EVENT
};

#define WOKI_EXT_EVENT_EXTENSION_ID(local_id) ((woki_ext_event_type_t)(WOKI_EXT_EVENT_NAMESPACE | ((uint32_t)(local_id) & UINT32_C(0x7fffffff))))

typedef struct woki_ext_empty_event {
    uint8_t reserved;
} woki_ext_empty_event_t;

typedef struct woki_ext_size_event {
    uint32_t width;
    uint32_t height;
} woki_ext_size_event_t;

typedef struct woki_ext_position_event {
    int32_t x;
    int32_t y;
} woki_ext_position_event_t;

typedef struct woki_ext_scale_event {
    float x;
    float y;
} woki_ext_scale_event_t;

typedef struct woki_ext_key_pressed_event {
    uint16_t key;
    uint32_t repeat_count;
} woki_ext_key_pressed_event_t;

typedef struct woki_ext_key_released_event {
    uint16_t key;
} woki_ext_key_released_event_t;

typedef struct woki_ext_key_typed_event {
    uint32_t character;
} woki_ext_key_typed_event_t;

typedef struct woki_ext_mouse_scrolled_event {
    float offset_x;
    float offset_y;
} woki_ext_mouse_scrolled_event_t;

typedef struct woki_ext_mouse_button_event {
    uint8_t button;
    float x;
    float y;
} woki_ext_mouse_button_event_t;

typedef woki_ext_empty_event_t woki_ext_window_closed_event_t;
typedef woki_ext_size_event_t woki_ext_window_resized_event_t;
typedef woki_ext_empty_event_t woki_ext_window_focused_event_t;
typedef woki_ext_empty_event_t woki_ext_window_lost_focus_event_t;
typedef woki_ext_position_event_t woki_ext_window_moved_event_t;
typedef woki_ext_empty_event_t woki_ext_window_minimized_event_t;
typedef woki_ext_empty_event_t woki_ext_window_maximized_event_t;
typedef woki_ext_empty_event_t woki_ext_window_restored_event_t;
typedef woki_ext_empty_event_t woki_ext_mouse_entered_event_t;
typedef woki_ext_empty_event_t woki_ext_mouse_left_event_t;
typedef woki_ext_mouse_button_event_t woki_ext_mouse_button_pressed_event_t;
typedef woki_ext_mouse_button_event_t woki_ext_mouse_button_released_event_t;
typedef woki_ext_mouse_button_event_t woki_ext_mouse_button_clicked_event_t;
typedef woki_ext_scale_event_t woki_ext_window_scale_changed_event_t;
typedef woki_ext_size_event_t woki_ext_viewport_resized_event_t;
typedef woki_ext_empty_event_t woki_ext_app_shutdown_event_t;
typedef woki_ext_empty_event_t woki_ext_app_suspend_event_t;
typedef woki_ext_empty_event_t woki_ext_app_resume_event_t;

static inline uint16_t woki_ext_event_read_u16_le(const uint8_t* bytes) {
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
}

static inline uint32_t woki_ext_event_read_u32_le(const uint8_t* bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) | ((uint32_t)bytes[3] << 24u);
}

static inline int32_t woki_ext_event_read_i32_le(const uint8_t* bytes) {
    const uint32_t bits = woki_ext_event_read_u32_le(bytes);
    int32_t value;
#if defined(__clang__) || defined(__GNUC__)
    __builtin_memcpy(&value, &bits, sizeof(value));
#else
    memcpy(&value, &bits, sizeof(value));
#endif
    return value;
}

static inline float woki_ext_event_read_f32_le(const uint8_t* bytes) {
    const uint32_t bits = woki_ext_event_read_u32_le(bytes);
    float value;
#if defined(__clang__) || defined(__GNUC__)
    __builtin_memcpy(&value, &bits, sizeof(value));
#else
    memcpy(&value, &bits, sizeof(value));
#endif
    return value;
}

static inline int32_t woki_ext_decode_empty_event(const uint8_t* payload, uint32_t len, woki_ext_empty_event_t* out) {
    (void)payload;
    if (len != 0u || out == (woki_ext_empty_event_t*)0)
        return WOKI_EXT_INVALID;
    out->reserved = 0u;
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_size_event(const uint8_t* payload, uint32_t len, woki_ext_size_event_t* out) {
    if (len != 8u || payload == (const uint8_t*)0 || out == (woki_ext_size_event_t*)0)
        return WOKI_EXT_INVALID;
    out->width = woki_ext_event_read_u32_le(payload);
    out->height = woki_ext_event_read_u32_le(payload + 4u);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_position_event(const uint8_t* payload, uint32_t len, woki_ext_position_event_t* out) {
    if (len != 8u || payload == (const uint8_t*)0 || out == (woki_ext_position_event_t*)0)
        return WOKI_EXT_INVALID;
    out->x = woki_ext_event_read_i32_le(payload);
    out->y = woki_ext_event_read_i32_le(payload + 4u);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_scale_event(const uint8_t* payload, uint32_t len, woki_ext_scale_event_t* out) {
    if (len != 8u || payload == (const uint8_t*)0 || out == (woki_ext_scale_event_t*)0)
        return WOKI_EXT_INVALID;
    out->x = woki_ext_event_read_f32_le(payload);
    out->y = woki_ext_event_read_f32_le(payload + 4u);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_key_pressed_payload(const uint8_t* payload, uint32_t len, woki_ext_key_pressed_event_t* out) {
    if (len != 6u || payload == (const uint8_t*)0 || out == (woki_ext_key_pressed_event_t*)0)
        return WOKI_EXT_INVALID;
    out->key = woki_ext_event_read_u16_le(payload);
    out->repeat_count = woki_ext_event_read_u32_le(payload + 2u);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_key_released_payload(const uint8_t* payload, uint32_t len, woki_ext_key_released_event_t* out) {
    if (len != 2u || payload == (const uint8_t*)0 || out == (woki_ext_key_released_event_t*)0)
        return WOKI_EXT_INVALID;
    out->key = woki_ext_event_read_u16_le(payload);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_key_typed_payload(const uint8_t* payload, uint32_t len, woki_ext_key_typed_event_t* out) {
    if (len != 4u || payload == (const uint8_t*)0 || out == (woki_ext_key_typed_event_t*)0)
        return WOKI_EXT_INVALID;
    out->character = woki_ext_event_read_u32_le(payload);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_mouse_scrolled_payload(const uint8_t* payload, uint32_t len, woki_ext_mouse_scrolled_event_t* out) {
    if (len != 8u || payload == (const uint8_t*)0 || out == (woki_ext_mouse_scrolled_event_t*)0)
        return WOKI_EXT_INVALID;
    out->offset_x = woki_ext_event_read_f32_le(payload);
    out->offset_y = woki_ext_event_read_f32_le(payload + 4u);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_mouse_button_event(const uint8_t* payload, uint32_t len, woki_ext_mouse_button_event_t* out) {
    if (len != 9u || payload == (const uint8_t*)0 || out == (woki_ext_mouse_button_event_t*)0)
        return WOKI_EXT_INVALID;
    out->button = payload[0];
    out->x = woki_ext_event_read_f32_le(payload + 1u);
    out->y = woki_ext_event_read_f32_le(payload + 5u);
    return WOKI_EXT_OK;
}

#define WOKI_EXT_DECODER_empty woki_ext_decode_empty_event
#define WOKI_EXT_DECODER_size woki_ext_decode_size_event
#define WOKI_EXT_DECODER_position woki_ext_decode_position_event
#define WOKI_EXT_DECODER_scale woki_ext_decode_scale_event
#define WOKI_EXT_DECODER_key_pressed woki_ext_decode_key_pressed_payload
#define WOKI_EXT_DECODER_key_released woki_ext_decode_key_released_payload
#define WOKI_EXT_DECODER_key_typed woki_ext_decode_key_typed_payload
#define WOKI_EXT_DECODER_mouse_scrolled woki_ext_decode_mouse_scrolled_payload
#define WOKI_EXT_DECODER_mouse_button woki_ext_decode_mouse_button_event
#define WOKI_EXT_EVENT(cpp_name, c_name, name, id, layout)                                                                                                                                                                 \
    static inline int32_t woki_ext_decode_##name##_event(const uint8_t* payload, uint32_t len, woki_ext_##name##_event_t* out) {                                                                                           \
        return WOKI_EXT_DECODER_##layout(payload, len, out);                                                                                                                                                               \
    }
#include "event_schema.def"
#undef WOKI_EXT_EVENT

#undef WOKI_EXT_DECODER_empty
#undef WOKI_EXT_DECODER_size
#undef WOKI_EXT_DECODER_position
#undef WOKI_EXT_DECODER_scale
#undef WOKI_EXT_DECODER_key_pressed
#undef WOKI_EXT_DECODER_key_released
#undef WOKI_EXT_DECODER_key_typed
#undef WOKI_EXT_DECODER_mouse_scrolled
#undef WOKI_EXT_DECODER_mouse_button

static inline int32_t woki_ext_event_is_wildcard(woki_ext_event_type_t type) {
    return type == WOKI_EXT_EVENT_WILDCARD;
}

static inline woki_ext_event_type_t woki_ext_extension_event_id(uint32_t local_id) {
    return WOKI_EXT_EVENT_EXTENSION_ID(local_id);
}
