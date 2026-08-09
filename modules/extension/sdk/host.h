#pragma once

#include <stdint.h>

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Guest wasm modules should include host_imports.h for import attributes.
// These declarations document the stable C ABI.

int32_t host_log(woki_ext_log_level_t level, const char* message, uint32_t len);

int32_t host_path_data(char* out, uint32_t out_cap);
int32_t host_path_cache(char* out, uint32_t out_cap);

int32_t host_file_read(const char* rel_path, uint8_t* out, uint32_t* inout_len);
int32_t host_file_write(const char* rel_path, const uint8_t* data, uint32_t len);
int32_t host_file_append(const char* rel_path, const uint8_t* data, uint32_t len);

// Length-prefixed path variants are also available from the host as host_file_*_n.

int32_t host_config_get(const char* key, char* out, uint32_t out_cap);
int32_t host_config_set(const char* key, const char* value, uint32_t len);

int32_t host_event_subscribe(woki_ext_event_type_t type);
int32_t host_event_emit(woki_ext_event_type_t type, const uint8_t* payload, uint32_t len);

#ifdef __cplusplus
}
#endif
