#pragma once

#include <stdint.h>
#if !defined(__clang__) && !defined(__GNUC__)
#include <string.h>
#endif

#include "types.h"

#define WOKI_EXT_EVENT_ABI_VERSION UINT16_C(2)
#define WOKI_EXT_EVENT_METADATA_SIZE UINT32_C(40)

enum {

#define WOKI_EXT_EVENT(cpp_name, c_name, name, id, layout) WOKI_EXT_EVENT_##c_name = id,
#include "event_schema.def"
#undef WOKI_EXT_EVENT
};

#define WOKI_EXT_EVENT_EXTENSION_ID(local_id) ((woki_ext_event_type_t)(WOKI_EXT_EVENT_NAMESPACE | ((uint32_t)(local_id) & UINT32_C(0x7fffffff))))

typedef struct woki_ext_event_metadata {
    uint16_t schema_version;
    uint16_t header_size;
    uint32_t payload_size;
    double timestamp;
    uint64_t sequence;
    uint32_t window;
    uint64_t device;
    uint16_t modifiers;
    uint8_t source;
    uint8_t flags;
} woki_ext_event_metadata_t;

typedef struct woki_ext_metadata_event {
    woki_ext_event_metadata_t metadata;
} woki_ext_metadata_event_t;

typedef struct woki_ext_size_event {
    woki_ext_event_metadata_t metadata;
    uint32_t width;
    uint32_t height;
} woki_ext_size_event_t;

typedef struct woki_ext_position_event {
    woki_ext_event_metadata_t metadata;
    int32_t x;
    int32_t y;
} woki_ext_position_event_t;

typedef struct woki_ext_scale_event {
    woki_ext_event_metadata_t metadata;
    float x;
    float y;
} woki_ext_scale_event_t;

typedef struct woki_ext_key_pressed_event {
    woki_ext_event_metadata_t metadata;
    uint16_t key;
    int32_t scan_code;
    uint32_t repeat_count;
} woki_ext_key_pressed_event_t;

typedef struct woki_ext_key_released_event {
    woki_ext_event_metadata_t metadata;
    uint16_t key;
    int32_t scan_code;
} woki_ext_key_released_event_t;

typedef struct woki_ext_pointer_event {
    woki_ext_event_metadata_t metadata;
    uint64_t pointer_id;
    uint8_t kind;
    uint8_t primary;
    uint8_t button;
    uint16_t buttons;
    float x, y, delta_x, delta_y;
    float pressure, contact_width, contact_height, tilt_x, tilt_y, twist;
} woki_ext_pointer_event_t;

typedef struct woki_ext_scroll_event {
    woki_ext_event_metadata_t metadata;
    float delta_x, delta_y, x, y;
    uint8_t unit, phase, kind, precise;
} woki_ext_scroll_event_t;

typedef struct woki_ext_text_event {
    woki_ext_event_metadata_t metadata;
    const uint8_t* text;
    uint32_t text_size;
} woki_ext_text_event_t;

typedef struct woki_ext_composition_event {
    woki_ext_event_metadata_t metadata;
    uint32_t selection_start, selection_length;
    const uint8_t* text;
    uint32_t text_size;
} woki_ext_composition_event_t;

typedef struct woki_ext_gesture_event {
    woki_ext_event_metadata_t metadata;
    uint8_t phase, pointer_count;
    float center_x, center_y, delta_x, delta_y, total_x, total_y, velocity_x, velocity_y;
} woki_ext_gesture_event_t;

typedef struct woki_ext_pinch_event {
    woki_ext_gesture_event_t gesture;
    float scale_delta, scale;
} woki_ext_pinch_event_t;

typedef struct woki_ext_rotate_event {
    woki_ext_gesture_event_t gesture;
    float radians_delta, radians;
} woki_ext_rotate_event_t;

typedef struct woki_ext_device_event {
    woki_ext_event_metadata_t metadata;
} woki_ext_device_event_t;

typedef struct woki_ext_gamepad_connection_event {
    woki_ext_event_metadata_t metadata;
    uint8_t mapped;
    uint16_t buttons;
    float axes[6];
    const uint8_t* name;
    uint32_t name_size;
    const uint8_t* guid;
    uint32_t guid_size;
    uint16_t raw_axis_count;
    uint16_t raw_button_count;
    uint16_t raw_hat_count;
    const uint8_t* raw_axes;
    const uint8_t* raw_buttons;
    const uint8_t* raw_hats;
} woki_ext_gamepad_connection_event_t;

