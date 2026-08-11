#include <tuple>
#include <vector>
#include <variant>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <wasmtime.hh>
#include <unordered_map>

#include "woki/ext/limits.hpp"
#include "woki/ext/host/cabi.hpp"
#include "woki/ext/wasm/guest_module.hpp"
#include "woki/ext/wasm/wasmtime_engine.hpp"

#include "types.h"

namespace woki::ext::wasm {

namespace {

namespace fs = std::filesystem;

[[nodiscard]] Error GuestCommandError(int32_t status) {
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

constexpr std::string_view kImportModule = "woki_host";
inline constexpr std::size_t kMaxGuestCStringBytes = 4096u;
inline constexpr u64 kCallFuel = 10'000'000u;
inline constexpr u64 kMemoryReservationBytes = 32u * 1024u * 1024u;
inline constexpr u64 kMaxWasmStackBytes = 1u * 1024u * 1024u;
inline constexpr int64_t kMaxMemoryBytes = static_cast<int64_t>(limits::kMaxMemoryPages * 65536u);
inline constexpr int64_t kMaxTableElements = 10'000;
inline constexpr int64_t kMaxInstances = 1;
inline constexpr int64_t kMaxTables = 1;
inline constexpr int64_t kMaxMemories = 1;

template <typename T>
[[nodiscard]] Result<T> FromWasmtimeResult(wasmtime::Result<T>&& result, std::string_view context) {
    if (result) {
        return Ok(result.ok());
    }
    return Err(ErrorCode::InvalidState, std::string(context) + ": " + result.err().message());
}

template <typename T>
[[nodiscard]] Result<T> FromTrapResult(wasmtime::TrapResult<T>&& result, std::string_view context) {
    if (result) {
        return Ok(result.ok());
    }
    return Err(ErrorCode::InvalidState, std::string(context) + ": " + result.err().message());
}

[[nodiscard]] std::optional<wasmtime::Memory> GuestMemory(wasmtime::Caller& caller) {
    if (auto exported = caller.get_export("memory")) {
        if (const auto* memory = std::get_if<wasmtime::Memory>(&*exported)) {
            return *memory;
        }
    }
    return std::nullopt;
}

[[nodiscard]] Result<std::string_view> GuestBytes(wasmtime::Caller& caller, int32_t offset, int32_t len) {
    if (offset < 0 || len < 0) {
        return Err(ErrorCode::InvalidArgument, "Negative guest pointer or length.");
    }

    const auto memory = GuestMemory(caller);
    if (!memory) {
        return Err(ErrorCode::InvalidState, "Guest module does not export linear memory.");
    }

    const wasmtime::Span<uint8_t> data = memory->data(caller);
    const auto start = static_cast<std::size_t>(offset);
    const auto size = static_cast<std::size_t>(len);
    if (start > data.size() || size > data.size() - start) {
        return Err(ErrorCode::ValidationOutOfRange, "Guest memory read is out of bounds.");
    }
    if (size == 0 && data.data() == nullptr)
        return Ok(std::string_view{});

    return Ok(std::string_view(reinterpret_cast<const char*>(data.data() + start), size));
}

[[nodiscard]] Result<std::string_view> GuestCString(wasmtime::Caller& caller, int32_t offset) {
    if (offset < 0) {
        return Err(ErrorCode::InvalidArgument, "Negative guest string pointer.");
    }

    const auto memory = GuestMemory(caller);
    if (!memory) {
        return Err(ErrorCode::InvalidState, "Guest module does not export linear memory.");
    }

    const wasmtime::Span<uint8_t> data = memory->data(caller);
    const auto start = static_cast<std::size_t>(offset);
    if (start >= data.size()) {
        return Err(ErrorCode::ValidationOutOfRange, "Guest string pointer is out of bounds.");
    }

    std::size_t len = 0;
    while (start + len < data.size() && len <= kMaxGuestCStringBytes && data[start + len] != 0) {
        ++len;
    }
    if (len > kMaxGuestCStringBytes || start + len >= data.size()) {
        return Err(ErrorCode::ValidationOutOfRange, "Guest string is not null-terminated in bounds.");
    }

    return Ok(std::string_view(reinterpret_cast<const char*>(data.data() + start), len));
}

[[nodiscard]] Result<char*> GuestCStringOut(wasmtime::Caller& caller, int32_t offset, u32 cap) {
    if (offset < 0) {
        return Err(ErrorCode::InvalidArgument, "Negative guest output pointer.");
    }

    const auto memory = GuestMemory(caller);
    if (!memory) {
        return Err(ErrorCode::InvalidState, "Guest module does not export linear memory.");
    }

    const wasmtime::Span<uint8_t> data = memory->data(caller);
    const auto start = static_cast<std::size_t>(offset);
    if (start > data.size() || cap > data.size() - start) {
        return Err(ErrorCode::ValidationOutOfRange, "Guest memory write is out of bounds.");
    }
    if (cap == 0 && data.data() == nullptr)
        return Ok(static_cast<char*>(nullptr));

    return Ok(reinterpret_cast<char*>(data.data() + start));
}

[[nodiscard]] Result<std::span<u8>> GuestBytesOut(wasmtime::Caller& caller, int32_t offset, u32 len) {
    if (offset < 0) {
        return Err(ErrorCode::InvalidArgument, "Negative guest output pointer.");
    }

    const auto memory = GuestMemory(caller);
    if (!memory) {
        return Err(ErrorCode::InvalidState, "Guest module does not export linear memory.");
    }

    const wasmtime::Span<uint8_t> data = memory->data(caller);
    const auto start = static_cast<std::size_t>(offset);
    const auto size = static_cast<std::size_t>(len);
    if (start > data.size() || size > data.size() - start) {
        return Err(ErrorCode::ValidationOutOfRange, "Guest output buffer is out of bounds.");
    }
    if (size == 0 && data.data() == nullptr)
        return Ok(std::span<u8>{});

    return Ok(std::span<u8>(data.data() + start, size));
}

[[nodiscard]] Result<u32> GuestU32(wasmtime::Caller& caller, int32_t offset) {
    auto bytes = GuestBytesOut(caller, offset, sizeof(u32));
    if (!bytes) {
        return Err(bytes.error());
    }

    return Ok(static_cast<u32>((*bytes)[0]) | (static_cast<u32>((*bytes)[1]) << 8u) | (static_cast<u32>((*bytes)[2]) << 16u) | (static_cast<u32>((*bytes)[3]) << 24u));
}

[[nodiscard]] Result<void> WriteGuestU32(wasmtime::Caller& caller, int32_t offset, u32 value) {
    auto bytes = GuestBytesOut(caller, offset, sizeof(u32));
    if (!bytes) {
        return Err(bytes.error());
    }

    (*bytes)[0] = static_cast<u8>(value);
    (*bytes)[1] = static_cast<u8>(value >> 8u);
    (*bytes)[2] = static_cast<u8>(value >> 16u);
    (*bytes)[3] = static_cast<u8>(value >> 24u);
    return Ok();
}

struct InstanceState {
    wasmtime::Store store;
    host::HostApi host;
    std::optional<wasmtime::Instance> instance;
    std::optional<wasmtime::TypedFunc<std::monostate, uint32_t>> ext_api_version;
    std::optional<wasmtime::TypedFunc<std::monostate, int32_t>> ext_init;
    std::optional<wasmtime::TypedFunc<double, std::monostate>> ext_on_tick;
    std::optional<wasmtime::TypedFunc<std::tuple<uint32_t, uint32_t, uint32_t>, std::monostate>> ext_on_event;
    std::optional<wasmtime::TypedFunc<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, std::monostate>> ext_on_event_named;
    std::optional<wasmtime::TypedFunc<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, int32_t>> ext_on_command;
    std::optional<wasmtime::TypedFunc<std::monostate, std::monostate>> ext_on_unload;
    std::optional<wasmtime::TypedFunc<uint32_t, uint32_t>> ext_alloc;
    std::optional<wasmtime::TypedFunc<std::tuple<uint32_t, uint32_t>, std::monostate>> ext_free;

    InstanceState(wasmtime::Engine& engine, host::HostApi host_api)
        : store(engine),
          host(std::move(host_api)) {
        store.limiter(kMaxMemoryBytes, kMaxTableElements, kMaxInstances, kMaxTables, kMaxMemories);
    }
};

[[nodiscard]] Result<void> RefillFuel(InstanceState& state) {
    auto fueled = FromWasmtimeResult(state.store.context().set_fuel(kCallFuel), "set wasm fuel");
    if (!fueled) {
        return Err(fueled.error());
    }
    return Ok();
}

[[nodiscard]] Result<wasmtime::Memory> InstanceMemory(InstanceState& state) {
    if (!state.instance) {
        return Err(ErrorCode::InvalidState, "Wasm instance is not initialized.");
    }

    auto exported = state.instance->get(state.store, "memory");
    if (!exported) {
        return Err(ErrorCode::FileNotFound, "Extension wasm module does not export memory.");
    }

    const auto* memory = std::get_if<wasmtime::Memory>(&*exported);
    if (memory == nullptr) {
        return Err(ErrorCode::ValidationInvalidState, "Extension export 'memory' is not memory.");
    }
    return Ok(*memory);
}

[[nodiscard]] Result<std::span<u8>> InstanceBytesOut(InstanceState& state, uint32_t offset, uint32_t len) {
    auto memory = InstanceMemory(state);
    if (!memory) {
        return Err(memory.error());
    }

    const wasmtime::Span<uint8_t> data = memory->data(state.store);
    const auto start = static_cast<std::size_t>(offset);
    const auto size = static_cast<std::size_t>(len);
    if (start > data.size() || size > data.size() - start) {
        return Err(ErrorCode::ValidationOutOfRange, "Guest event payload buffer is out of bounds.");
    }
    if (size == 0 && data.data() == nullptr)
        return Ok(std::span<u8>{});
    return Ok(std::span<u8>(data.data() + start, size));
}

[[nodiscard]] Result<uint32_t> AllocateGuestBytes(InstanceState& state, std::span<const u8> bytes) {
    if (bytes.empty()) {
        return Ok(0u);
    }
    if (!state.ext_alloc) {
        return Err(ErrorCode::InvalidState, "Extension cannot receive host payloads because it does not export ext_alloc.");
    }

    auto allocated = FromTrapResult(state.ext_alloc->call(state.store, static_cast<uint32_t>(bytes.size())), "ext_alloc trap");
    if (!allocated) {
        return Err(allocated.error());
    }
    const uint32_t guest_ptr = *allocated;
    if (guest_ptr == 0) {
        return Err(ErrorCode::ValidationInvalidState, "Extension ext_alloc returned null.");
    }

    auto guest = InstanceBytesOut(state, guest_ptr, static_cast<uint32_t>(bytes.size()));
    if (!guest) {
        if (state.ext_free) {
            auto freed = FromTrapResult(state.ext_free->call(state.store, std::make_tuple(guest_ptr, static_cast<uint32_t>(bytes.size()))), "ext_free trap");
            if (!freed)
                return Err(freed.error());
        }
        return Err(guest.error());
    }
    std::ranges::copy(bytes, guest->begin());
    return Ok(guest_ptr);
}

[[nodiscard]] Result<void> FreeGuestBytes(InstanceState& state, uint32_t ptr, uint32_t len) {
    if (ptr == 0 || !state.ext_free) {
        return Ok();
    }
    auto freed = FromTrapResult(state.ext_free->call(state.store, std::make_tuple(ptr, len)), "ext_free trap");
    return freed ? Ok() : Err(freed.error());
}

template <typename Params, typename Results>
[[nodiscard]] Result<wasmtime::TypedFunc<Params, Results>> TypedExport(InstanceState& state, std::string_view name) {
    if (!state.instance) {
        return Err(ErrorCode::InvalidState, "Wasm instance is not initialized.");
    }

    auto exported = state.instance->get(state.store, name);
    if (!exported) {
        return Err(ErrorCode::FileNotFound, "Extension wasm module is missing export '" + std::string(name) + "'.");
    }

    const auto* func = std::get_if<wasmtime::Func>(&*exported);
    if (func == nullptr) {
        return Err(ErrorCode::ValidationInvalidState, "Extension export '" + std::string(name) + "' is not a function.");
    }

    auto typed = func->typed<Params, Results>(state.store);
    if (!typed) {
        return Err(ErrorCode::ValidationInvalidState, "Extension export signature mismatch for '" + std::string(name) + "': " + typed.err().message());
    }
    return Ok(typed.ok());
}

[[nodiscard]] Result<void> DefineHostImports(wasmtime::Linker& linker, const Manifest& manifest, host::HostApi& host) {
    host::HostApi* host_ptr = &host;

    if (HasPermission(manifest, Permission::Log)) {
        if (auto defined = linker.func_wrap(kImportModule, "host_log",
                [host_ptr](wasmtime::Caller caller, int32_t level, int32_t ptr, int32_t len) -> int32_t {
                    auto bytes = GuestBytes(caller, ptr, len);
                    if (!bytes) {
                        return host::cabi::kInvalid;
                    }
                    return host::cabi::Log(*host_ptr, static_cast<u32>(level), bytes->data(), static_cast<u32>(bytes->size()));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_log: " + defined.err().message());
        }
    }

    if (HasPermission(manifest, Permission::Paths)) {
        if (auto defined = linker.func_wrap(kImportModule, "host_path_data",
                [host_ptr](wasmtime::Caller caller, int32_t out_ptr, int32_t out_cap) -> int32_t {
                    if (out_cap < 0) {
                        return host::cabi::kInvalid;
                    }
                    auto out = GuestCStringOut(caller, out_ptr, static_cast<u32>(out_cap));
                    if (!out) {
                        return host::cabi::kInvalid;
                    }
                    return host::cabi::PathData(*host_ptr, *out, static_cast<u32>(out_cap));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_path_data: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_path_cache",
                [host_ptr](wasmtime::Caller caller, int32_t out_ptr, int32_t out_cap) -> int32_t {
                    if (out_cap < 0) {
                        return host::cabi::kInvalid;
                    }
                    auto out = GuestCStringOut(caller, out_ptr, static_cast<u32>(out_cap));
                    if (!out) {
                        return host::cabi::kInvalid;
                    }
                    return host::cabi::PathCache(*host_ptr, *out, static_cast<u32>(out_cap));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_path_cache: " + defined.err().message());
        }
    }

    if (HasPermission(manifest, Permission::Storage)) {
        if (auto defined = linker.func_wrap(kImportModule, "host_file_read",
                [host_ptr](wasmtime::Caller caller, int32_t path_ptr, int32_t out_ptr, int32_t inout_len_ptr) -> int32_t {
                    auto path = GuestCString(caller, path_ptr);
                    if (!path) {
                        return host::cabi::kInvalid;
                    }
                    auto capacity = GuestU32(caller, inout_len_ptr);
                    if (!capacity) {
                        return host::cabi::kInvalid;
                    }
                    auto out = GuestBytesOut(caller, out_ptr, *capacity);
                    if (!out) {
                        return host::cabi::kInvalid;
                    }

                    u32 inout_len = *capacity;
                    const i32 status = host::cabi::FileRead(*host_ptr, path->data(), static_cast<u32>(path->size()), out->data(), &inout_len);
                    auto wrote_len = WriteGuestU32(caller, inout_len_ptr, inout_len);
                    if (!wrote_len) {
                        return host::cabi::kInvalid;
                    }
                    return status;
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_file_read: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_file_read_n",
                [host_ptr](wasmtime::Caller caller, int32_t path_ptr, int32_t path_len, int32_t out_ptr, int32_t inout_len_ptr) -> int32_t {
                    auto path = GuestBytes(caller, path_ptr, path_len);
                    if (!path) {
                        return host::cabi::kInvalid;
                    }
                    auto capacity = GuestU32(caller, inout_len_ptr);
                    if (!capacity) {
                        return host::cabi::kInvalid;
                    }
                    auto out = GuestBytesOut(caller, out_ptr, *capacity);
                    if (!out) {
                        return host::cabi::kInvalid;
                    }

                    u32 inout_len = *capacity;
                    const i32 status = host::cabi::FileRead(*host_ptr, path->data(), static_cast<u32>(path->size()), out->data(), &inout_len);
                    auto wrote_len = WriteGuestU32(caller, inout_len_ptr, inout_len);
                    if (!wrote_len) {
                        return host::cabi::kInvalid;
                    }
                    return status;
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_file_read_n: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_file_write_n",
                [host_ptr](wasmtime::Caller caller, int32_t path_ptr, int32_t path_len, int32_t data_ptr, int32_t data_len) -> int32_t {
                    auto path = GuestBytes(caller, path_ptr, path_len);
                    if (!path) {
                        return host::cabi::kInvalid;
                    }
                    auto bytes = GuestBytes(caller, data_ptr, data_len);
                    if (!bytes) {
                        return host::cabi::kInvalid;
                    }
                    return host::cabi::FileWrite(*host_ptr, path->data(), static_cast<u32>(path->size()), reinterpret_cast<const u8*>(bytes->data()), static_cast<u32>(bytes->size()));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_file_write_n: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_file_append_n",
                [host_ptr](wasmtime::Caller caller, int32_t path_ptr, int32_t path_len, int32_t data_ptr, int32_t data_len) -> int32_t {
                    auto path = GuestBytes(caller, path_ptr, path_len);
                    if (!path) {
                        return host::cabi::kInvalid;
                    }
                    auto bytes = GuestBytes(caller, data_ptr, data_len);
                    if (!bytes) {
                        return host::cabi::kInvalid;
                    }
                    return host::cabi::FileAppend(*host_ptr, path->data(), static_cast<u32>(path->size()), reinterpret_cast<const u8*>(bytes->data()), static_cast<u32>(bytes->size()));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_file_append_n: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_file_write",
                [host_ptr](wasmtime::Caller caller, int32_t path_ptr, int32_t data_ptr, int32_t data_len) -> int32_t {
                    auto path = GuestCString(caller, path_ptr);
                    if (!path) {
                        return host::cabi::kInvalid;
                    }
                    auto bytes = GuestBytes(caller, data_ptr, data_len);
                    if (!bytes) {
                        return host::cabi::kInvalid;
                    }
                    return host::cabi::FileWrite(*host_ptr, path->data(), static_cast<u32>(path->size()), reinterpret_cast<const u8*>(bytes->data()), static_cast<u32>(bytes->size()));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_file_write: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_file_append",
                [host_ptr](wasmtime::Caller caller, int32_t path_ptr, int32_t data_ptr, int32_t data_len) -> int32_t {
                    auto path = GuestCString(caller, path_ptr);
                    if (!path) {
                        return host::cabi::kInvalid;
                    }
                    auto bytes = GuestBytes(caller, data_ptr, data_len);
                    if (!bytes) {
                        return host::cabi::kInvalid;
                    }
                    return host::cabi::FileAppend(*host_ptr, path->data(), static_cast<u32>(path->size()), reinterpret_cast<const u8*>(bytes->data()), static_cast<u32>(bytes->size()));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_file_append: " + defined.err().message());
        }
    }

    if (HasPermission(manifest, Permission::Config)) {
        if (auto defined = linker.func_wrap(kImportModule, "host_config_get",
                [host_ptr](wasmtime::Caller caller, int32_t key_ptr, int32_t out_ptr, int32_t out_cap) -> int32_t {
                    if (out_cap < 0) {
                        return host::cabi::kInvalid;
                    }
                    auto key = GuestCString(caller, key_ptr);
                    if (!key) {
                        return host::cabi::kInvalid;
                    }
                    auto out = GuestCStringOut(caller, out_ptr, static_cast<u32>(out_cap));
                    if (!out) {
                        return host::cabi::kInvalid;
                    }
                    std::string key_text(*key);
                    return host::cabi::ConfigGet(*host_ptr, key_text.c_str(), *out, static_cast<u32>(out_cap));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_config_get: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_config_set",
                [host_ptr](wasmtime::Caller caller, int32_t key_ptr, int32_t value_ptr, int32_t value_len) -> int32_t {
                    auto key = GuestCString(caller, key_ptr);
                    if (!key) {
                        return host::cabi::kInvalid;
                    }
                    auto value = GuestBytes(caller, value_ptr, value_len);
                    if (!value) {
                        return host::cabi::kInvalid;
                    }
                    std::string key_text(*key);
                    return host::cabi::ConfigSet(*host_ptr, key_text.c_str(), value->data(), static_cast<u32>(value->size()));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_config_set: " + defined.err().message());
        }
    }

    {
        if (auto defined = linker.func_wrap(kImportModule, "host_event_subscribe", [host_ptr](int32_t event_type) -> int32_t { return host::cabi::EventSubscribe(*host_ptr, static_cast<u32>(event_type)); }); !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_event_subscribe: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_event_emit",
                [host_ptr](wasmtime::Caller caller, int32_t event_type, int32_t payload_ptr, int32_t payload_len) -> int32_t {
                    if (payload_len < 0) {
                        return host::cabi::kInvalid;
                    }
                    if (static_cast<u32>(payload_len) > limits::kMaxEventBytes) {
                        return host::cabi::kNoSpace;
                    }
                    auto payload = GuestBytes(caller, payload_ptr, payload_len);
                    if (!payload) {
                        return host::cabi::kInvalid;
                    }
                    return host::cabi::EventEmit(*host_ptr, static_cast<u32>(event_type), reinterpret_cast<const u8*>(payload->data()), static_cast<u32>(payload->size()));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_event_emit: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_event_subscribe_named",
                [host_ptr](wasmtime::Caller caller, int32_t name_ptr, int32_t name_len) -> int32_t {
                    if (name_len < 0)
                        return host::cabi::kInvalid;
                    auto name = GuestBytes(caller, name_ptr, name_len);
                    if (!name)
                        return host::cabi::kInvalid;
                    return host::cabi::EventSubscribeNamed(*host_ptr, name->data(), static_cast<u32>(name->size()));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_event_subscribe_named: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_event_emit_named",
                [host_ptr](wasmtime::Caller caller, int32_t name_ptr, int32_t name_len, int32_t payload_ptr, int32_t payload_len) -> int32_t {
                    if (name_len < 0 || payload_len < 0)
                        return host::cabi::kInvalid;
                    auto name = GuestBytes(caller, name_ptr, name_len);
                    auto payload = GuestBytes(caller, payload_ptr, payload_len);
                    if (!name || !payload)
                        return host::cabi::kInvalid;
                    return host::cabi::EventEmitNamed(*host_ptr, name->data(), static_cast<u32>(name->size()), reinterpret_cast<const u8*>(payload->data()), static_cast<u32>(payload->size()));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_event_emit_named: " + defined.err().message());
        }
    }

    return Ok();
}

} // namespace

struct WasmtimeEngine::Impl {
    wasmtime::Engine engine;

    Impl()
        : engine([] {
              wasmtime::Config config;
              config.consume_fuel(true);
              config.memory_reservation(kMemoryReservationBytes);
              config.max_wasm_stack(kMaxWasmStackBytes);
              return wasmtime::Engine(std::move(config));
          }()) {}
};

WasmtimeEngine::WasmtimeEngine()
    : impl_(std::make_unique<Impl>()) {}

WasmtimeEngine::~WasmtimeEngine() = default;

namespace {

class WasmtimeInstance final : public RuntimeInstance {
public:
    WasmtimeInstance(std::string id, std::unique_ptr<InstanceState> state)
        : id_(std::move(id)),
          state_(std::move(state)) {}

    ~WasmtimeInstance() override {
        Unload();
    }

    Result<void> Initialize() override {
        if (auto fueled = RefillFuel(*state_); !fueled)
            return Err(fueled.error());
        auto result = FromTrapResult(state_->ext_init->call(state_->store, std::monostate{}), "ext_init trap");
        if (!result)
            return Err(result.error());
        if (*result != 0)
            return Err(ErrorCode::ValidationInvalidState, "Extension ext_init returned failure code " + std::to_string(*result) + ".");
        return Ok();
    }

    Result<void> Tick(f64 delta_ms) override {
        if (auto fueled = RefillFuel(*state_); !fueled)
            return Err(fueled.error());
        auto result = FromTrapResult(state_->ext_on_tick->call(state_->store, delta_ms), "ext_on_tick trap");
        return result ? Ok() : Err(result.error());
    }

    Result<void> DispatchEvent(u32 event_type, std::span<const u8> payload) override {
        if (payload.size() > limits::kMaxEventBytes)
            return Err(ErrorCode::ValidationOutOfRange, "Extension event payload exceeds the ABI size limit.");
        if (auto fueled = RefillFuel(*state_); !fueled)
            return Err(fueled.error());
        const uint32_t len = static_cast<uint32_t>(payload.size());
        auto ptr = AllocateGuestBytes(*state_, payload);
        if (!ptr)
            return Err(ptr.error());
        auto result = FromTrapResult(state_->ext_on_event->call(state_->store, std::make_tuple(event_type, *ptr, len)), "ext_on_event trap");
        auto freed = FreeGuestBytes(*state_, *ptr, len);
        if (!result)
            return Err(result.error());
        if (!freed)
            return Err(freed.error());
        return Ok();
    }

    Result<void> DispatchNamedEvent(std::string_view topic, std::span<const u8> payload) override {
        if (!state_->ext_on_event_named)
            return Ok();
        if (topic.size() > limits::kMaxEventTopicBytes || payload.size() > limits::kMaxEventBytes)
            return Err(ErrorCode::ValidationOutOfRange, "Named extension event exceeds the ABI size limit.");
        if (auto fueled = RefillFuel(*state_); !fueled)
            return Err(fueled.error());
        auto name_ptr = AllocateGuestBytes(*state_, {reinterpret_cast<const u8*>(topic.data()), topic.size()});
        if (!name_ptr)
            return Err(name_ptr.error());
        auto payload_ptr = AllocateGuestBytes(*state_, payload);
        if (!payload_ptr) {
            (void)FreeGuestBytes(*state_, *name_ptr, static_cast<u32>(topic.size()));
            return Err(payload_ptr.error());
        }
        auto result = FromTrapResult(state_->ext_on_event_named->call(state_->store, std::make_tuple(*name_ptr, static_cast<u32>(topic.size()), *payload_ptr, static_cast<u32>(payload.size()))),
            "ext_on_event_named trap");
        auto payload_freed = FreeGuestBytes(*state_, *payload_ptr, static_cast<u32>(payload.size()));
        auto name_freed = FreeGuestBytes(*state_, *name_ptr, static_cast<u32>(topic.size()));
        if (!result)
            return Err(result.error());
        if (!payload_freed)
            return Err(payload_freed.error());
        if (!name_freed)
            return Err(name_freed.error());
        return Ok();
    }

    Result<void> DispatchCommand(std::string_view command_id, std::span<const u8> payload) override {
        if (!state_->ext_on_command)
            return Err(ErrorCode::InvalidState, "Extension '" + id_ + "' declares commands but does not export ext_on_command.");
        if (command_id.empty() || command_id.size() > limits::kMaxPathBytes || payload.size() > limits::kMaxEventBytes) {
            return Err(ErrorCode::ValidationOutOfRange, "Extension command id or payload exceeds the ABI size limit.");
        }
        if (auto fueled = RefillFuel(*state_); !fueled)
            return Err(fueled.error());
        auto command_ptr = AllocateGuestBytes(*state_, {reinterpret_cast<const u8*>(command_id.data()), command_id.size()});
        if (!command_ptr)
            return Err(command_ptr.error());
        const uint32_t command_len = static_cast<uint32_t>(command_id.size());
        const uint32_t payload_len = static_cast<uint32_t>(payload.size());
        auto payload_ptr = AllocateGuestBytes(*state_, payload);
        if (!payload_ptr) {
            (void)FreeGuestBytes(*state_, *command_ptr, command_len);
            return Err(payload_ptr.error());
        }
        auto result = FromTrapResult(state_->ext_on_command->call(state_->store, std::make_tuple(*command_ptr, command_len, *payload_ptr, payload_len)), "ext_on_command trap");
        auto payload_freed = FreeGuestBytes(*state_, *payload_ptr, payload_len);
        auto command_freed = FreeGuestBytes(*state_, *command_ptr, command_len);
        if (!result)
            return Err(result.error());
        if (*result != WOKI_EXT_OK)
            return Err(GuestCommandError(*result));
        if (!payload_freed)
            return Err(payload_freed.error());
        if (!command_freed)
            return Err(command_freed.error());
        return Ok();
    }

    void Unload() noexcept override {
        if (state_ == nullptr)
            return;
        (void)RefillFuel(*state_);
        (void)FromTrapResult(state_->ext_on_unload->call(state_->store, std::monostate{}), "ext_on_unload trap");
        state_.reset();
    }

private:
    std::string id_;
    std::unique_ptr<InstanceState> state_;
};

} // namespace

Result<scope<RuntimeInstance>> WasmtimeEngine::Create(const ExtensionPackage& package, host::HostApi host) {
    auto bytes = LoadGuestModule(package.Layout().wasm);
    if (!bytes) {
        return Err(bytes.error());
    }
    auto valid = ValidateGuestModule(*bytes, package.GetManifest());
    if (!valid) {
        return Err(valid.error());
    }

    auto module = FromWasmtimeResult(wasmtime::Module::compile(impl_->engine, wasmtime::Span<uint8_t>{bytes->data(), bytes->size()}), "Failed to compile extension wasm module");
    if (!module) {
        return Err(module.error());
    }

    auto state = std::make_unique<InstanceState>(impl_->engine, std::move(host));
    if (auto fueled = RefillFuel(*state); !fueled) {
        return Err(fueled.error());
    }
    wasmtime::Linker linker(impl_->engine);

    auto imports = DefineHostImports(linker, package.GetManifest(), state->host);
    if (!imports) {
        return Err(imports.error());
    }

    auto instance = FromTrapResult(linker.instantiate(state->store, *module), "Failed to instantiate extension module");
    if (!instance) {
        return Err(instance.error());
    }

    state->instance = *instance;

    auto api_version = TypedExport<std::monostate, uint32_t>(*state, "ext_api_version");
    if (!api_version) {
        return Err(api_version.error());
    }
    auto init = TypedExport<std::monostate, int32_t>(*state, "ext_init");
    if (!init) {
        return Err(init.error());
    }
    auto tick = TypedExport<double, std::monostate>(*state, "ext_on_tick");
    if (!tick) {
        return Err(tick.error());
    }
    auto event = TypedExport<std::tuple<uint32_t, uint32_t, uint32_t>, std::monostate>(*state, "ext_on_event");
    if (!event) {
        return Err(event.error());
    }
    auto unload = TypedExport<std::monostate, std::monostate>(*state, "ext_on_unload");
    if (!unload) {
        return Err(unload.error());
    }

    if (state->instance->get(state->store, "ext_alloc")) {
        auto alloc = TypedExport<uint32_t, uint32_t>(*state, "ext_alloc");
        if (!alloc) {
            return Err(alloc.error());
        }
        state->ext_alloc = *alloc;
    }
    if (state->instance->get(state->store, "ext_free")) {
        auto free = TypedExport<std::tuple<uint32_t, uint32_t>, std::monostate>(*state, "ext_free");
        if (!free) {
            return Err(free.error());
        }
        state->ext_free = *free;
    }
    if (state->instance->get(state->store, "ext_on_command")) {
        auto command = TypedExport<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, int32_t>(*state, "ext_on_command");
        if (!command) {
            return Err(command.error());
        }
        state->ext_on_command = *command;
    }
    if (state->instance->get(state->store, "ext_on_event_named")) {
        auto named_event = TypedExport<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, std::monostate>(*state, "ext_on_event_named");
        if (!named_event)
            return Err(named_event.error());
        state->ext_on_event_named = *named_event;
    }

    state->ext_api_version = *api_version;
    state->ext_init = *init;
    state->ext_on_tick = *tick;
    state->ext_on_event = *event;
    state->ext_on_unload = *unload;

    if (auto fueled = RefillFuel(*state); !fueled) {
        return Err(fueled.error());
    }
    auto version = FromTrapResult(state->ext_api_version->call(state->store, std::monostate{}), "ext_api_version trap");
    if (!version) {
        return Err(version.error());
    }
    if (static_cast<u32>(*version) != package.GetManifest().api_version) {
        return Err(ErrorCode::ValidationInvalidState, "Extension apiVersion mismatch. Manifest declares " + std::to_string(package.GetManifest().api_version) + ", wasm exports " + std::to_string(*version) + ".");
    }
    return Ok(scope<RuntimeInstance>(createScope<WasmtimeInstance>(package.Id(), std::move(state))));
}

} // namespace woki::ext::wasm
