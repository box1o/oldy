#include <array>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <string_view>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/limits.hpp>
#include <woki/ext/host/api.hpp>
#include <woki/ext/registry.hpp>
#include <woki/ext/wasm/web_engine.hpp>
#include <woki/ext/internal/event_service.hpp>
#if defined(WOKI_EXTENSION_WITH_WASMTIME)
#include <woki/ext/wasm/wasmtime_engine.hpp>
#endif

#include "wasm_test_compiler.hpp"

namespace {

namespace fs = std::filesystem;

#ifndef __EMSCRIPTEN__
[[nodiscard]] fs::path TempRoot(std::string_view name) {
    const fs::path root = fs::temp_directory_path() / "woki_engine_adapter_tests" / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

#if defined(WOKI_EXTENSION_WITH_WASMTIME)
void WriteText(const fs::path& path, std::string_view text) {
    std::ofstream output(path);
    REQUIRE(output.good());
    output << text;
}

void WriteBytes(const fs::path& path, std::initializer_list<unsigned char> bytes) {
    std::ofstream output(path, std::ios::binary);
    REQUIRE(output.good());
    output.write(reinterpret_cast<const char*>(bytes.begin()), static_cast<std::streamsize>(bytes.size()));
}
#endif

[[nodiscard]] woki::ext::ExtensionPackage MakePackage(const fs::path& root, std::string id = "woki.test", std::vector<woki::ext::Permission> permissions = {}, std::vector<woki::ext::CommandContribution> commands = {}) {
    woki::ext::Manifest manifest;
    manifest.id = id;
    manifest.name = "Test";
    manifest.version = "1.0.0";
    manifest.requested_capabilities.permissions = std::move(permissions);
    manifest.commands = std::move(commands);
    auto package = woki::ext::ExtensionPackage::Create(id, std::move(manifest), {root, root / "manifest.yaml", root / "extension.wasm", root / "data", root / "config", root / "cache"});
    REQUIRE(package);
    return std::move(*package);
}

[[nodiscard]] woki::ext::host::HostApi MakeHost(const woki::ext::ExtensionPackage& package) {
    return woki::ext::host::HostApi({package.Id(), package.GetManifest().requested_capabilities.permissions, package.Layout().data_root, package.Layout().config_root, package.Layout().cache_root, {}});
}
#endif

#if defined(WOKI_EXTENSION_WITH_WASMTIME)
constexpr std::string_view kExports = "--export=ext_api_version --export=ext_init --export=ext_on_tick --export=ext_on_event --export=ext_on_unload";

class RecordingBus final : public woki::ext::host::EventBus {
public:
    void Publish(const woki::ext::host::Event& event) override {
        events.push_back(event);
    }

    std::vector<woki::ext::host::Event> events;
};

void Compile(const fs::path& root, std::string_view source, std::string_view extra_exports = {}) {
    WriteText(root / "extension.c", source);
    const std::string exports = std::string(kExports) + " " + std::string(extra_exports);
    REQUIRE(woki_test_compile_wasm((root / "extension.c").string(), (root / "extension.wasm").string(), exports));
}
#endif

} // namespace

TEST_CASE("WebEngine reports its native adapter contract deterministically") {
#ifndef __EMSCRIPTEN__
    const fs::path root = TempRoot("web_native");
    const auto package = MakePackage(root);
    woki::ext::wasm::WebEngine engine;
    const auto created = engine.Create(package, MakeHost(package));
    REQUIRE_FALSE(created);
    REQUIRE(created.error().Code() == woki::ErrorCode::InvalidState);
    REQUIRE(created.error().Message().contains("Emscripten"));
    const auto selected = woki::ext::wasm::CreateEngine();
#if defined(WOKI_EXTENSION_WITH_WASMTIME)
    REQUIRE(selected != nullptr);
#else
    REQUIRE(selected == nullptr);
#endif
#endif
}

#if defined(WOKI_EXTENSION_WITH_WASMTIME)

TEST_CASE("Wasmtime real engine drives lifecycle, payloads, and independent stores") {
    const fs::path root = TempRoot("wasmtime_lifecycle");
    Compile(root, R"c(
static unsigned char buffers[4096];
static unsigned cursor;
static unsigned initialized;
static unsigned events;
static unsigned commands;
__attribute__((export_name("ext_alloc"))) unsigned ext_alloc(unsigned len) {
    if (!len || cursor + len > sizeof(buffers)) return 0;
    unsigned result = (unsigned)(buffers + cursor); cursor += len; return result;
}

__attribute__((export_name("ext_free"))) void ext_free(unsigned ptr, unsigned len) {
    (void)ptr;
    (void)len;
}

__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) {
    return 1;
}

__attribute__((export_name("ext_init"))) int ext_init(void) {
    initialized++;
    return initialized == 1 ? 0 : 9;
}

__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double delta) {
    if (delta == 7.5)
        events += 10;
}

__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned type, unsigned ptr, unsigned len) {
    unsigned char* p = (unsigned char*)ptr;
    if (type == 12 && len == 3 && p[0] == 4 && p[1] == 5 && p[2] == 6)
        events++;
}

