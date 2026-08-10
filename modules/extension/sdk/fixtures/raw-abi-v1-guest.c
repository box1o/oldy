#include "ext.h"
#include "host.h"

static uint8_t buffer[64];

uint32_t ext_api_version(void) {
    return WOKI_EXT_API_VERSION;
}

int32_t ext_init(void) {
    uint32_t size = sizeof(buffer);
    int32_t status = host_log(WOKI_EXT_LOG_INFO, "fixture", 7);
    status += host_path_data((char*)buffer, sizeof(buffer));
    status += host_path_cache((char*)buffer, sizeof(buffer));
    status += host_file_read("file", buffer, &size);
    status += host_file_write("file", buffer, size);
    status += host_file_append("file", buffer, size);
    status += host_file_read_n("file", 4, buffer, &size);
    status += host_file_write_n("file", 4, buffer, size);
    status += host_file_append_n("file", 4, buffer, size);
    status += host_config_get("key", (char*)buffer, sizeof(buffer));
    status += host_config_set("key", "value", 5);
    status += host_event_subscribe(WOKI_EXT_EVENT_WILDCARD);
    status += host_event_emit(WOKI_EXT_EVENT_EXTENSION_ID(1), buffer, size);
    status += host_event_subscribe_named("org.example.ready", 17);
    status += host_event_emit_named("org.example.fixture.ready", 25, buffer, size);
    return status;
}

void ext_on_tick(double dt_ms) {
    (void)dt_ms;
}

void ext_on_event(woki_ext_event_type_t type, const uint8_t* payload, uint32_t len) {
    (void)type;
    (void)payload;
    (void)len;
}

void ext_on_event_named(const char* name, uint32_t name_len, const uint8_t* payload, uint32_t len) {
    (void)name;
    (void)name_len;
    (void)payload;
    (void)len;
}

void ext_on_unload(void) {}

int32_t ext_on_command(const char* command_id, uint32_t command_len, const uint8_t* payload, uint32_t len) {
    (void)command_id;
    (void)command_len;
    (void)payload;
    (void)len;
    return WOKI_EXT_OK;
}

uint32_t ext_alloc(uint32_t len) {
    return len <= sizeof(buffer) ? (uint32_t)(uintptr_t)buffer : 0;
}

void ext_free(uint32_t ptr, uint32_t len) {
    (void)ptr;
    (void)len;
}
