#pragma once

#include <stdint.h>

#include "types.h"
#include "macros.h"

#ifdef __cplusplus
extern "C" {
#endif

WOKI_IMPORT("woki_host", "host_log")
int32_t host_log(woki_ext_log_level_t level, const char* message, uint32_t len);

WOKI_IMPORT("woki_host", "host_path_data")
int32_t host_path_data(char* out, uint32_t out_cap);

WOKI_IMPORT("woki_host", "host_path_cache")
int32_t host_path_cache(char* out, uint32_t out_cap);

WOKI_IMPORT("woki_host", "host_file_read")
int32_t host_file_read(const char* rel_path, uint8_t* out, uint32_t* inout_len);

WOKI_IMPORT("woki_host", "host_file_write")
int32_t host_file_write(const char* rel_path, const uint8_t* data, uint32_t len);

WOKI_IMPORT("woki_host", "host_file_append")
int32_t host_file_append(const char* rel_path, const uint8_t* data, uint32_t len);

WOKI_IMPORT("woki_host", "host_file_read_n")
int32_t host_file_read_n(const char* rel_path, uint32_t rel_path_len, uint8_t* out, uint32_t* inout_len);

WOKI_IMPORT("woki_host", "host_file_write_n")
int32_t host_file_write_n(const char* rel_path, uint32_t rel_path_len, const uint8_t* data, uint32_t len);

WOKI_IMPORT("woki_host", "host_file_append_n")
int32_t host_file_append_n(const char* rel_path, uint32_t rel_path_len, const uint8_t* data, uint32_t len);

WOKI_IMPORT("woki_host", "host_config_get")
int32_t host_config_get(const char* key, char* out, uint32_t out_cap);

WOKI_IMPORT("woki_host", "host_config_set")
int32_t host_config_set(const char* key, const char* value, uint32_t len);

WOKI_IMPORT("woki_host", "host_event_subscribe")
int32_t host_event_subscribe(woki_ext_event_type_t type);

WOKI_IMPORT("woki_host", "host_event_emit")
int32_t host_event_emit(woki_ext_event_type_t type, const uint8_t* payload, uint32_t len);

#ifdef __cplusplus
}
#endif