__attribute__((export_name("ext_on_command"))) int ext_on_command(unsigned id_ptr, unsigned id_len, unsigned payload_ptr, unsigned payload_len) {
    unsigned char* id = (unsigned char*)id_ptr;
    unsigned char* p = (unsigned char*)payload_ptr;
    if (id_len == 13 && id[0] == 'w' && id[12] == 'n' && payload_len == 2 && p[0] == 8 && p[1] == 9)
        commands++;
    return commands == 1 ? 0 : 4;
}

__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {
    initialized = 99;
}
)c",
        "--export=ext_alloc --export=ext_free --export=ext_on_command");

    auto first_package = MakePackage(root, "woki.first", {}, {{"woki.first.run", "Run", ""}});
    auto second_package = MakePackage(root, "woki.second", {}, {{"woki.second.run", "Run", ""}});
    woki::ext::wasm::WasmtimeEngine engine;
    auto first = engine.Create(first_package, MakeHost(first_package));
    auto second = engine.Create(second_package, MakeHost(second_package));
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE((*first)->Initialize());
    REQUIRE((*second)->Initialize());
    REQUIRE((*first)->Tick(7.5));
    const std::array<woki::u8, 3> event{4, 5, 6};
    REQUIRE((*first)->DispatchEvent(12, event));
    const std::array<woki::u8, 2> payload{8, 9};
    REQUIRE((*first)->DispatchCommand("woki.test.run", payload));
    REQUIRE((*second)->DispatchCommand("woki.test.run", payload));
    (*first)->Unload();
    (*first)->Unload();
    (*second)->Unload();
}

TEST_CASE("Wasmtime event subscription imports are compatible no-ops and emission is queued") {
    const fs::path root = TempRoot("wasmtime_events");
    Compile(root, R"c(
__attribute__((import_module("woki_host"), import_name("host_event_subscribe"))) extern int host_event_subscribe(unsigned);
__attribute__((import_module("woki_host"), import_name("host_event_emit"))) extern int host_event_emit(unsigned, const unsigned char*, unsigned);
static const unsigned char payload[] = {3, 1, 4};
static unsigned char buffer[8];
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) { return host_event_subscribe(77); }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double d) {
    if (d == 2.0) (void)host_event_emit(0x8000002au, payload, sizeof(payload));
}

__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned a, unsigned b, unsigned c) {
    (void)a;
    (void)b;
    (void)c;
}

__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}

__attribute__((export_name("ext_alloc"))) unsigned ext_alloc(unsigned len) {
    return len <= sizeof(buffer) ? (unsigned)buffer : 0;
}