typedef struct woki_ext_gamepad_button_event {
    woki_ext_event_metadata_t metadata;
    uint8_t button, pressed;
    float value;
} woki_ext_gamepad_button_event_t;

typedef struct woki_ext_gamepad_axis_event {
    woki_ext_event_metadata_t metadata;
    uint8_t axis;
    float value;
} woki_ext_gamepad_axis_event_t;

typedef struct woki_ext_joystick_button_event {
    woki_ext_event_metadata_t metadata;
    uint16_t index;
    uint8_t pressed;
} woki_ext_joystick_button_event_t;

typedef struct woki_ext_joystick_axis_event {
    woki_ext_event_metadata_t metadata;
    uint16_t index;
    float value;
} woki_ext_joystick_axis_event_t;

typedef struct woki_ext_joystick_hat_event {
    woki_ext_event_metadata_t metadata;
    uint16_t index;
    uint8_t value;
} woki_ext_joystick_hat_event_t;

typedef struct woki_ext_text_list_event {
    woki_ext_event_metadata_t metadata;
    uint32_t count;
    const uint8_t* data;
    uint32_t data_size;
} woki_ext_text_list_event_t;

typedef struct woki_ext_platform_error_event {
    woki_ext_event_metadata_t metadata;
    int32_t code;
    const uint8_t* description;
    uint32_t description_size;
} woki_ext_platform_error_event_t;

#define WOKI_EXT_EVENT(cpp_name, c_name, name, id, layout) typedef woki_ext_##layout##_event_t woki_ext_##name##_event_t;
#include "event_schema.def"
#undef WOKI_EXT_EVENT

static inline uint16_t woki_ext_event_read_u16_le(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8u));
}

static inline uint32_t woki_ext_event_read_u32_le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) | ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

static inline uint64_t woki_ext_event_read_u64_le(const uint8_t* p) {
    return (uint64_t)woki_ext_event_read_u32_le(p) | ((uint64_t)woki_ext_event_read_u32_le(p + 4u) << 32u);
}
#if defined(__clang__) || defined(__GNUC__)
#define WOKI_EXT_COPY_BITS(dst, src, size) __builtin_memcpy((dst), (src), (size))
#else
#define WOKI_EXT_COPY_BITS(dst, src, size) memcpy((dst), (src), (size))
#endif
static inline int32_t woki_ext_event_read_i32_le(const uint8_t* p) {
    uint32_t b = woki_ext_event_read_u32_le(p);
    int32_t v;
    WOKI_EXT_COPY_BITS(&v, &b, sizeof(v));
    return v;
}

static inline float woki_ext_event_read_f32_le(const uint8_t* p) {
    uint32_t b = woki_ext_event_read_u32_le(p);
    float v;
    WOKI_EXT_COPY_BITS(&v, &b, sizeof(v));
    return v;
}

static inline double woki_ext_event_read_f64_le(const uint8_t* p) {
    uint64_t b = woki_ext_event_read_u64_le(p);
    double v;
    WOKI_EXT_COPY_BITS(&v, &b, sizeof(v));
    return v;
}

static inline int32_t woki_ext_decode_event_metadata(const uint8_t* p, uint32_t len, woki_ext_event_metadata_t* out) {
    if (p == (const uint8_t*)0 || out == (woki_ext_event_metadata_t*)0 || len < WOKI_EXT_EVENT_METADATA_SIZE)
        return WOKI_EXT_INVALID;
    out->schema_version = woki_ext_event_read_u16_le(p);
    out->header_size = woki_ext_event_read_u16_le(p + 2u);
    out->payload_size = woki_ext_event_read_u32_le(p + 4u);
    if (out->schema_version != WOKI_EXT_EVENT_ABI_VERSION || out->header_size != WOKI_EXT_EVENT_METADATA_SIZE || out->payload_size != len)
        return WOKI_EXT_INVALID;
    out->timestamp = woki_ext_event_read_f64_le(p + 8u);
    out->sequence = woki_ext_event_read_u64_le(p + 16u);
    out->window = woki_ext_event_read_u32_le(p + 24u);
    out->device = woki_ext_event_read_u64_le(p + 28u);
    out->modifiers = woki_ext_event_read_u16_le(p + 36u);
    out->source = p[38];
    out->flags = p[39];
    return WOKI_EXT_OK;
}

