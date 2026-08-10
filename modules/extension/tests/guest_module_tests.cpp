#include <vector>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/wasm/guest_module.hpp>

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

void WriteFile(const fs::path& path, std::string_view contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.good());
    output << contents;
}

void ReplaceBytes(const fs::path& path, std::string_view from, std::string_view to) {
    REQUIRE(from.size() == to.size());
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());
    std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const std::size_t offset = bytes.find(from);
    REQUIRE(offset != std::string::npos);
    bytes.replace(offset, from.size(), to);
    WriteFile(path, bytes);
}

std::vector<unsigned char> ReadBytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void WriteBytes(const fs::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.good());
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::size_t ReadUleb(const std::vector<unsigned char>& bytes, std::size_t& offset) {
    std::size_t value = 0;
    unsigned shift = 0;
    do {
        REQUIRE(offset < bytes.size());
        const unsigned char byte = bytes[offset++];
        value |= static_cast<std::size_t>(byte & 0x7f) << shift;
        shift += 7;
        if ((byte & 0x80) == 0)
            return value;
    } while (shift < 35);
    FAIL("invalid test wasm ULEB");
    return 0;
}

std::size_t FindSection(const std::vector<unsigned char>& bytes, unsigned char wanted) {
    std::size_t offset = 8;
    while (offset < bytes.size()) {
        const std::size_t section = offset;
        const unsigned char id = bytes[offset++];
        const std::size_t size = ReadUleb(bytes, offset);
        if (id == wanted)
            return section;
        offset += size;
    }
    FAIL("test wasm section not found");
    return bytes.size();
}

void RemoveMemoryMaximum(std::vector<unsigned char>& bytes) {
    std::size_t section = FindSection(bytes, 5);
    std::size_t size_offset = section + 1;
    std::size_t payload = size_offset;
    const std::size_t old_size = ReadUleb(bytes, payload);
    REQUIRE(old_size < 128);
    REQUIRE(bytes[payload++] == 1);
    REQUIRE(bytes[payload] == 1);
    bytes[payload++] = 0;
    (void)ReadUleb(bytes, payload);
    const std::size_t maximum = payload;
    (void)ReadUleb(bytes, payload);
    const std::size_t removed = payload - maximum;
    bytes.erase(bytes.begin() + static_cast<std::ptrdiff_t>(maximum), bytes.begin() + static_cast<std::ptrdiff_t>(payload));
    bytes[size_offset] = static_cast<unsigned char>(old_size - removed);
}

void SetMemoryMaximumTo513(std::vector<unsigned char>& bytes) {
    std::size_t section = FindSection(bytes, 5) + 1;
    (void)ReadUleb(bytes, section);
    REQUIRE(bytes[section++] == 1);
    REQUIRE(bytes[section++] == 1);
    (void)ReadUleb(bytes, section);
    REQUIRE(bytes[section] == 0x80);
    REQUIRE(bytes[section + 1] == 0x04);
    bytes[section] = 0x81;
}

