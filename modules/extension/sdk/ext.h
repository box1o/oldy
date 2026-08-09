#pragma once

#include <stdint.h>

#include "version.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t ext_api_version(void);
int32_t ext_init(void);
void ext_on_tick(double dt_ms);
void ext_on_event(uint32_t type, const uint8_t* payload, uint32_t len);
int32_t ext_on_command(const char* command_id, uint32_t command_len, const uint8_t* payload, uint32_t len);
void ext_on_unload(void);

// Optional. Export these when the extension wants non-empty event payloads.
uint32_t ext_alloc(uint32_t len);
void ext_free(uint32_t ptr, uint32_t len);

#ifdef __cplusplus
}
#endif