__attribute__((export_name("ext_free"))) void ext_free(unsigned ptr, unsigned len) {
    (void)ptr;
    (void)len;
}
)c",
        "--export=ext_alloc --export=ext_free");

    auto package = MakePackage(root, "woki.events", {woki::ext::Permission::Events});
    auto service = std::make_shared<woki::ext::host::EventService>();
    RecordingBus bus;
    service->SetBus(&bus);
    woki::ext::host::HostApi host({package.Id(), package.GetManifest().requested_capabilities.permissions, package.Layout().data_root, package.Layout().config_root, package.Layout().cache_root, service});

    woki::ext::wasm::WasmtimeEngine engine;
    auto instance = engine.Create(package, std::move(host));
    const std::string instance_error = instance ? std::string{} : std::string(instance.error().Message());
    INFO(instance_error);
    REQUIRE(instance);
    REQUIRE((*instance)->Initialize());
    REQUIRE((*instance)->Tick(2.0));
    REQUIRE(bus.events.empty());
    service->Drain();
    REQUIRE(bus.events.size() == 1);
    REQUIRE(bus.events[0].type == (woki::ext::host::kExtensionEventNamespace | 42));
    REQUIRE(bus.events[0].payload == std::vector<woki::u8>{3, 1, 4});
    REQUIRE(bus.events[0].origin.extension_id == "woki.events");
    (*instance)->Unload();
}

TEST_CASE("Wasmtime reads and writes ABI u32 values as little endian") {
    const fs::path root = TempRoot("wasmtime_little_endian_u32");
    Compile(root, R"c(
__attribute__((import_module("woki_host"), import_name("host_file_read"))) extern int host_file_read(const char*, unsigned char*, unsigned char*);
static unsigned char output[4];
static unsigned char length[4] = {4, 0, 0, 0};
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) {
    (void)host_file_read("state", output, length);
    return length[0] == 8 && length[1] == 0 && length[2] == 0 && length[3] == 0 ? 0 : 9;
}
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double d) { (void)d; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned a, unsigned b, unsigned c) { (void)a;(void)b;(void)c; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c");
    auto package = MakePackage(root, "woki.endian", {woki::ext::Permission::Storage});
    fs::create_directories(package.Layout().data_root);
    WriteText(package.Layout().data_root / "state", "12345678");
    woki::ext::wasm::WasmtimeEngine engine;
    auto instance = engine.Create(package, MakeHost(package));
    REQUIRE(instance);
    CHECK((*instance)->Initialize());
}

TEST_CASE("Wasmtime classifies documented guest command statuses") {
    const fs::path root = TempRoot("wasmtime_command_status");
    Compile(root, R"c(
static unsigned char buffer[64];
__attribute__((export_name("ext_alloc"))) unsigned ext_alloc(unsigned len) { return len <= sizeof(buffer) ? (unsigned)buffer : 0; }
__attribute__((export_name("ext_free"))) void ext_free(unsigned ptr, unsigned len) { (void)ptr; (void)len; }
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) { return 0; }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double delta) { (void)delta; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned type, unsigned ptr, unsigned len) { (void)type; (void)ptr; (void)len; }
__attribute__((export_name("ext_on_command"))) int ext_on_command(unsigned id_ptr, unsigned id_len, unsigned payload_ptr, unsigned payload_len) {
    (void)id_ptr; (void)id_len; (void)payload_ptr; (void)payload_len; return -2;
}
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c",
        "--export=ext_alloc --export=ext_free --export=ext_on_command");
    auto package = MakePackage(root, "woki.status", {}, {{"woki.status.run", "Run", ""}});
    woki::ext::wasm::WasmtimeEngine engine;
    auto instance = engine.Create(package, MakeHost(package));
    REQUIRE(instance);
    REQUIRE((*instance)->Initialize());
    auto result = (*instance)->DispatchCommand("woki.status.run", {});
    REQUIRE_FALSE(result);
    CHECK(result.error().Code() == woki::ErrorCode::FileAccessDenied);
}

