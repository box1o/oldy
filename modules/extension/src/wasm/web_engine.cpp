#include <array>
#include <atomic>
#include <string>

#include "woki/ext/host/cabi.hpp"
#include "woki/ext/wasm/web_engine.hpp"
#include "woki/ext/wasm/guest_module.hpp"
#if defined(WOKI_EXTENSION_WITH_WASMTIME)
#include "woki/ext/wasm/wasmtime_engine.hpp"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

#include "types.h"
#include "perm_bits.h"
extern "C" {
int woki_web_ext_load(const char*, const char*, const unsigned char*, unsigned, const char*, const char*, const char*, unsigned, unsigned);
int woki_web_ext_api_version(const char*);
int woki_web_ext_init(const char*);
int woki_web_ext_tick(const char*, double);
int woki_web_ext_event(const char*, unsigned, const unsigned char*, unsigned);
int woki_web_ext_event_named(const char*, const char*, unsigned, const unsigned char*, unsigned);
int woki_web_ext_command(const char*, const char*, const unsigned char*, unsigned);
void woki_web_ext_unload(const char*);
void woki_web_ext_discard(const char*);
int woki_web_ext_last_error(const char*, char*, unsigned);
}

extern "C" EMSCRIPTEN_KEEPALIVE int woki_web_host_event_subscribe(woki::ext::host::HostApi* host, unsigned event_type) {
    return woki::ext::host::cabi::EventSubscribe(*host, event_type);
}

extern "C" EMSCRIPTEN_KEEPALIVE int woki_web_host_event_emit(woki::ext::host::HostApi* host, unsigned event_type, const unsigned char* payload, unsigned len) {
    return woki::ext::host::cabi::EventEmit(*host, event_type, payload, len);
}

extern "C" EMSCRIPTEN_KEEPALIVE int woki_web_host_event_subscribe_named(woki::ext::host::HostApi* host, const char* name, unsigned name_len) {
    return woki::ext::host::cabi::EventSubscribeNamed(*host, name, name_len);
}

extern "C" EMSCRIPTEN_KEEPALIVE int woki_web_host_event_emit_named(woki::ext::host::HostApi* host, const char* name, unsigned name_len, const unsigned char* payload, unsigned len) {
    return woki::ext::host::cabi::EventEmitNamed(*host, name, name_len, payload, len);
}
#endif

namespace woki::ext::wasm {
namespace {

#ifdef __EMSCRIPTEN__
[[nodiscard]] Error WebError(std::string_view handle, std::string_view context) {
    std::array<char, 4096> detail{};
    const std::string handle_text(handle);
    woki_web_ext_last_error(handle_text.c_str(), detail.data(), static_cast<unsigned>(detail.size()));
    return Error(ErrorCode::InvalidState, detail[0] != '\0' ? std::string(context) + ": " + detail.data() : std::string(context));
}

[[nodiscard]] Error GuestCommandError(int status) {
    const std::string message = "Extension command returned guest status " + std::to_string(status) + ".";
    switch (status) {
        case WOKI_EXT_ERR:
            return Error(ErrorCode::UnknownError, message);
        case WOKI_EXT_DENIED:
            return Error(ErrorCode::FileAccessDenied, message);
        case WOKI_EXT_NO_SPACE:
            return Error(ErrorCode::ValidationOutOfRange, message);
        case WOKI_EXT_NOT_FOUND:
            return Error(ErrorCode::FileNotFound, message);
        case WOKI_EXT_INVALID:
            return Error(ErrorCode::InvalidArgument, message);
        default:
            return Error(ErrorCode::ValidationInvalidState, "Extension command returned unknown status " + std::to_string(status) + ".");
    }
}

[[nodiscard]] std::string NextSessionHandle() {
    static std::atomic_uint64_t next{0};
    return "woki-web-session-" + std::to_string(next.fetch_add(1, std::memory_order_relaxed));
}

[[nodiscard]] unsigned PermissionMask(const host::HostApi& host) noexcept {
    unsigned mask = 0;
    if (host.Allows(Permission::Log))
        mask |= WOKI_EXT_PERM_LOG;
    if (host.Allows(Permission::Paths))
        mask |= WOKI_EXT_PERM_PATHS;
    if (host.Allows(Permission::Storage))
        mask |= WOKI_EXT_PERM_STORAGE;
    if (host.Allows(Permission::Config))
        mask |= WOKI_EXT_PERM_CONFIG;
    if (host.Allows(Permission::Events))
        mask |= WOKI_EXT_PERM_EVENTS;
    return mask;
}

class WebInstance final : public RuntimeInstance {
public:
    WebInstance(std::string handle, host::HostApi host)
        : handle_(std::move(handle)),
          host_(std::move(host)) {}