#if defined(WOKI_EXTENSION_WITH_WASMTIME)
void CorruptFirstFunctionInstruction(std::vector<unsigned char>& bytes) {
    std::size_t code = FindSection(bytes, 10) + 1;
    (void)ReadUleb(bytes, code);
    REQUIRE(ReadUleb(bytes, code) > 0);
    (void)ReadUleb(bytes, code);
    const std::size_t local_groups = ReadUleb(bytes, code);
    for (std::size_t i = 0; i < local_groups; ++i) {
        (void)ReadUleb(bytes, code);
        REQUIRE(code < bytes.size());
        ++code;
    }
    REQUIRE(code < bytes.size());
    bytes[code] = 0xff;
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
    REQUIRE(info->memory);
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

TEST_CASE("Guest validation rejects adversarial ABI names, signatures, memory, and allocator halves") {
    const fs::path root = fs::temp_directory_path() / "woki_guest_module_tests_adversarial";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path source = root / "extension.c";
    const fs::path wasm = root / "extension.wasm";
    woki::ext::Manifest manifest;
    manifest.id = "woki.test";

    const auto compile = [&](std::string_view init, std::string_view extra = {}, std::string_view exports = {}) {
        WriteFile(source, std::string(R"c(
char memory_anchor[8];
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) )c")
                              + std::string(init) + R"c(
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double d) { (void)d; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned a, unsigned b, unsigned c) { (void)a; (void)b; (void)c; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c" + std::string(extra));
        REQUIRE(woki_test_compile_wasm(source.string(), wasm.string(),
            std::string("--export=ext_api_version --export=ext_init --export=ext_on_tick --export=ext_on_event --export=ext_on_unload ") + std::string(exports)));
    };

    compile("int ext_init(unsigned wrong) { return (int)wrong; }");
    auto invalid = woki::ext::wasm::ValidateGuestModule(wasm, manifest);
    REQUIRE_FALSE(invalid);
    REQUIRE(invalid.error().Message().contains("ext_init"));

    compile("int ext_init(void) { return 0; }");
    ReplaceBytes(wasm, "memory", "memorz");
    invalid = woki::ext::wasm::ValidateGuestModule(wasm, manifest);
    REQUIRE_FALSE(invalid);
    REQUIRE(invalid.error().Message().contains("memory"));

    compile("int ext_init(void) { return 0; }", R"c(
__attribute__((export_name("ext_alloc"))) unsigned ext_alloc(unsigned len) { return len; }
)c",
        "--export=ext_alloc");
    invalid = woki::ext::wasm::ValidateGuestModule(wasm, manifest);
    REQUIRE_FALSE(invalid);
    REQUIRE(invalid.error().Message().contains("pair"));
}

TEST_CASE("Guest validation rejects start sections and memory outside the raw ABI cap") {
    const fs::path root = fs::temp_directory_path() / "woki_guest_module_tests_memory_policy";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path source = root / "extension.c";
    const fs::path wasm = root / "extension.wasm";
    WriteFile(source, R"c(
char memory_anchor[8];
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) { return 0; }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double d) { (void)d; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned a, unsigned b, unsigned c) { (void)a; (void)b; (void)c; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c");
    REQUIRE(woki_test_compile_wasm(source.string(), wasm.string(), "--export=ext_api_version --export=ext_init --export=ext_on_tick --export=ext_on_event --export=ext_on_unload"));
    woki::ext::Manifest manifest;
    manifest.id = "woki.test";
    REQUIRE(woki::ext::wasm::ValidateGuestModule(wasm, manifest));

    auto bytes = ReadBytes(wasm);
    const std::size_t code = FindSection(bytes, 10);
    bytes.insert(bytes.begin() + static_cast<std::ptrdiff_t>(code), {0x08, 0x01, 0x00});
    WriteBytes(root / "start.wasm", bytes);
    auto invalid = woki::ext::wasm::ValidateGuestModule(root / "start.wasm", manifest);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().Message().contains("start section"));

    bytes = ReadBytes(wasm);
    RemoveMemoryMaximum(bytes);
    WriteBytes(root / "unbounded.wasm", bytes);
    invalid = woki::ext::wasm::ValidateGuestModule(root / "unbounded.wasm", manifest);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().Message().contains("declare a maximum"));

    bytes = ReadBytes(wasm);
    SetMemoryMaximumTo513(bytes);
    WriteBytes(root / "excessive.wasm", bytes);
    invalid = woki::ext::wasm::ValidateGuestModule(root / "excessive.wasm", manifest);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().Message().contains("32 MiB"));
}

