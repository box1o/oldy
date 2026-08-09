#include "ext.h"
#include "version.h"
#include "guest_alloc.h"
#include "host_imports.h"

namespace {

[[nodiscard]] bool CommandIs(const char* id, uint32_t len, const char* lit, uint32_t lit_len) {
    if (id == nullptr || len != lit_len) {
        return false;
    }
    for (uint32_t i = 0; i < len; ++i) {
        if (id[i] != lit[i]) {
            return false;
        }
    }
    return true;
}

} // namespace

extern "C" {

WOKI_EXPORT("ext_api_version")

uint32_t ext_api_version(void) {
    return WOKI_EXT_API_VERSION;
}

WOKI_EXPORT("ext_init")

int32_t ext_init(void) {
    // nf-md-cat U+F011B — UTF-8 must be \xF3\xB0\x84\x9B (not \xF3\x90\x84\x9B)
    static constexpr char kMessage[] = "\xF3\xB0\x84\x9B kitty says hi!";
    return host_log(WOKI_EXT_LOG_INFO, kMessage, sizeof(kMessage) - 1);
}

WOKI_EXPORT("ext_on_tick")

void ext_on_tick(double dt_ms) {
    (void)dt_ms;
}

WOKI_EXPORT("ext_on_event")

void ext_on_event(uint32_t type, const uint8_t* payload, uint32_t len) {
    (void)type;
    (void)payload;
    (void)len;
}

WOKI_EXPORT("ext_on_command")

int32_t ext_on_command(const char* command_id, uint32_t command_len, const uint8_t* payload, uint32_t len) {
    (void)payload;
    (void)len;

    static constexpr char kPet[] = "woki.kitty.pet";
    if (CommandIs(command_id, command_len, kPet, sizeof(kPet) - 1)) {
        static constexpr char kMessage[] = "\xF3\xB0\x84\x9B *purrr*";
        return host_log(WOKI_EXT_LOG_INFO, kMessage, sizeof(kMessage) - 1);
    }

    static constexpr char kComplex[] = "woki.kitty.complex";
    if (CommandIs(command_id, command_len, kComplex, sizeof(kComplex) - 1)) {
        static constexpr char kMessage[] = "\xF3\xB0\x84\x9B complex command!";
        return host_log(WOKI_EXT_LOG_INFO, kMessage, sizeof(kMessage) - 1);
    }

    return -1;
}

WOKI_EXPORT("ext_on_unload")

void ext_on_unload(void) {}

} // extern "C"
