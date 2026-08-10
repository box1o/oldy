#pragma once

#include <stdint.h>

#include "ext.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WOKI_EXT_GUEST_BUFFER_COUNT
#define WOKI_EXT_GUEST_BUFFER_COUNT 2u
#endif

#if WOKI_EXT_GUEST_BUFFER_COUNT < 2
#error "WOKI_EXT_GUEST_BUFFER_COUNT must be at least 2 for command delivery"
#endif

struct woki_ext_guest_alloc_state {
    uint8_t buffers[WOKI_EXT_GUEST_BUFFER_COUNT][WOKI_EXT_GUEST_BUFFER_SIZE];
    uint32_t lengths[WOKI_EXT_GUEST_BUFFER_COUNT];
};

// C++ inline definitions and C weak definitions let this compatibility header
// remain include-only while coalescing state in multi-translation-unit guests.
#if defined(__cplusplus) && __cplusplus >= 201703L
#define WOKI_GUEST_ALLOC_DEFINITION inline
inline struct woki_ext_guest_alloc_state woki_ext_guest_alloc_state_instance = {};
#else
#define WOKI_GUEST_ALLOC_DEFINITION WOKI_WEAK
WOKI_WEAK struct woki_ext_guest_alloc_state woki_ext_guest_alloc_state_instance = {{0}, {0}};
#endif

WOKI_GUEST_ALLOC_DEFINITION
uint32_t ext_alloc(uint32_t len) {
    if (len == 0 || len > WOKI_EXT_GUEST_BUFFER_SIZE) {
        return 0;
    }
    uint32_t slot = 0;
    while (slot < WOKI_EXT_GUEST_BUFFER_COUNT && woki_ext_guest_alloc_state_instance.lengths[slot] != 0) {
        ++slot;
    }
    if (slot == WOKI_EXT_GUEST_BUFFER_COUNT) {
        return 0;
    }
    woki_ext_guest_alloc_state_instance.lengths[slot] = len;
#if defined(__cplusplus)
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(woki_ext_guest_alloc_state_instance.buffers[slot]));
#else
    return (uint32_t)(uintptr_t)woki_ext_guest_alloc_state_instance.buffers[slot];
#endif
}

WOKI_GUEST_ALLOC_DEFINITION
void ext_free(uint32_t ptr, uint32_t len) {
    for (uint32_t slot = 0; slot < WOKI_EXT_GUEST_BUFFER_COUNT; ++slot) {
        if (ptr == (uint32_t)(uintptr_t)woki_ext_guest_alloc_state_instance.buffers[slot] && len == woki_ext_guest_alloc_state_instance.lengths[slot]) {
            woki_ext_guest_alloc_state_instance.lengths[slot] = 0;
            return;
        }
    }
}

#undef WOKI_GUEST_ALLOC_DEFINITION

#ifdef __cplusplus
}
#endif
