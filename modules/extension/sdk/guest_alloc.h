#pragma once

#include <stdint.h>

#include "macros.h"
#include "woki_limits.h"

#ifdef __cplusplus
extern "C" {
#endif

static uint8_t g_woki_guest_buffer[WOKI_EXT_GUEST_BUFFER_SIZE];

WOKI_EXPORT("ext_alloc")

uint32_t ext_alloc(uint32_t len) {
    if (len == 0 || len > sizeof(g_woki_guest_buffer)) {
        return 0;
    }
#if defined(__cplusplus)
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_woki_guest_buffer));
#else
    return (uint32_t)(uintptr_t)g_woki_guest_buffer;
#endif
}

WOKI_EXPORT("ext_free")

void ext_free(uint32_t ptr, uint32_t len) {
    (void)ptr;
    (void)len;
}

#ifdef __cplusplus
}
#endif
