#pragma once

#include <stdint.h>

#include "abi.h"
#include "macros.h"

#ifdef __cplusplus
extern "C" {
#endif

// Required exports.
WOKI_EXPORT("ext_api_version")
uint32_t ext_api_version(void);
WOKI_EXPORT("ext_init")
int32_t ext_init(void);
WOKI_EXPORT("ext_on_tick")
void ext_on_tick(double dt_ms);
WOKI_EXPORT("ext_on_event")
void ext_on_event(woki_ext_event_type_t type, const uint8_t* payload, uint32_t len);
// Optional. Hosts use this callback for exact named-topic delivery.
WOKI_EXPORT("ext_on_event_named")
void ext_on_event_named(const char* name, uint32_t name_len, const uint8_t* payload, uint32_t len);
WOKI_EXPORT("ext_on_unload")
void ext_on_unload(void);

// Required only when the manifest contributes commands.
WOKI_EXPORT("ext_on_command")
int32_t ext_on_command(const char* command_id, uint32_t command_len, const uint8_t* payload, uint32_t len);

// Optional as a pair. Required to receive any non-empty host-owned payload.
WOKI_EXPORT("ext_alloc")
uint32_t ext_alloc(uint32_t len);
WOKI_EXPORT("ext_free")
void ext_free(uint32_t ptr, uint32_t len);

#ifdef __cplusplus
}
#endif