    Result<void> Load(const ExtensionPackage& package, std::span<const u8> wasm) {
        const PackageLayout& layout = package.Layout();
        const int loaded = woki_web_ext_load(handle_.c_str(), package.Id().c_str(), wasm.data(), static_cast<unsigned>(wasm.size()), layout.data_root.generic_string().c_str(), layout.config_root.generic_string().c_str(),
            layout.cache_root.generic_string().c_str(), PermissionMask(host_), reinterpret_cast<unsigned>(&host_));
        return loaded == 0 ? Ok() : Err(WebError(handle_, "Failed to load web extension"));
    }

    ~WebInstance() override {
        Unload();
    }

    Result<void> Initialize() override {
        if (woki_web_ext_init(handle_.c_str()) != 0)
            return Err(WebError(handle_, "Extension ext_init failed"));
        return Ok();
    }

    Result<void> Tick(f64 delta_ms) override {
        if (woki_web_ext_tick(handle_.c_str(), delta_ms) != 0)
            return Err(WebError(handle_, "Extension ext_on_tick failed"));
        return Ok();
    }

    Result<void> DispatchEvent(u32 type, std::span<const u8> payload) override {
        if (woki_web_ext_event(handle_.c_str(), type, payload.data(), static_cast<unsigned>(payload.size())) != 0)
            return Err(WebError(handle_, "Extension ext_on_event failed"));
        return Ok();
    }

    Result<void> DispatchNamedEvent(std::string_view topic, std::span<const u8> payload) override {
        if (woki_web_ext_event_named(handle_.c_str(), topic.data(), static_cast<unsigned>(topic.size()), payload.data(), static_cast<unsigned>(payload.size())) != 0)
            return Err(WebError(handle_, "Extension ext_on_event_named failed"));
        return Ok();
    }

    Result<void> DispatchCommand(std::string_view command, std::span<const u8> payload) override {
        const std::string id(command);
        const int result = woki_web_ext_command(handle_.c_str(), id.c_str(), payload.data(), static_cast<unsigned>(payload.size()));
        if (result == INT32_MIN)
            return Err(WebError(handle_, "Extension ext_on_command failed"));
        if (result != WOKI_EXT_OK)
            return Err(GuestCommandError(result));
        return Ok();
    }

    void Unload() noexcept override {
        if (!unloaded_) {
            woki_web_ext_unload(handle_.c_str());
            unloaded_ = true;
        }
    }

private:
    std::string handle_;
    host::HostApi host_;
    bool unloaded_{false};
};
#endif
} // namespace

WebEngine::WebEngine(bool allow_trusted_synchronous_execution) noexcept
#ifdef __EMSCRIPTEN__
    : allow_trusted_synchronous_execution_(allow_trusted_synchronous_execution)
#endif
{
#ifndef __EMSCRIPTEN__
    (void)allow_trusted_synchronous_execution;
#endif
}

Result<scope<RuntimeInstance>> WebEngine::Create(const ExtensionPackage& package, host::HostApi host) {
#ifdef __EMSCRIPTEN__
    if (!allow_trusted_synchronous_execution_)
        return Err(ErrorCode::InvalidState, "WebEngine synchronous guest execution is disabled; explicitly opt in only for trusted wasm.");
    auto bytes = LoadGuestModule(package.Layout().wasm);
    if (!bytes)
        return Err(bytes.error());
    auto valid = ValidateGuestModule(*bytes, package.GetManifest());
    if (!valid)
        return Err(valid.error());
    const std::string handle = NextSessionHandle();
    auto instance = createScope<WebInstance>(handle, std::move(host));
    if (auto loaded = instance->Load(package, *bytes); !loaded) {
        const Error error = loaded.error();
        woki_web_ext_discard(handle.c_str());
        return Err(error);
    }
    const int version = woki_web_ext_api_version(handle.c_str());
    if (version < 0) {
        const Error error = WebError(handle, "Failed to read web extension apiVersion");
        woki_web_ext_discard(handle.c_str());
        return Err(error);
    }
    if (static_cast<u32>(version) != package.GetManifest().api_version) {
        woki_web_ext_discard(handle.c_str());
        return Err(ErrorCode::ValidationInvalidState, "Extension apiVersion mismatch.");
    }
    return Ok(scope<RuntimeInstance>(std::move(instance)));
#else
    (void)host;
    (void)package;
    return Err(ErrorCode::InvalidState, "WebEngine is only available when building Woki with Emscripten.");
#endif
}

scope<RuntimeEngine> CreateEngine(bool allow_trusted_synchronous_web) {
#ifdef __EMSCRIPTEN__
    return createScope<WebEngine>(allow_trusted_synchronous_web);
#elif defined(WOKI_EXTENSION_WITH_WASMTIME)
    (void)allow_trusted_synchronous_web;
    return createScope<WasmtimeEngine>();
#else
    (void)allow_trusted_synchronous_web;
    return {};
#endif
}

} // namespace woki::ext::wasm
