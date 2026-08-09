#include <tuple>
#include <vector>
#include <cstring>
#include <fstream>
#include <variant>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <wasmtime.hh>
#include <unordered_map>

#include "woki/ext/host/cabi.hpp"
#include "woki/ext/wasm/wasmtime_engine.hpp"

namespace woki::ext::wasm {

namespace {

namespace fs = std::filesystem;

constexpr std::string_view kImportModule = "woki_host";
inline constexpr std::size_t kMaxGuestCStringBytes = 4096u;
inline constexpr u64 kCallFuel = 10'000'000u;
inline constexpr u64 kMemoryReservationBytes = 32u * 1024u * 1024u;
inline constexpr u64 kMaxWasmStackBytes = 1u * 1024u * 1024u;

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

[[nodiscard]] Result<std::vector<uint8_t>> ReadWasmBytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.good()) {
        return Err(ErrorCode::FileReadError, "Failed to open wasm module: " + path.string());
    }

    const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (bytes.empty()) {
        return Err(ErrorCode::ParseInvalidFormat, "Wasm module is empty: " + path.string());
    }
    return Ok(bytes);
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
    if (start + size > data.size()) {
        return Err(ErrorCode::ValidationOutOfRange, "Guest memory read is out of bounds.");
    }

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
    if (start + cap > data.size()) {
        return Err(ErrorCode::ValidationOutOfRange, "Guest memory write is out of bounds.");
    }

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
    if (start + size > data.size()) {
        return Err(ErrorCode::ValidationOutOfRange, "Guest output buffer is out of bounds.");
    }

    return Ok(std::span<u8>(data.data() + start, size));
}

[[nodiscard]] Result<u32> GuestU32(wasmtime::Caller& caller, int32_t offset) {
    auto bytes = GuestBytesOut(caller, offset, sizeof(u32));
    if (!bytes) {
        return Err(bytes.error());
    }

    u32 value = 0;
    std::memcpy(&value, bytes->data(), sizeof(value));
    return Ok(value);
}

[[nodiscard]] Result<void> WriteGuestU32(wasmtime::Caller& caller, int32_t offset, u32 value) {
    auto bytes = GuestBytesOut(caller, offset, sizeof(u32));
    if (!bytes) {
        return Err(bytes.error());
    }

    std::memcpy(bytes->data(), &value, sizeof(value));
    return Ok();
}

struct InstanceState {
    wasmtime::Store store;
    std::optional<wasmtime::Instance> instance;
    std::optional<wasmtime::TypedFunc<std::monostate, uint32_t>> ext_api_version;
    std::optional<wasmtime::TypedFunc<std::monostate, int32_t>> ext_init;
    std::optional<wasmtime::TypedFunc<double, std::monostate>> ext_on_tick;
    std::optional<wasmtime::TypedFunc<std::tuple<uint32_t, uint32_t, uint32_t>, std::monostate>> ext_on_event;
    std::optional<wasmtime::TypedFunc<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, int32_t>> ext_on_command;
    std::optional<wasmtime::TypedFunc<std::monostate, std::monostate>> ext_on_unload;
    std::optional<wasmtime::TypedFunc<uint32_t, uint32_t>> ext_alloc;
    std::optional<wasmtime::TypedFunc<std::tuple<uint32_t, uint32_t>, std::monostate>> ext_free;

    explicit InstanceState(wasmtime::Engine& engine)
        : store(engine) {}
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
    if (start + size > data.size()) {
        return Err(ErrorCode::ValidationOutOfRange, "Guest event payload buffer is out of bounds.");
    }
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
            (void)state.ext_free->call(state.store, std::make_tuple(guest_ptr, static_cast<uint32_t>(bytes.size())));
        }
        return Err(guest.error());
    }
    std::ranges::copy(bytes, guest->begin());
    return Ok(guest_ptr);
}