#define WOKI_EXT_BEGIN(expected)                                                                                                                                                                                           \
    if (len != (expected) || woki_ext_decode_event_metadata(p, len, &out->metadata) != WOKI_EXT_OK)                                                                                                                        \
    return WOKI_EXT_INVALID

static inline int32_t woki_ext_decode_metadata_payload(const uint8_t* p, uint32_t len, woki_ext_metadata_event_t* out) {
    if (!out)
        return WOKI_EXT_INVALID;
    WOKI_EXT_BEGIN(40u);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_size_payload(const uint8_t* p, uint32_t len, woki_ext_size_event_t* out) {
    if (!out)
        return WOKI_EXT_INVALID;
    WOKI_EXT_BEGIN(48u);
    out->width = woki_ext_event_read_u32_le(p + 40);
    out->height = woki_ext_event_read_u32_le(p + 44);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_position_payload(const uint8_t* p, uint32_t len, woki_ext_position_event_t* out) {
    if (!out)
        return WOKI_EXT_INVALID;
    WOKI_EXT_BEGIN(48u);
    out->x = woki_ext_event_read_i32_le(p + 40);
    out->y = woki_ext_event_read_i32_le(p + 44);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_scale_payload(const uint8_t* p, uint32_t len, woki_ext_scale_event_t* out) {
    if (!out)
        return WOKI_EXT_INVALID;
    WOKI_EXT_BEGIN(48u);
    out->x = woki_ext_event_read_f32_le(p + 40);
    out->y = woki_ext_event_read_f32_le(p + 44);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_key_pressed_payload(const uint8_t* p, uint32_t len, woki_ext_key_pressed_event_t* out) {
    if (!out)
        return WOKI_EXT_INVALID;
    WOKI_EXT_BEGIN(50u);
    out->key = woki_ext_event_read_u16_le(p + 40);
    out->scan_code = woki_ext_event_read_i32_le(p + 42);
    out->repeat_count = woki_ext_event_read_u32_le(p + 46);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_key_released_payload(const uint8_t* p, uint32_t len, woki_ext_key_released_event_t* out) {
    if (!out)
        return WOKI_EXT_INVALID;
    WOKI_EXT_BEGIN(46u);
    out->key = woki_ext_event_read_u16_le(p + 40);
    out->scan_code = woki_ext_event_read_i32_le(p + 42);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_pointer_payload(const uint8_t* p, uint32_t len, woki_ext_pointer_event_t* out) {
    if (!out)
        return WOKI_EXT_INVALID;
    WOKI_EXT_BEGIN(94u);
    out->pointer_id = woki_ext_event_read_u64_le(p + 40);
    out->kind = p[48];
    out->primary = p[49];
    out->button = p[50];
    out->buttons = woki_ext_event_read_u16_le(p + 52);
    out->x = woki_ext_event_read_f32_le(p + 54);
    out->y = woki_ext_event_read_f32_le(p + 58);
    out->delta_x = woki_ext_event_read_f32_le(p + 62);
    out->delta_y = woki_ext_event_read_f32_le(p + 66);
    out->pressure = woki_ext_event_read_f32_le(p + 70);
    out->contact_width = woki_ext_event_read_f32_le(p + 74);
    out->contact_height = woki_ext_event_read_f32_le(p + 78);
    out->tilt_x = woki_ext_event_read_f32_le(p + 82);
    out->tilt_y = woki_ext_event_read_f32_le(p + 86);
    out->twist = woki_ext_event_read_f32_le(p + 90);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_scroll_payload(const uint8_t* p, uint32_t len, woki_ext_scroll_event_t* out) {
    if (!out)
        return WOKI_EXT_INVALID;
    WOKI_EXT_BEGIN(60u);
    out->delta_x = woki_ext_event_read_f32_le(p + 40);
    out->delta_y = woki_ext_event_read_f32_le(p + 44);
    out->x = woki_ext_event_read_f32_le(p + 48);
    out->y = woki_ext_event_read_f32_le(p + 52);
    out->unit = p[56];
    out->phase = p[57];
    out->kind = p[58];
    out->precise = p[59];
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_text_payload(const uint8_t* p, uint32_t len, woki_ext_text_event_t* out) {
    if (!out || len < 44u || woki_ext_decode_event_metadata(p, len, &out->metadata) != WOKI_EXT_OK)
        return WOKI_EXT_INVALID;
    out->text_size = woki_ext_event_read_u32_le(p + 40);
    if (out->text_size != len - 44u)
        return WOKI_EXT_INVALID;
    out->text = p + 44;
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_composition_payload(const uint8_t* p, uint32_t len, woki_ext_composition_event_t* out) {
    if (!out || len < 52u || woki_ext_decode_event_metadata(p, len, &out->metadata) != WOKI_EXT_OK)
        return WOKI_EXT_INVALID;
    out->selection_start = woki_ext_event_read_u32_le(p + 40);
    out->selection_length = woki_ext_event_read_u32_le(p + 44);
    out->text_size = woki_ext_event_read_u32_le(p + 48);
    if (out->text_size != len - 52u)
        return WOKI_EXT_INVALID;
    out->text = p + 52;
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_gesture_payload(const uint8_t* p, uint32_t len, woki_ext_gesture_event_t* out) {
    if (!out)
        return WOKI_EXT_INVALID;
    WOKI_EXT_BEGIN(76u);
    out->phase = p[40];
    out->pointer_count = p[41];
    out->center_x = woki_ext_event_read_f32_le(p + 44);
    out->center_y = woki_ext_event_read_f32_le(p + 48);
    out->delta_x = woki_ext_event_read_f32_le(p + 52);
    out->delta_y = woki_ext_event_read_f32_le(p + 56);
    out->total_x = woki_ext_event_read_f32_le(p + 60);
    out->total_y = woki_ext_event_read_f32_le(p + 64);
    out->velocity_x = woki_ext_event_read_f32_le(p + 68);
    out->velocity_y = woki_ext_event_read_f32_le(p + 72);
    return WOKI_EXT_OK;
}

static inline void woki_ext_decode_gesture_fields(const uint8_t* p, woki_ext_gesture_event_t* out) {
    out->phase = p[40];
    out->pointer_count = p[41];
    out->center_x = woki_ext_event_read_f32_le(p + 44);
    out->center_y = woki_ext_event_read_f32_le(p + 48);
    out->delta_x = woki_ext_event_read_f32_le(p + 52);
    out->delta_y = woki_ext_event_read_f32_le(p + 56);
    out->total_x = woki_ext_event_read_f32_le(p + 60);
    out->total_y = woki_ext_event_read_f32_le(p + 64);
    out->velocity_x = woki_ext_event_read_f32_le(p + 68);
    out->velocity_y = woki_ext_event_read_f32_le(p + 72);
}

static inline int32_t woki_ext_decode_pinch_payload(const uint8_t* p, uint32_t len, woki_ext_pinch_event_t* out) {
    if (!out || len != 84u || woki_ext_decode_event_metadata(p, len, &out->gesture.metadata) != WOKI_EXT_OK)
        return WOKI_EXT_INVALID;
    woki_ext_decode_gesture_fields(p, &out->gesture);
    out->scale_delta = woki_ext_event_read_f32_le(p + 76);
    out->scale = woki_ext_event_read_f32_le(p + 80);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_rotate_payload(const uint8_t* p, uint32_t len, woki_ext_rotate_event_t* out) {
    if (!out || len != 84u || woki_ext_decode_event_metadata(p, len, &out->gesture.metadata) != WOKI_EXT_OK)
        return WOKI_EXT_INVALID;
    woki_ext_decode_gesture_fields(p, &out->gesture);
    out->radians_delta = woki_ext_event_read_f32_le(p + 76);
    out->radians = woki_ext_event_read_f32_le(p + 80);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_device_payload(const uint8_t* p, uint32_t len, woki_ext_device_event_t* out) {
    return woki_ext_decode_metadata_payload(p, len, (woki_ext_metadata_event_t*)out);
}

static inline int32_t woki_ext_decode_gamepad_connection_payload(const uint8_t* p, uint32_t len, woki_ext_gamepad_connection_event_t* out) {
    if (!out || len < 88u || woki_ext_decode_event_metadata(p, len, &out->metadata) != WOKI_EXT_OK)
        return WOKI_EXT_INVALID;
    out->mapped = p[40];
    out->buttons = woki_ext_event_read_u16_le(p + 44);
    for (uint32_t i = 0; i < 6; i++)
        out->axes[i] = woki_ext_event_read_f32_le(p + 48 + i * 4);
    out->name_size = woki_ext_event_read_u32_le(p + 72);
    out->guid_size = woki_ext_event_read_u32_le(p + 76);
    out->raw_axis_count = woki_ext_event_read_u16_le(p + 80);
    out->raw_button_count = woki_ext_event_read_u16_le(p + 82);
    out->raw_hat_count = woki_ext_event_read_u16_le(p + 84);
    const uint32_t raw_axes_size = (uint32_t)out->raw_axis_count * 4u;
    const uint32_t variable_size = out->name_size + out->guid_size + raw_axes_size + (uint32_t)out->raw_button_count + (uint32_t)out->raw_hat_count;
    if (variable_size != len - 88u)
        return WOKI_EXT_INVALID;
    out->name = p + 88;
    out->guid = out->name + out->name_size;
    out->raw_axes = out->guid + out->guid_size;
    out->raw_buttons = out->raw_axes + raw_axes_size;
    out->raw_hats = out->raw_buttons + out->raw_button_count;
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_gamepad_button_payload(const uint8_t* p, uint32_t len, woki_ext_gamepad_button_event_t* out) {
    if (!out)
        return WOKI_EXT_INVALID;
    WOKI_EXT_BEGIN(48u);
    out->button = p[40];
    out->pressed = p[41];
    out->value = woki_ext_event_read_f32_le(p + 44);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_gamepad_axis_payload(const uint8_t* p, uint32_t len, woki_ext_gamepad_axis_event_t* out) {
    if (!out)
        return WOKI_EXT_INVALID;
    WOKI_EXT_BEGIN(48u);
    out->axis = p[40];
    out->value = woki_ext_event_read_f32_le(p + 44);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_joystick_button_payload(const uint8_t* p, uint32_t len, woki_ext_joystick_button_event_t* out) {
    if (!out)
        return WOKI_EXT_INVALID;
    WOKI_EXT_BEGIN(44u);
    out->index = woki_ext_event_read_u16_le(p + 40);
    out->pressed = p[42];
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_joystick_axis_payload(const uint8_t* p, uint32_t len, woki_ext_joystick_axis_event_t* out) {
    if (!out)
        return WOKI_EXT_INVALID;
    WOKI_EXT_BEGIN(48u);
    out->index = woki_ext_event_read_u16_le(p + 40);
    out->value = woki_ext_event_read_f32_le(p + 44);
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_joystick_hat_payload(const uint8_t* p, uint32_t len, woki_ext_joystick_hat_event_t* out) {
    if (!out)
        return WOKI_EXT_INVALID;
    WOKI_EXT_BEGIN(44u);
    out->index = woki_ext_event_read_u16_le(p + 40);
    out->value = p[42];
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_text_list_payload(const uint8_t* p, uint32_t len, woki_ext_text_list_event_t* out) {
    if (!out || len < 48u || woki_ext_decode_event_metadata(p, len, &out->metadata) != WOKI_EXT_OK)
        return WOKI_EXT_INVALID;
    out->count = woki_ext_event_read_u32_le(p + 40);
    out->data_size = woki_ext_event_read_u32_le(p + 44);
    if (out->data_size != len - 48u)
        return WOKI_EXT_INVALID;
    out->data = p + 48;
    return WOKI_EXT_OK;
}

static inline int32_t woki_ext_decode_platform_error_payload(const uint8_t* p, uint32_t len, woki_ext_platform_error_event_t* out) {
    if (!out || len < 48u || woki_ext_decode_event_metadata(p, len, &out->metadata) != WOKI_EXT_OK)
        return WOKI_EXT_INVALID;
    out->code = woki_ext_event_read_i32_le(p + 40);
    out->description_size = woki_ext_event_read_u32_le(p + 44);
    if (out->description_size != len - 48u)
        return WOKI_EXT_INVALID;
    out->description = p + 48;
    return WOKI_EXT_OK;
}

#undef WOKI_EXT_BEGIN

#define WOKI_EXT_EVENT(cpp_name, c_name, name, id, layout)                                                                                                                                                                 \
    static inline int32_t woki_ext_decode_##name##_event(const uint8_t* p, uint32_t len, woki_ext_##name##_event_t* out) {                                                                                                 \
        return woki_ext_decode_##layout##_payload(p, len, out);                                                                                                                                                            \
    }
#include "event_schema.def"
#undef WOKI_EXT_EVENT

static inline int32_t woki_ext_event_is_wildcard(woki_ext_event_type_t type) {
    return type == WOKI_EXT_EVENT_WILDCARD;
}

static inline woki_ext_event_type_t woki_ext_extension_event_id(uint32_t local_id) {
    return WOKI_EXT_EVENT_EXTENSION_ID(local_id);
}