TEST_CASE("Wasmtime treats allocator cleanup fuel exhaustion as a callback failure") {
    const fs::path root = TempRoot("wasmtime_allocator_cleanup");
    Compile(root, R"c(
static unsigned char buffer[64];
__attribute__((export_name("ext_alloc"))) unsigned ext_alloc(unsigned len) { return len <= sizeof(buffer) ? (unsigned)buffer : 0; }
__attribute__((export_name("ext_free"))) void ext_free(unsigned ptr, unsigned len) { (void)ptr; (void)len; for (;;) {} }
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) { return 0; }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double delta) { (void)delta; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned type, unsigned ptr, unsigned len) { (void)type; (void)ptr; (void)len; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c",
        "--export=ext_alloc --export=ext_free");
    auto package = MakePackage(root, "woki.cleanup");
    woki::ext::wasm::WasmtimeEngine engine;
    auto instance = engine.Create(package, MakeHost(package));
    REQUIRE(instance);
    REQUIRE((*instance)->Initialize());
    const std::array<woki::u8, 1> payload{7};
    const auto dispatched = (*instance)->DispatchEvent(1, payload);
    REQUIRE_FALSE(dispatched);
    CHECK(dispatched.error().Message().contains("ext_free"));
}

TEST_CASE("Wasmtime frees callback payloads after a guest callback fails") {
    const fs::path root = TempRoot("wasmtime_callback_cleanup");
    Compile(root, R"c(
static unsigned char buffers[2][64];
static unsigned allocated[2];
static unsigned calls;
__attribute__((export_name("ext_alloc"))) unsigned ext_alloc(unsigned len) {
    if (!len || len > sizeof(buffers[0])) return 0;
    for (unsigned i = 0; i < 2; ++i) {
        if (!allocated[i]) { allocated[i] = len; return (unsigned)buffers[i]; }
    }
    return 0;
}
__attribute__((export_name("ext_free"))) void ext_free(unsigned ptr, unsigned len) {
    for (unsigned i = 0; i < 2; ++i) {
        if (ptr == (unsigned)buffers[i] && allocated[i] == len) allocated[i] = 0;
    }
}
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) { return 0; }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double delta) { (void)delta; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned type, unsigned ptr, unsigned len) { (void)type; (void)ptr; (void)len; }
__attribute__((export_name("ext_on_command"))) int ext_on_command(unsigned id_ptr, unsigned id_len, unsigned payload_ptr, unsigned payload_len) {
    (void)id_ptr; (void)id_len; (void)payload_ptr; (void)payload_len;
    return calls++ == 0 ? -2 : 0;
}
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c",
        "--export=ext_alloc --export=ext_free --export=ext_on_command");
    auto package = MakePackage(root, "woki.callback-cleanup", {}, {{"woki.callback-cleanup.run", "Run", ""}});
    woki::ext::wasm::WasmtimeEngine engine;
    auto instance = engine.Create(package, MakeHost(package));
    REQUIRE(instance);
    REQUIRE((*instance)->Initialize());
    const std::array<woki::u8, 1> payload{7};
    const auto failed = (*instance)->DispatchCommand("woki.callback-cleanup.run", payload);
    REQUIRE_FALSE(failed);
    CHECK(failed.error().Code() == woki::ErrorCode::FileAccessDenied);
    CHECK((*instance)->DispatchCommand("woki.callback-cleanup.run", payload));
}

TEST_CASE("Wasmtime accepts zero-length spans at the end of guest memory") {
    const fs::path root = TempRoot("wasmtime_empty_span");
    Compile(root, R"c(
__attribute__((import_module("woki_host"), import_name("host_log"))) extern int host_log(unsigned, const char*, unsigned);
char memory_anchor[8];
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) {
    unsigned end = __builtin_wasm_memory_size(0) * 65536u;
    return host_log(1, (const char*)end, 0);
}
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double delta) { (void)delta; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned type, unsigned ptr, unsigned len) { (void)type; (void)ptr; (void)len; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c");
    auto package = MakePackage(root, "woki.empty-span", {woki::ext::Permission::Log});
    woki::ext::wasm::WasmtimeEngine engine;
    auto instance = engine.Create(package, MakeHost(package));
    REQUIRE(instance);
    CHECK((*instance)->Initialize());
}

