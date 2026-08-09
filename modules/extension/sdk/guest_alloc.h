#pragma once

#include <stdint.h>

#include "macros.h"
#include "woki_limits.h"

#ifdef __cplusplus
extern "C" {
#endif

static uint8_t g_woki_guest_buffers[2][WOKI_EXT_GUEST_BUFFER_SIZE];
static uint8_t g_woki_guest_buffer_used[2];

WOKI_EXPORT("ext_alloc")

uint32_t ext_alloc(uint32_t len) {
    if (len == 0 || len > WOKI_EXT_GUEST_BUFFER_SIZE) {
        return 0;
    }
    uint32_t slot = 0;
    while (slot < 2 && g_woki_guest_buffer_used[slot]) {
        ++slot;
    }
    if (slot == 2) {
        return 0;
    }
    g_woki_guest_buffer_used[slot] = 1;
#if defined(__cplusplus)
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_woki_guest_buffers[slot]));
#else
    return (uint32_t)(uintptr_t)g_woki_guest_buffers[slot];
#endif
}

WOKI_EXPORT("ext_free")

void ext_free(uint32_t ptr, uint32_t len) {
    (void)len;
    for (uint32_t slot = 0; slot < 2; ++slot) {
        if (ptr == (uint32_t)(uintptr_t)g_woki_guest_buffers[slot]) {
            g_woki_guest_buffer_used[slot] = 0;
            return;
        }
    }
}

#ifdef __cplusplus
}
#endif
