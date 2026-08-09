
#include "woki/ext/wasm/web_engine.hpp"

#ifdef __EMSCRIPTEN__
#include "perm_bits.h"

extern "C" {
int woki_web_ext_load(const char* id, const char* wasm_path, const char* data_path, const char* cache_path, unsigned permissions);
int woki_web_ext_api_version(const char* id);
int woki_web_ext_init(const char* id);
int woki_web_ext_tick(const char* id, double delta_ms);
int woki_web_ext_event(const char* id, unsigned event_type, const unsigned char* payload, unsigned payload_len);
int woki_web_ext_command(const char* id, const char* command_id, const unsigned char* payload, unsigned payload_len);
void woki_web_ext_unload(const char* id);
const char* woki_web_ext_last_error();
}

#endif

namespace woki::ext::wasm {

namespace {

#ifdef __EMSCRIPTEN__

enum PermissionBits : unsigned {
    kLog = WOKI_EXT_PERM_LOG,
    kPaths = WOKI_EXT_PERM_PATHS,
    kStorage = WOKI_EXT_PERM_STORAGE,
    kConfig = WOKI_EXT_PERM_CONFIG,
    kEvents = WOKI_EXT_PERM_EVENTS,
};

[[nodiscard]] unsigned PermissionMask(const Manifest& manifest) noexcept {
    unsigned mask = 0;
    if (HasPermission(manifest, Permission::Log)) {
        mask |= kLog;
    }
    if (HasPermission(manifest, Permission::Paths)) {
        mask |= kPaths;
    }
    if (HasPermission(manifest, Permission::Storage)) {
        mask |= kStorage;
    }
    if (HasPermission(manifest, Permission::Config)) {
        mask |= kConfig;
    }
    if (HasPermission(manifest, Permission::Events)) {
        mask |= kEvents;
    }
    return mask;
}

[[nodiscard]] Error WebError(std::string_view context) {
    const char* detail = woki_web_ext_last_error();
    if (detail != nullptr && detail[0] != '\0') {
        return Error(ErrorCode::InvalidState, std::string(context) + ": " + detail);
    }
    return Error(ErrorCode::InvalidState, std::string(context));
}

#endif

#ifndef __EMSCRIPTEN__
[[nodiscard]] Error Unsupported() {
    return Error(ErrorCode::InvalidState, "WebEngine is only available when building Woki with Emscripten.");
}
#endif

} // namespace

Result<void> WebEngine::Load(Record& record, host::HostApi& host) {
    (void)host;
#ifdef __EMSCRIPTEN__
    const std::string wasm_path = record.package.wasm.generic_string();
    const std::string data_path = record.package.data_root.generic_string();
    const std::string cache_path = record.package.cache_root.generic_string();
    const int loaded = woki_web_ext_load(record.id.c_str(), wasm_path.c_str(), data_path.c_str(), cache_path.c_str(), PermissionMask(record.manifest));
    if (loaded != 0) {
        return Err(WebError("Failed to load web extension"));
    }
    return Ok();
#else
    (void)record;
    return Err(Unsupported());
#endif
}

Result<u32> WebEngine::ApiVersion(Record& record) {
#ifdef __EMSCRIPTEN__
    const int api_version = woki_web_ext_api_version(record.id.c_str());
    if (api_version < 0) {
        return Err(WebError("Failed to read web extension apiVersion"));
    }
    return Ok(static_cast<u32>(api_version));
#else
    (void)record;
    return Err(Unsupported());
#endif
}

Result<i32> WebEngine::Init(Record& record) {
#ifdef __EMSCRIPTEN__
    const int initialized = woki_web_ext_init(record.id.c_str());
    if (initialized != 0) {
        return Err(WebError("Extension ext_init failed"));
    }
    return Ok(0);
#else
    (void)record;
    return Err(Unsupported());
#endif
}

Result<void> WebEngine::Tick(Record& record, f64 delta_ms) {
#ifdef __EMSCRIPTEN__
    if (woki_web_ext_tick(record.id.c_str(), delta_ms) != 0) {
        return Err(WebError("Extension ext_on_tick failed"));
    }
    return Ok();
#else
    (void)record;
    (void)delta_ms;
    return Err(Unsupported());
#endif
}

Result<void> WebEngine::Event(Record& record, u32 event_type, std::span<const u8> payload) {
#ifdef __EMSCRIPTEN__
    const int dispatched = woki_web_ext_event(record.id.c_str(), event_type, payload.data(), static_cast<unsigned>(payload.size()));
    if (dispatched != 0) {
        return Err(WebError("Extension ext_on_event failed"));
    }
    return Ok();
#else
    (void)record;
    (void)event_type;
    (void)payload;
    return Err(Unsupported());
#endif
}

Result<i32> WebEngine::Command(Record& record, std::string_view command_id, std::span<const u8> payload) {
#ifdef __EMSCRIPTEN__
    const std::string owned_command_id(command_id);
    const int dispatched = woki_web_ext_command(record.id.c_str(), owned_command_id.c_str(), payload.data(), static_cast<unsigned>(payload.size()));
    if (dispatched < 0) {
        return Err(WebError("Extension ext_on_command failed"));
    }
    return Ok(dispatched);
#else
    (void)record;
    (void)command_id;
    (void)payload;
    return Err(Unsupported());
#endif
}

void WebEngine::Discard(Record& record) {
#ifdef __EMSCRIPTEN__
    woki_web_ext_unload(record.id.c_str());
#else
    (void)record;
#endif
}

void WebEngine::Unload(Record& record) {
#ifdef __EMSCRIPTEN__
    woki_web_ext_unload(record.id.c_str());
#else
    (void)record;
#endif
}

} // namespace woki::ext::wasm
