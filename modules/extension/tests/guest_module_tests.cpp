#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/ext.hpp>

#include "wasm_test_compiler.hpp"

namespace {

namespace fs = std::filesystem;

void WriteBytes(const fs::path& path, std::initializer_list<unsigned char> bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.good());
    for (unsigned char byte : bytes) {
        output.put(static_cast<char>(byte));
    }
}

#if defined(WOKI_EXTENSION_WITH_WASMTIME)

void WriteFile(const fs::path& path, std::string_view contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.good());
    output << contents;
}

#endif

} // namespace

TEST_CASE("Guest wasm magic validation") {
    const fs::path root = fs::temp_directory_path() / "woki_guest_module_tests";
    fs::create_directories(root);
    const fs::path wasm = root / "bad.wasm";

    WriteBytes(wasm, {0x00, 0x00, 0x00, 0x00});
    auto invalid = woki::ext::wasm::ValidateWasmMagic(wasm);
    REQUIRE_FALSE(invalid.has_value());

    WriteBytes(wasm, {0x00, 0x61, 0x73, 0x6d});
    auto valid = woki::ext::wasm::ValidateWasmMagic(wasm);
    REQUIRE(valid.has_value());
}

#if defined(WOKI_EXTENSION_WITH_WASMTIME)

TEST_CASE("Guest wasm export inspection") {
    const fs::path root = fs::temp_directory_path() / "woki_guest_module_tests_exports";
    fs::create_directories(root);
    const fs::path source = root / "extension.c";
    const fs::path wasm = root / "extension.wasm";

    WriteFile(source, R"c(
char memory_anchor[8192];
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) { return 0; }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double dt) { (void)dt; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned t, unsigned p, unsigned l) { (void)t; (void)p; (void)l; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c");

    REQUIRE(woki_test_compile_wasm(source.string(), wasm.string(),
        "--export=ext_api_version --export=ext_init --export=ext_on_tick "
        "--export=ext_on_event --export=ext_on_unload"));

    auto info = woki::ext::wasm::InspectGuestModule(wasm);
    REQUIRE(info.has_value());
    REQUIRE(info->valid_magic);
    REQUIRE(info->ext_api_version);
    REQUIRE(info->ext_init);
    REQUIRE(info->ext_on_tick);
    REQUIRE(info->ext_on_event);
    REQUIRE(info->ext_on_unload);
    REQUIRE_FALSE(info->ext_on_command);

    woki::ext::Manifest manifest;
    manifest.id = "woki.test";
    manifest.commands.push_back(woki::ext::CommandContribution{"woki.test.hello", "Hello", "Examples"});
    auto invalid = woki::ext::wasm::ValidateGuestModule(wasm, manifest);
    REQUIRE_FALSE(invalid.has_value());
}

#endif
