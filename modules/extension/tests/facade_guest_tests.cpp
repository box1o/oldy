#include <array>
#include <memory>
#include <string>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <string_view>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/host/api.hpp>
#include <woki/ext/registry.hpp>
#include <woki/ext/sdk/events.h>
#include <woki/ext/internal/event_service.hpp>
#if defined(WOKI_EXTENSION_WITH_WASMTIME)
#include <woki/ext/wasm/wasmtime_engine.hpp>
#endif

namespace {

namespace fs = std::filesystem;

#if defined(WOKI_EXTENSION_WITH_WASMTIME)

fs::path FacadeTempRoot(std::string_view name) {
    const fs::path root = fs::temp_directory_path() / "woki_facade_guest_tests" / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void WriteFacadeFile(const fs::path& path, std::string_view contents) {
    std::ofstream output(path);
    REQUIRE(output.good());
    output << contents;
}

woki::ext::ExtensionPackage FacadePackage(const fs::path& root, std::string id) {
    woki::ext::Manifest manifest;
    manifest.id = id;
    manifest.name = id;
    manifest.version = "1.0.0";
    manifest.requested_capabilities.permissions = {woki::ext::Permission::Log, woki::ext::Permission::Events, woki::ext::Permission::Paths, woki::ext::Permission::Storage, woki::ext::Permission::Config};
    manifest.commands = {{id + ".verify", "Verify", ""}};
    auto package = woki::ext::ExtensionPackage::Create(id, std::move(manifest), {root, root / "manifest.yaml", root / "extension.wasm", root / "data", root / "config", root / "cache"});
    REQUIRE(package);
    return std::move(*package);
}

woki::ext::host::HostApi FacadeHost(const woki::ext::ExtensionPackage& package,
    const std::shared_ptr<woki::ext::host::EventService>& service,
    std::vector<woki::ext::Permission> permissions = {woki::ext::Permission::Log, woki::ext::Permission::Events, woki::ext::Permission::Paths, woki::ext::Permission::Storage, woki::ext::Permission::Config}) {
    return woki::ext::host::HostApi({package.Id(), std::move(permissions), package.Layout().data_root, package.Layout().config_root, package.Layout().cache_root, service});
}

bool CompileFacadeGuest(const fs::path& root) {
    const fs::path source = root / "extension.cpp";
    const fs::path imports = root / "imports.txt";
    WriteFacadeFile(imports, "host_log\nhost_path_data\nhost_file_read_n\nhost_file_write_n\nhost_config_get\nhost_config_set\n");
    WriteFacadeFile(source, R"cpp(
#include <woki/extension.hpp>

using namespace woki;

class FacadeGuest final {
public:
    ~FacadeGuest() {}

    void OnAttach() noexcept {
        (void)slog::Info("facade attached: ", 1u);
        state = 1u;
    }

    void OnUpdate(f64 delta) noexcept {
        if (delta == 2.5) state |= 1u;
    }

    void OnEvent(events::Event& event) noexcept {
        events::EventDispatcher dispatcher{event};
        dispatcher.Dispatch<events::WindowResizedEvent>([&](events::WindowResizedEvent resized) noexcept {
            if (resized.width == 800u && resized.height == 600u) state |= 2u;
        });
        if (event.IsNamed() && event.Topic() == "woki.facade.signal") {
            const Bytes payload = event.Payload();
            if (payload.Size() == 1u && payload.Data()[0] == 7u) state |= 4u;
        }
    }

    Status OnCommand(const extension::Command& command) noexcept {
        events::Event null_payload{WOKI_EXT_EVENT_WINDOW_RESIZED, nullptr, 8u};
        const u8 byte = 0;
        events::Event short_payload{WOKI_EXT_EVENT_WINDOW_RESIZED, &byte, 1u};
        if (null_payload.Is<events::WindowResizedEvent>() || short_payload.Is<events::WindowResizedEvent>())
            return Status::Error();
        if (command.Id() != "woki.facade.verify")
            return Status::NotFound();
        if (command.Payload().Size() != 1u || command.Payload().Data()[0] != 9u || state != 7u)
            return Status::Invalid();
        StringBuffer<4096> data_path;
        if (const Status status = paths::Data(data_path); !status)
            return status;
        if (const Status status = config::Set("answer", "42"); !status)
            return status;
        StringBuffer<16> answer;
        if (const Status status = config::Get("answer", answer); !status || answer.View() != "42")
            return status ? Status::Error() : status;
        const char embedded_key[]{'b', 'a', 'd', '\0', 'x'};
        if (config::Set({embedded_key, 5u}, "rejected").Code() != WOKI_EXT_INVALID)
            return Status::Error();
        const u8 expected[]{4u, 2u};
        if (const Status status = storage::Write("state.bin", {expected, 2u}); !status)
            return status;
        u8 bytes[2]{};
        MutableBytes output{bytes, 2u};
        if (const Status status = storage::Read("state.bin", output); !status || output.Size() != 2u || bytes[0] != 4u || bytes[1] != 2u)
            return status ? Status::Error() : status;
        return slog::Info("facade verified");
    }

    void OnDetach() noexcept { state = 0u; }

private:
    u32 state{};
};

WOKI_EXTENSION(FacadeGuest)
)cpp");

    const std::string command = std::string("\"") + WOKI_TEST_WASM_CLANGXX
                                + "\" --target=wasm32-unknown-unknown -std=c++23 -nostdlib -fno-builtin -fno-exceptions -fno-rtti"
                                  " -I\"" WOKI_EXTENSION_SOURCE_DIR "/sdk\""
                                  " -DWOKI_EXT_HAS_PATHS=1 -DWOKI_EXT_HAS_STORAGE=1 -DWOKI_EXT_HAS_CONFIG=1 -DWOKI_EXT_HAS_EVENTS=1"
                                  " -Wl,--no-entry -Wl,--export-memory -Wl,--max-memory=33554432"
                                  " -Wl,--allow-undefined-file=\""
                                + imports.string()
                                + "\""
                                  " -Wl,--export=ext_api_version -Wl,--export=ext_init -Wl,--export=ext_on_tick"
                                  " -Wl,--export=ext_on_event -Wl,--export=ext_on_event_named -Wl,--export=ext_on_unload"
                                  " -Wl,--export=ext_on_command -Wl,--export=ext_alloc -Wl,--export=ext_free"
                                  " -o \""
                                + (root / "extension.wasm").string() + "\" \"" + source.string() + "\"";
    return std::system(command.c_str()) == 0;
}

bool RejectFacadeGuest(const fs::path& root, std::string_view body) {
    const fs::path source = root / "rejected.cpp";
    WriteFacadeFile(source, std::string("#include <woki/extension.hpp>\n") + std::string(body));
    const std::string command = std::string("\"") + WOKI_TEST_WASM_CLANGXX
                                + "\" --target=wasm32-unknown-unknown -std=c++23 -nostdlib -fno-builtin -fno-exceptions -fno-rtti"
                                  " -I\"" WOKI_EXTENSION_SOURCE_DIR "/sdk\" -fsyntax-only \""
                                + source.string() + "\" >/dev/null 2>&1";
    return std::system(command.c_str()) != 0;
}

#endif

} // namespace

#if defined(WOKI_EXTENSION_WITH_WASMTIME)

TEST_CASE("C++ facade guest runs lifecycle, allocator, command, and event paths in Wasmtime") {
    const fs::path root = FacadeTempRoot("facade");
    REQUIRE(CompileFacadeGuest(root));
    auto package = FacadePackage(root, "woki.facade");
    auto service = std::make_shared<woki::ext::host::EventService>();
    woki::ext::wasm::WasmtimeEngine engine;
    auto instance = engine.Create(package, FacadeHost(package, service));
    REQUIRE(instance);
    REQUIRE((*instance)->Initialize());

    REQUIRE((*instance)->Tick(2.5));
    const std::array<woki::u8, 8> resized{0x20, 0x03, 0, 0, 0x58, 0x02, 0, 0};
    REQUIRE((*instance)->DispatchEvent(WOKI_EXT_EVENT_WINDOW_RESIZED, resized));
    const std::array<woki::u8, 1> named_payload{7};
    REQUIRE((*instance)->DispatchNamedEvent("woki.facade.signal", named_payload));
    const std::array<woki::u8, 1> command_payload{9};
    CHECK((*instance)->DispatchCommand("woki.facade.verify", command_payload));
    CHECK_FALSE((*instance)->DispatchCommand("woki.facade.unknown", {}));
    (*instance)->Unload();
}

TEST_CASE("C++ extension facade rejects non-conforming callbacks and subscriptions") {
    const fs::path root = FacadeTempRoot("compile-rejections");
    CHECK(RejectFacadeGuest(root, "struct Bad { void OnAttach() {} }; WOKI_EXTENSION(Bad)\n"));
    CHECK(RejectFacadeGuest(root, "struct Bad { int OnUpdate(woki::f64) noexcept { return 0; } }; WOKI_EXTENSION(Bad)\n"));
    CHECK(RejectFacadeGuest(root, "struct Bad { void OnEvent(const woki::events::Event&) noexcept {} }; WOKI_EXTENSION(Bad)\n"));
    CHECK(RejectFacadeGuest(root, "struct Bad { void OnCommand(woki::extension::Command&) noexcept {} }; WOKI_EXTENSION(Bad)\n"));
    CHECK(RejectFacadeGuest(root, "void f() { woki::events::Subscribe(); }\n"));
}

TEST_CASE("Staged Kitty package executes through Wasmtime when available") {
    const fs::path root = WOKI_KITTY_PACKAGE_DIR;
    REQUIRE(fs::is_regular_file(root / "extension.wasm"));
    woki::ext::Manifest manifest;
    manifest.id = "woki.kitty";
    manifest.name = "kitty";
    manifest.version = "0.1.0";
    manifest.requested_capabilities.permissions = {woki::ext::Permission::Log, woki::ext::Permission::Events};
    manifest.commands = {{"woki.kitty.pet", "Pet the Kitty", "Fun"}, {"woki.kitty.complex", "Complex command", "Fun"}};
    const auto permissions = manifest.requested_capabilities.permissions;
    auto package_result = woki::ext::ExtensionPackage::Create(manifest.id, std::move(manifest), {root, root / "manifest.yaml", root / "extension.wasm", root / "data", root / "config", root / "cache"});
    REQUIRE(package_result);
    auto package = std::move(*package_result);
    auto service = std::make_shared<woki::ext::host::EventService>();
    woki::ext::wasm::WasmtimeEngine engine;
    auto instance = engine.Create(package, FacadeHost(package, service, permissions));
    REQUIRE(instance);
    REQUIRE((*instance)->Initialize());
    const std::array<woki::u8, 8> resized{0x80, 0x07, 0, 0, 0x38, 0x04, 0, 0};
    REQUIRE((*instance)->DispatchEvent(WOKI_EXT_EVENT_WINDOW_RESIZED, resized));
    const std::array<woki::u8, 6> key_pressed{69, 0, 0, 0, 0, 0};
    REQUIRE((*instance)->DispatchEvent(WOKI_EXT_EVENT_KEY_PRESSED, key_pressed));
    CHECK((*instance)->DispatchCommand("woki.kitty.pet", {}));
    CHECK((*instance)->Tick(16.0));
    CHECK((*instance)->DispatchCommand("woki.kitty.complex", {}));
    CHECK_FALSE((*instance)->DispatchCommand("woki.kitty.unknown", {}));
    (*instance)->Unload();
}

#endif
