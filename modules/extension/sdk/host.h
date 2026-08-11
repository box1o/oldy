#pragma once

#include <stdint.h>

#include "abi.h"
#include "macros.h"

#ifdef __cplusplus
extern "C" {
#endif

// Stable raw ABI v1 imports. host_imports.h remains as a compatibility include.

WOKI_IMPORT(WOKI_EXT_IMPORT_MODULE, "host_log")
int32_t host_log(woki_ext_log_level_t level, const char* message, uint32_t len);

WOKI_IMPORT(WOKI_EXT_IMPORT_MODULE, "host_path_data")
int32_t host_path_data(char* out, uint32_t out_cap);
WOKI_IMPORT(WOKI_EXT_IMPORT_MODULE, "host_path_cache")
int32_t host_path_cache(char* out, uint32_t out_cap);

WOKI_IMPORT(WOKI_EXT_IMPORT_MODULE, "host_file_read")
int32_t host_file_read(const char* rel_path, uint8_t* out, uint32_t* inout_len);
WOKI_IMPORT(WOKI_EXT_IMPORT_MODULE, "host_file_write")
int32_t host_file_write(const char* rel_path, const uint8_t* data, uint32_t len);
WOKI_IMPORT(WOKI_EXT_IMPORT_MODULE, "host_file_append")
int32_t host_file_append(const char* rel_path, const uint8_t* data, uint32_t len);

// Prefer these length-prefixed variants in new guests.
WOKI_IMPORT(WOKI_EXT_IMPORT_MODULE, "host_file_read_n")
int32_t host_file_read_n(const char* rel_path, uint32_t rel_path_len, uint8_t* out, uint32_t* inout_len);
WOKI_IMPORT(WOKI_EXT_IMPORT_MODULE, "host_file_write_n")
int32_t host_file_write_n(const char* rel_path, uint32_t rel_path_len, const uint8_t* data, uint32_t len);
WOKI_IMPORT(WOKI_EXT_IMPORT_MODULE, "host_file_append_n")
int32_t host_file_append_n(const char* rel_path, uint32_t rel_path_len, const uint8_t* data, uint32_t len);

WOKI_IMPORT(WOKI_EXT_IMPORT_MODULE, "host_config_get")
int32_t host_config_get(const char* key, char* out, uint32_t out_cap);
WOKI_IMPORT(WOKI_EXT_IMPORT_MODULE, "host_config_set")
int32_t host_config_set(const char* key, const char* value, uint32_t len);

WOKI_IMPORT(WOKI_EXT_IMPORT_MODULE, "host_event_subscribe")
// Compatibility no-op. The events manifest permission delivers all public
// application events; subscriptions do not filter delivery.
int32_t host_event_subscribe(woki_ext_event_type_t type);
WOKI_IMPORT(WOKI_EXT_IMPORT_MODULE, "host_event_emit")
// Emitted types must include WOKI_EXT_EVENT_NAMESPACE. Delivery is queued
// until the current guest callback returns.
int32_t host_event_emit(woki_ext_event_type_t type, const uint8_t* payload, uint32_t len);

// Named topics are collision-free reverse-DNS strings. The subscription call
// is a compatibility no-op; emissions must be owned by the extension id.
WOKI_IMPORT(WOKI_EXT_IMPORT_MODULE, "host_event_subscribe_named")
int32_t host_event_subscribe_named(const char* name, uint32_t name_len);
WOKI_IMPORT(WOKI_EXT_IMPORT_MODULE, "host_event_emit_named")
int32_t host_event_emit_named(const char* name, uint32_t name_len, const uint8_t* payload, uint32_t len);

static inline int32_t woki_ext_subscribe_all_events(void) {
    return host_event_subscribe(WOKI_EXT_EVENT_WILDCARD);
}

#ifdef __cplusplus
}
#endif