TEST_CASE("Wasmtime real engine reports compile, export, signature, version, and import errors") {
    woki::ext::wasm::WasmtimeEngine engine;

    const fs::path missing = TempRoot("wasmtime_missing_file");
    auto package = MakePackage(missing);
    REQUIRE_FALSE(engine.Create(package, MakeHost(package)));

    const fs::path invalid = TempRoot("wasmtime_invalid_binary");
    WriteText(invalid / "extension.wasm", "not wasm");
    package = MakePackage(invalid);
    REQUIRE_FALSE(engine.Create(package, MakeHost(package)));

    const fs::path missing_export = TempRoot("wasmtime_missing_export");
    WriteText(missing_export / "extension.c", R"c(
char memory_anchor[8];
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double d) { (void)d; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned a, unsigned b, unsigned c) { (void)a;(void)b;(void)c; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c");
    REQUIRE(woki_test_compile_wasm((missing_export / "extension.c").string(), (missing_export / "extension.wasm").string(), "--export=ext_api_version --export=ext_on_tick --export=ext_on_event --export=ext_on_unload"));
    package = MakePackage(missing_export);
    const auto absent = engine.Create(package, MakeHost(package));
    REQUIRE_FALSE(absent);
    REQUIRE(absent.error().Message().contains("ext_init"));

    const fs::path exports = TempRoot("wasmtime_exports");
    Compile(exports, R"c(
char memory_anchor[8];
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(unsigned wrong) { return (int)wrong; }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double d) { (void)d; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned a, unsigned b, unsigned c) { (void)a;(void)b;(void)c; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c");
    package = MakePackage(exports);
    const auto signature = engine.Create(package, MakeHost(package));
    REQUIRE_FALSE(signature);
    REQUIRE(signature.error().Message().contains("ext_init"));

    const fs::path version = TempRoot("wasmtime_version");
    Compile(version, R"c(
char memory_anchor[8];
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 99; }
__attribute__((export_name("ext_init"))) int ext_init(void) { return 0; }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double d) { (void)d; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned a, unsigned b, unsigned c) { (void)a;(void)b;(void)c; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c");
    package = MakePackage(version);
    const auto mismatch = engine.Create(package, MakeHost(package));
    REQUIRE_FALSE(mismatch);
    REQUIRE(mismatch.error().Message().contains("apiVersion mismatch"));

    const fs::path import = TempRoot("wasmtime_permission_import");
    Compile(import, R"c(
__attribute__((import_module("woki_host"), import_name("host_log"))) extern int host_log(unsigned, const char*, unsigned);
char memory_anchor[8];
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) { return host_log(1, "x", 1); }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double d) { (void)d; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned a, unsigned b, unsigned c) { (void)a;(void)b;(void)c; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c");
    package = MakePackage(import);
    const auto denied = engine.Create(package, MakeHost(package));
    REQUIRE_FALSE(denied);
    REQUIRE(denied.error().Message().contains("host_log"));
}

TEST_CASE("Wasmtime real engine validates guest pointers, fuel, and host payload boundaries") {
    const fs::path pointer = TempRoot("wasmtime_pointer");
    Compile(pointer, R"c(
__attribute__((import_module("woki_host"), import_name("host_log"))) extern int host_log(unsigned, const char*, unsigned);
char memory_anchor[8];
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) { return host_log(1, (const char*)0x7fffffff, 4); }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double d) { (void)d; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned a, unsigned b, unsigned c) { (void)a;(void)b;(void)c; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c");
    auto package = MakePackage(pointer, "woki.pointer", {woki::ext::Permission::Log});
    woki::ext::wasm::WasmtimeEngine engine;
    auto instance = engine.Create(package, MakeHost(package));
    REQUIRE(instance);
    const auto initialized = (*instance)->Initialize();
    REQUIRE_FALSE(initialized);
    REQUIRE(initialized.error().Message().contains("-5"));
    (*instance)->Unload();

    const fs::path fuel = TempRoot("wasmtime_fuel");
    Compile(fuel, R"c(
char memory_anchor[8];
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) { return 0; }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double d) { volatile unsigned x = (unsigned)d; for (;;) x++; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned a, unsigned b, unsigned c) { (void)a;(void)b;(void)c; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c");
    package = MakePackage(fuel, "woki.fuel");
    instance = engine.Create(package, MakeHost(package));
    REQUIRE(instance);
    REQUIRE((*instance)->Initialize());
    const auto exhausted = (*instance)->Tick(1.0);
    REQUIRE_FALSE(exhausted);
    REQUIRE(exhausted.error().Message().contains("fuel"));
    (*instance)->Unload();

    const fs::path payload = TempRoot("wasmtime_payload_limits");
    Compile(payload, R"c(
static unsigned char buffer[16];
__attribute__((export_name("ext_alloc"))) unsigned ext_alloc(unsigned len) { return len <= sizeof(buffer) ? (unsigned)buffer : 0; }
__attribute__((export_name("ext_free"))) void ext_free(unsigned ptr, unsigned len) { (void)ptr; (void)len; }
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) { return 0; }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double d) { (void)d; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned a, unsigned b, unsigned c) { (void)a;(void)b;(void)c; }
__attribute__((export_name("ext_on_command"))) int ext_on_command(unsigned a, unsigned b, unsigned c, unsigned d) { (void)a;(void)b;(void)c;(void)d; return 0; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c",
        "--export=ext_alloc --export=ext_free --export=ext_on_command");
    package = MakePackage(payload, "woki.payload", {}, {{"woki.payload.run", "Run", ""}});
    instance = engine.Create(package, MakeHost(package));
    REQUIRE(instance);
    REQUIRE((*instance)->Initialize());
    const std::vector<woki::u8> oversized(woki::ext::limits::kMaxEventBytes + 1);
    REQUIRE((*instance)->DispatchEvent(1, oversized).error().Code() == woki::ErrorCode::ValidationOutOfRange);
    REQUIRE((*instance)->DispatchCommand("run", oversized).error().Code() == woki::ErrorCode::ValidationOutOfRange);
    REQUIRE((*instance)->DispatchCommand("", {}).error().Code() == woki::ErrorCode::ValidationOutOfRange);
    (*instance)->Unload();
}

TEST_CASE("Wasmtime store limiter enforces memory and rejects non-ABI resource modules") {
    const fs::path root = TempRoot("wasmtime_memory_limiter");
    Compile(root, R"c(
char memory_anchor[8];
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) {
    return __builtin_wasm_memory_grow(0, 600) == (unsigned)-1 ? 0 : 7;
}
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double d) { (void)d; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned a, unsigned b, unsigned c) { (void)a;(void)b;(void)c; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c");
    auto package = MakePackage(root, "woki.memory-limit");
    woki::ext::wasm::WasmtimeEngine engine;
    auto instance = engine.Create(package, MakeHost(package));
    REQUIRE(instance);
    REQUIRE((*instance)->Initialize());
    (*instance)->Unload();

    const fs::path table_elements = TempRoot("wasmtime_table_element_limiter");
    WriteBytes(table_elements / "extension.wasm", {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x04, 0x05, 0x01, 0x70, 0x00, 0x91, 0x4e});
    package = MakePackage(table_elements, "woki.table-elements");
    const auto oversized_table = engine.Create(package, MakeHost(package));
    REQUIRE_FALSE(oversized_table);
    REQUIRE(oversized_table.error().Message().contains("memory"));

    const fs::path table_count = TempRoot("wasmtime_table_count_limiter");
    WriteBytes(table_count / "extension.wasm", {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x04, 0x07, 0x02, 0x70, 0x00, 0x00, 0x70, 0x00, 0x00});
    package = MakePackage(table_count, "woki.table-count");
    const auto too_many_tables = engine.Create(package, MakeHost(package));
    REQUIRE_FALSE(too_many_tables);
    REQUIRE(too_many_tables.error().Message().contains("memory"));

    const fs::path memory_count = TempRoot("wasmtime_memory_count_limiter");
    WriteBytes(memory_count / "extension.wasm", {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x05, 0x05, 0x02, 0x00, 0x01, 0x00, 0x01});
    package = MakePackage(memory_count, "woki.memory-count");
    const auto too_many_memories = engine.Create(package, MakeHost(package));
    REQUIRE_FALSE(too_many_memories);
    REQUIRE(too_many_memories.error().Message().contains("memory"));
}

#endif
