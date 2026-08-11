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
    manifest.requested_capabilities.permissions = {woki::ext::Permission::Events};
    manifest.commands = {{id + ".verify", "Verify", ""}};
    auto package = woki::ext::ExtensionPackage::Create(id, std::move(manifest), {root, root / "manifest.yaml", root / "extension.wasm", root / "data", root / "config", root / "cache"});
    REQUIRE(package);
    return std::move(*package);
}

woki::ext::host::HostApi FacadeHost(const woki::ext::ExtensionPackage& package,
    const std::shared_ptr<woki::ext::host::EventSession>& session,
    const std::shared_ptr<woki::ext::host::EventService>& service,
    std::vector<woki::ext::Permission> permissions = {woki::ext::Permission::Events}) {
    return woki::ext::host::HostApi({package.Id(), std::move(permissions), package.Layout().data_root, package.Layout().config_root, package.Layout().cache_root, session, service});
}

bool CompileFacadeGuest(const fs::path& root) {
    const fs::path source = root / "extension.cpp";
    const fs::path imports = root / "imports.txt";
    WriteFacadeFile(imports, "host_event_subscribe\nhost_event_subscribe_named\n");
    WriteFacadeFile(source, R"cpp(
#include <woki/ext/plugin.hpp>

using namespace woki::ext;

class FacadeGuest final {
public:
    Status OnLoad(Context& context) noexcept {
        const Status typed = context.GetEvents().Subscribe<WindowResizedEvent>();
        return typed ? context.GetEvents().Subscribe("woki.facade.signal") : typed;
    }

    void OnTick(Context&, double delta) noexcept {
        if (delta == 2.5) state |= 1u;
    }

    void OnEvent(Context&, Event& event) noexcept {
        event.Dispatch<WindowResizedEvent>([&](WindowResizedEvent resized) noexcept {
            if (resized.width == 800u && resized.height == 600u) state |= 2u;
        });
        if (event.IsNamed() && event.Topic() == "woki.facade.signal") {
            const Bytes payload = event.Payload();
            if (payload.Size() == 1u && payload.Data()[0] == 7u) state |= 4u;
        }
    }

    Status OnCommand(Context&, StringView command, Bytes payload) noexcept {
        Event null_payload{WOKI_EXT_EVENT_WINDOW_RESIZED, nullptr, 8u};
        const u8 byte = 0;
        Event short_payload{WOKI_EXT_EVENT_WINDOW_RESIZED, &byte, 1u};
        if (null_payload.Is<WindowResizedEvent>() || short_payload.Is<WindowResizedEvent>())
            return Status::Invalid();
        if (command != "woki.facade.verify" || payload.Size() != 1u || payload.Data()[0] != 9u)
            return Status::NotFound();
        return state == 7u ? Status::Success() : Status::Error();
    }

    void OnUnload(Context&) noexcept { state = 0u; }

private:
    u32 state;
};

WOKI_PLUGIN(FacadeGuest)
)cpp");

    const std::string command = std::string("\"") + WOKI_TEST_WASM_CLANGXX
                                + "\" --target=wasm32-unknown-unknown -std=c++23 -nostdlib -fno-builtin -fno-exceptions -fno-rtti"
                                  " -I\"" WOKI_EXTENSION_SOURCE_DIR "/sdk\""
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

#endif

} // namespace

#if defined(WOKI_EXTENSION_WITH_WASMTIME)

TEST_CASE("C++ facade guest runs lifecycle, allocator, command, and event paths in Wasmtime") {
    const fs::path root = FacadeTempRoot("facade");
    REQUIRE(CompileFacadeGuest(root));
    auto package = FacadePackage(root, "woki.facade");
    auto session = std::make_shared<woki::ext::host::EventSession>();
    auto service = std::make_shared<woki::ext::host::EventService>();
    woki::ext::wasm::WasmtimeEngine engine;
    auto instance = engine.Create(package, FacadeHost(package, session, service));
    REQUIRE(instance);
    REQUIRE((*instance)->Initialize());
    CHECK(session->IsSubscribed(WOKI_EXT_EVENT_WINDOW_RESIZED));
    CHECK(session->IsSubscribed("woki.facade.signal"));

    REQUIRE((*instance)->Tick(2.5));
    const std::array<woki::u8, 8> resized{0x20, 0x03, 0, 0, 0x58, 0x02, 0, 0};
    REQUIRE((*instance)->DispatchEvent(WOKI_EXT_EVENT_WINDOW_RESIZED, resized));
    const std::array<woki::u8, 1> named_payload{7};
    REQUIRE((*instance)->DispatchNamedEvent("woki.facade.signal", named_payload));
    const std::array<woki::u8, 1> command_payload{9};
    CHECK((*instance)->DispatchCommand("woki.facade.verify", command_payload));
    (*instance)->Unload();
}

TEST_CASE("Staged Kitty package executes through Wasmtime when available") {
    const fs::path root = WOKI_KITTY_PACKAGE_DIR;
    REQUIRE(fs::is_regular_file(root / "extension.wasm"));
    auto manifest = woki::ext::LoadManifest(root / "manifest.yaml");
    REQUIRE(manifest);
    const auto permissions = manifest->requested_capabilities.permissions;
    const auto wasm_path = manifest->wasm_path;
    auto package_result = woki::ext::ExtensionPackage::Create(manifest->id, std::move(*manifest), {root, root / "manifest.yaml", root / wasm_path, root / "data", root / "config", root / "cache"});
    REQUIRE(package_result);
    auto package = std::move(*package_result);
    auto session = std::make_shared<woki::ext::host::EventSession>();
    auto service = std::make_shared<woki::ext::host::EventService>();
    woki::ext::wasm::WasmtimeEngine engine;
    auto instance = engine.Create(package, FacadeHost(package, session, service, permissions));
    REQUIRE(instance);
    REQUIRE((*instance)->Initialize());
    CHECK(session->IsSubscribed(WOKI_EXT_EVENT_WINDOW_RESIZED));
    CHECK(session->IsSubscribed(WOKI_EXT_EVENT_KEY_PRESSED));
    const std::array<woki::u8, 8> resized{0x80, 0x07, 0, 0, 0x38, 0x04, 0, 0};
    REQUIRE((*instance)->DispatchEvent(WOKI_EXT_EVENT_WINDOW_RESIZED, resized));
    const std::array<woki::u8, 6> key_pressed{69, 0, 0, 0, 0, 0};
    REQUIRE((*instance)->DispatchEvent(WOKI_EXT_EVENT_KEY_PRESSED, key_pressed));
    CHECK((*instance)->DispatchCommand("woki.kitty.pet", {}));
    (*instance)->Unload();
}

#endif