TEST_CASE("Guest event and command callbacks require the allocator pair") {
    const fs::path root = fs::temp_directory_path() / "woki_guest_module_tests_command_allocator";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path source = root / "extension.c";
    const fs::path wasm = root / "extension.wasm";
    WriteFile(source, R"c(
char memory_anchor[8];
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) { return 0; }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double d) { (void)d; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned a, unsigned b, unsigned c) { (void)a; (void)b; (void)c; }
__attribute__((export_name("ext_on_command"))) int ext_on_command(unsigned a, unsigned b, unsigned c, unsigned d) { (void)a; (void)b; (void)c; (void)d; return 0; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c");
    REQUIRE(woki_test_compile_wasm(source.string(), wasm.string(), "--export=ext_api_version --export=ext_init --export=ext_on_tick --export=ext_on_event --export=ext_on_command --export=ext_on_unload"));
    woki::ext::Manifest manifest;
    manifest.id = "woki.test";
    manifest.requested_capabilities.permissions.push_back(woki::ext::Permission::Events);
    auto invalid = woki::ext::wasm::ValidateGuestModule(wasm, manifest);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().Message().contains("ext_alloc/ext_free"));

    manifest.requested_capabilities.permissions.clear();
    manifest.commands.push_back({"woki.test.run", "Run", ""});
    invalid = woki::ext::wasm::ValidateGuestModule(wasm, manifest);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().Message().contains("ext_alloc/ext_free"));
}

TEST_CASE("Guest validation rejects unknown and undeclared host imports") {
    const fs::path root = fs::temp_directory_path() / "woki_guest_module_tests_imports";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path source = root / "extension.c";
    const fs::path wasm = root / "extension.wasm";
    WriteFile(source, R"c(
__attribute__((import_module("woki_host"), import_name("host_log"))) extern int host_log(unsigned, const char*, unsigned);
char memory_anchor[8];
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) { return host_log(1, "x", 1); }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double d) { (void)d; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned a, unsigned b, unsigned c) { (void)a; (void)b; (void)c; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c");
    REQUIRE(woki_test_compile_wasm(source.string(), wasm.string(), "--export=ext_api_version --export=ext_init --export=ext_on_tick --export=ext_on_event --export=ext_on_unload"));

    woki::ext::Manifest manifest;
    manifest.id = "woki.test";
    auto invalid = woki::ext::wasm::ValidateGuestModule(wasm, manifest);
    REQUIRE_FALSE(invalid);
    REQUIRE(invalid.error().Message().contains("permission"));

    manifest.requested_capabilities.permissions.push_back(woki::ext::Permission::Log);
    REQUIRE(woki::ext::wasm::ValidateGuestModule(wasm, manifest));
    ReplaceBytes(wasm, "host_log", "host_lag");
    invalid = woki::ext::wasm::ValidateGuestModule(wasm, manifest);
    REQUIRE_FALSE(invalid);
    REQUIRE(invalid.error().Message().contains("Unknown woki_host import"));
}

#if defined(WOKI_EXTENSION_WITH_WASMTIME)
TEST_CASE("Guest validation asks Wasmtime to validate modules with imports") {
    const fs::path root = fs::temp_directory_path() / "woki_guest_module_tests_import_validation";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path source = root / "extension.c";
    const fs::path wasm = root / "extension.wasm";
    WriteFile(source, R"c(
__attribute__((import_module("woki_host"), import_name("host_log"))) extern int host_log(unsigned, const char*, unsigned);
char memory_anchor[8];
__attribute__((export_name("ext_api_version"))) unsigned ext_api_version(void) { return 1; }
__attribute__((export_name("ext_init"))) int ext_init(void) { return host_log(1, "x", 1); }
__attribute__((export_name("ext_on_tick"))) void ext_on_tick(double d) { (void)d; }
__attribute__((export_name("ext_on_event"))) void ext_on_event(unsigned a, unsigned b, unsigned c) { (void)a; (void)b; (void)c; }
__attribute__((export_name("ext_on_unload"))) void ext_on_unload(void) {}
)c");
    REQUIRE(woki_test_compile_wasm(source.string(), wasm.string(), "--export=ext_api_version --export=ext_init --export=ext_on_tick --export=ext_on_event --export=ext_on_unload"));
    auto bytes = ReadBytes(wasm);
    CorruptFirstFunctionInstruction(bytes);
    WriteBytes(wasm, bytes);

    woki::ext::Manifest manifest;
    manifest.id = "woki.test";
    manifest.requested_capabilities.permissions.push_back(woki::ext::Permission::Log);
    const auto invalid = woki::ext::wasm::ValidateGuestModule(wasm, manifest);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().Message().contains("Failed to compile wasm module"));
}
#endif