void FreeGuestBytes(InstanceState& state, uint32_t ptr, uint32_t len) {
    if (ptr == 0 || !state.ext_free) {
        return;
    }
    (void)state.ext_free->call(state.store, std::make_tuple(ptr, len));
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

[[nodiscard]] Result<void> DefineHostImports(wasmtime::Linker& linker, Record& record) {
    Record* record_ptr = &record;

    if (HasPermission(record.manifest, Permission::Log)) {
        if (auto defined = linker.func_wrap(kImportModule, "host_log",
                [record_ptr](wasmtime::Caller caller, int32_t level, int32_t ptr, int32_t len) -> int32_t {
                    auto bytes = GuestBytes(caller, ptr, len);
                    if (!bytes) {
                        return host::cabi::kInvalid;
                    }
                    return host::cabi::Log(*record_ptr, static_cast<u32>(level), bytes->data(), static_cast<u32>(bytes->size()));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_log: " + defined.err().message());
        }
    }

    if (HasPermission(record.manifest, Permission::Paths)) {
        if (auto defined = linker.func_wrap(kImportModule, "host_path_data",
                [record_ptr](wasmtime::Caller caller, int32_t out_ptr, int32_t out_cap) -> int32_t {
                    auto out = GuestCStringOut(caller, out_ptr, static_cast<u32>(out_cap));
                    if (!out) {
                        return host::cabi::kInvalid;
                    }
                    return host::cabi::PathData(*record_ptr, *out, static_cast<u32>(out_cap));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_path_data: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_path_cache",
                [record_ptr](wasmtime::Caller caller, int32_t out_ptr, int32_t out_cap) -> int32_t {
                    auto out = GuestCStringOut(caller, out_ptr, static_cast<u32>(out_cap));
                    if (!out) {
                        return host::cabi::kInvalid;
                    }
                    return host::cabi::PathCache(*record_ptr, *out, static_cast<u32>(out_cap));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_path_cache: " + defined.err().message());
        }
    }

    if (HasPermission(record.manifest, Permission::Storage)) {
        if (auto defined = linker.func_wrap(kImportModule, "host_file_read",
                [record_ptr](wasmtime::Caller caller, int32_t path_ptr, int32_t out_ptr, int32_t inout_len_ptr) -> int32_t {
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
                    const i32 status = host::cabi::FileRead(*record_ptr, path->data(), static_cast<u32>(path->size()), out->data(), &inout_len);
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
                [record_ptr](wasmtime::Caller caller, int32_t path_ptr, int32_t path_len, int32_t out_ptr, int32_t inout_len_ptr) -> int32_t {
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
                    const i32 status = host::cabi::FileRead(*record_ptr, path->data(), static_cast<u32>(path->size()), out->data(), &inout_len);
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
                [record_ptr](wasmtime::Caller caller, int32_t path_ptr, int32_t path_len, int32_t data_ptr, int32_t data_len) -> int32_t {
                    auto path = GuestBytes(caller, path_ptr, path_len);
                    if (!path) {
                        return host::cabi::kInvalid;
                    }
                    auto bytes = GuestBytes(caller, data_ptr, data_len);
                    if (!bytes) {
                        return host::cabi::kInvalid;
                    }
                    return host::cabi::FileWrite(*record_ptr, path->data(), static_cast<u32>(path->size()), reinterpret_cast<const u8*>(bytes->data()), static_cast<u32>(bytes->size()));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_file_write_n: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_file_append_n",
                [record_ptr](wasmtime::Caller caller, int32_t path_ptr, int32_t path_len, int32_t data_ptr, int32_t data_len) -> int32_t {
                    auto path = GuestBytes(caller, path_ptr, path_len);
                    if (!path) {
                        return host::cabi::kInvalid;
                    }
                    auto bytes = GuestBytes(caller, data_ptr, data_len);
                    if (!bytes) {
                        return host::cabi::kInvalid;
                    }
                    return host::cabi::FileAppend(*record_ptr, path->data(), static_cast<u32>(path->size()), reinterpret_cast<const u8*>(bytes->data()), static_cast<u32>(bytes->size()));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_file_append_n: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_file_write",
                [record_ptr](wasmtime::Caller caller, int32_t path_ptr, int32_t data_ptr, int32_t data_len) -> int32_t {
                    auto path = GuestCString(caller, path_ptr);
                    if (!path) {
                        return host::cabi::kInvalid;
                    }
                    auto bytes = GuestBytes(caller, data_ptr, data_len);
                    if (!bytes) {
                        return host::cabi::kInvalid;
                    }
                    return host::cabi::FileWrite(*record_ptr, path->data(), static_cast<u32>(path->size()), reinterpret_cast<const u8*>(bytes->data()), static_cast<u32>(bytes->size()));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_file_write: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_file_append",
                [record_ptr](wasmtime::Caller caller, int32_t path_ptr, int32_t data_ptr, int32_t data_len) -> int32_t {
                    auto path = GuestCString(caller, path_ptr);
                    if (!path) {
                        return host::cabi::kInvalid;
                    }
                    auto bytes = GuestBytes(caller, data_ptr, data_len);
                    if (!bytes) {
                        return host::cabi::kInvalid;
                    }
                    return host::cabi::FileAppend(*record_ptr, path->data(), static_cast<u32>(path->size()), reinterpret_cast<const u8*>(bytes->data()), static_cast<u32>(bytes->size()));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_file_append: " + defined.err().message());
        }
    }

    if (HasPermission(record.manifest, Permission::Config)) {
        if (auto defined = linker.func_wrap(kImportModule, "host_config_get",
                [record_ptr](wasmtime::Caller caller, int32_t key_ptr, int32_t out_ptr, int32_t out_cap) -> int32_t {
                    auto key = GuestCString(caller, key_ptr);
                    if (!key) {
                        return host::cabi::kInvalid;
                    }
                    auto out = GuestCStringOut(caller, out_ptr, static_cast<u32>(out_cap));
                    if (!out) {
                        return host::cabi::kInvalid;
                    }
                    std::string key_text(*key);
                    return host::cabi::ConfigGet(*record_ptr, key_text.c_str(), *out, static_cast<u32>(out_cap));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_config_get: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_config_set",
                [record_ptr](wasmtime::Caller caller, int32_t key_ptr, int32_t value_ptr, int32_t value_len) -> int32_t {
                    auto key = GuestCString(caller, key_ptr);
                    if (!key) {
                        return host::cabi::kInvalid;
                    }
                    auto value = GuestBytes(caller, value_ptr, value_len);
                    if (!value) {
                        return host::cabi::kInvalid;
                    }
                    std::string key_text(*key);
                    return host::cabi::ConfigSet(*record_ptr, key_text.c_str(), value->data(), static_cast<u32>(value->size()));
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_config_set: " + defined.err().message());
        }
    }

    if (HasPermission(record.manifest, Permission::Events)) {
        if (auto defined = linker.func_wrap(kImportModule, "host_event_subscribe", [](int32_t /*event_type*/) -> int32_t { return host::cabi::kOk; }); !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_event_subscribe: " + defined.err().message());
        }

        if (auto defined = linker.func_wrap(kImportModule, "host_event_emit",
                [record_ptr](wasmtime::Caller caller, int32_t event_type, int32_t payload_ptr, int32_t payload_len) -> int32_t {
                    auto payload = GuestBytes(caller, payload_ptr, payload_len);
                    if (!payload) {
                        return host::cabi::kInvalid;
                    }
                    host::HostApi host(*record_ptr);
                    host.Log(host::LogLevel::Debug, "extension emitted event type " + std::to_string(event_type) + " payload bytes " + std::to_string(payload->size()));
                    return host::cabi::kOk;
                });
            !defined) {
            return Err(ErrorCode::InvalidState, "Failed to define host import woki_host::host_event_emit: " + defined.err().message());
        }
    }

    return Ok();
}

} // namespace

struct WasmtimeEngine::Impl {
    wasmtime::Engine engine;
    std::unordered_map<std::string, std::unique_ptr<InstanceState>> instances;

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

Result<void> WasmtimeEngine::Load(Record& record, host::HostApi& /*host*/) {
    if (impl_->instances.contains(record.id)) {
        return Err(ErrorCode::ValidationInvalidState, "Extension '" + record.id + "' is already loaded in the Wasmtime engine.");
    }

    auto bytes = ReadWasmBytes(record.package.wasm);
    if (!bytes) {
        return Err(bytes.error());
    }

    auto module = FromWasmtimeResult(wasmtime::Module::compile(impl_->engine, wasmtime::Span<uint8_t>{bytes->data(), bytes->size()}), "Failed to compile extension wasm module");
    if (!module) {
        return Err(module.error());
    }

    auto state = std::make_unique<InstanceState>(impl_->engine);
    if (auto fueled = RefillFuel(*state); !fueled) {
        return Err(fueled.error());
    }
    wasmtime::Linker linker(impl_->engine);

    auto imports = DefineHostImports(linker, record);
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

    state->ext_api_version = *api_version;
    state->ext_init = *init;
    state->ext_on_tick = *tick;
    state->ext_on_event = *event;
    state->ext_on_unload = *unload;

    impl_->instances.emplace(record.id, std::move(state));
    return Ok();
}

Result<u32> WasmtimeEngine::ApiVersion(Record& record) {
    auto* state = impl_->instances.at(record.id).get();
    if (auto fueled = RefillFuel(*state); !fueled) {
        return Err(fueled.error());
    }
    auto version = FromTrapResult(state->ext_api_version->call(state->store, std::monostate{}), "ext_api_version trap");
    if (!version) {
        return Err(version.error());
    }
    return Ok(static_cast<u32>(*version));
}

Result<i32> WasmtimeEngine::Init(Record& record) {
    auto* state = impl_->instances.at(record.id).get();
    if (auto fueled = RefillFuel(*state); !fueled) {
        return Err(fueled.error());
    }
    return FromTrapResult(state->ext_init->call(state->store, std::monostate{}), "ext_init trap");
}

Result<void> WasmtimeEngine::Tick(Record& record, f64 delta_ms) {
    auto* state = impl_->instances.at(record.id).get();
    if (auto fueled = RefillFuel(*state); !fueled) {
        return Err(fueled.error());
    }
    auto ticked = FromTrapResult(state->ext_on_tick->call(state->store, delta_ms), "ext_on_tick trap");
    if (!ticked) {
        return Err(ticked.error());
    }
    return Ok();
}

Result<void> WasmtimeEngine::Event(Record& record, u32 event_type, std::span<const u8> payload) {
    auto* state = impl_->instances.at(record.id).get();
    if (auto fueled = RefillFuel(*state); !fueled) {
        return Err(fueled.error());
    }

    uint32_t payload_len = static_cast<uint32_t>(payload.size());
    auto allocated_payload = AllocateGuestBytes(*state, payload);
    if (!allocated_payload) {
        return Err(allocated_payload.error());
    }
    const uint32_t payload_ptr = *allocated_payload;

    auto dispatched = FromTrapResult(state->ext_on_event->call(state->store, std::make_tuple(event_type, payload_ptr, payload_len)), "ext_on_event trap");

    FreeGuestBytes(*state, payload_ptr, payload_len);
    if (!dispatched) {
        return Err(dispatched.error());
    }
    return Ok();
}

Result<i32> WasmtimeEngine::Command(Record& record, std::string_view command_id, std::span<const u8> payload) {
    auto* state = impl_->instances.at(record.id).get();
    if (!state->ext_on_command) {
        return Err(ErrorCode::InvalidState, "Extension '" + record.id + "' declares commands but does not export ext_on_command.");
    }
    if (auto fueled = RefillFuel(*state); !fueled) {
        return Err(fueled.error());
    }

    auto command_ptr = AllocateGuestBytes(*state, std::span<const u8>(reinterpret_cast<const u8*>(command_id.data()), command_id.size()));
    if (!command_ptr) {
        return Err(command_ptr.error());
    }

    const uint32_t command_len = static_cast<uint32_t>(command_id.size());
    const uint32_t payload_len = static_cast<uint32_t>(payload.size());
    auto payload_ptr = AllocateGuestBytes(*state, payload);
    if (!payload_ptr) {
        FreeGuestBytes(*state, *command_ptr, command_len);
        return Err(payload_ptr.error());
    }

    auto dispatched = FromTrapResult(state->ext_on_command->call(state->store, std::make_tuple(*command_ptr, command_len, *payload_ptr, payload_len)), "ext_on_command trap");

    FreeGuestBytes(*state, *payload_ptr, payload_len);
    FreeGuestBytes(*state, *command_ptr, command_len);
    if (!dispatched) {
        return Err(dispatched.error());
    }
    return Ok(*dispatched);
}

void WasmtimeEngine::Discard(Record& record) {
    impl_->instances.erase(record.id);
}

void WasmtimeEngine::Unload(Record& record) {
    const auto it = impl_->instances.find(record.id);
    if (it == impl_->instances.end()) {
        return;
    }

    auto* state = it->second.get();
    (void)RefillFuel(*state);
    (void)FromTrapResult(state->ext_on_unload->call(state->store, std::monostate{}), "ext_on_unload trap");
    impl_->instances.erase(it);
}

} // namespace woki::ext::wasm
