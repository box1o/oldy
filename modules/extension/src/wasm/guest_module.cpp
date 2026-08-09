#include <array>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

#include "woki/ext/wasm/guest_module.hpp"

#if defined(WOKI_EXTENSION_WITH_WASMTIME)
#include <wasmtime.hh>
#endif

namespace woki::ext::wasm {

namespace {

namespace fs = std::filesystem;

#if defined(WOKI_EXTENSION_WITH_WASMTIME)

[[nodiscard]] Result<std::vector<u8>> ReadWasmBytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.good()) {
        return Err(ErrorCode::FileReadError, "Failed to open wasm module: " + path.string());
    }

    const std::vector<u8> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (bytes.empty()) {
        return Err(ErrorCode::ParseInvalidFormat, "Wasm module is empty: " + path.string());
    }
    return Ok(bytes);
}

void MarkExport(GuestModuleInfo& info, std::string_view name) {
    if (name == "ext_api_version") {
        info.ext_api_version = true;
    } else if (name == "ext_init") {
        info.ext_init = true;
    } else if (name == "ext_on_tick") {
        info.ext_on_tick = true;
    } else if (name == "ext_on_event") {
        info.ext_on_event = true;
    } else if (name == "ext_on_unload") {
        info.ext_on_unload = true;
    } else if (name == "ext_on_command") {
        info.ext_on_command = true;
    } else if (name == "ext_alloc") {
        info.ext_alloc = true;
    } else if (name == "ext_free") {
        info.ext_free = true;
    }
}

#endif // WOKI_EXTENSION_WITH_WASMTIME

} // namespace

Result<void> ValidateWasmMagic(const fs::path& wasm_path) {
    std::ifstream input(wasm_path, std::ios::binary);
    if (!input.good()) {
        return Err(ErrorCode::FileReadError, "Failed to open wasm module: " + wasm_path.string());
    }

    std::array<unsigned char, 4> magic{};
    input.read(reinterpret_cast<char*>(magic.data()), static_cast<std::streamsize>(magic.size()));
    if (input.gcount() != static_cast<std::streamsize>(magic.size())) {
        return Err(ErrorCode::ParseInvalidFormat, "Wasm module is too small: " + wasm_path.string());
    }

    constexpr std::array<unsigned char, 4> kWasmMagic{0x00, 0x61, 0x73, 0x6d};
    if (magic != kWasmMagic) {
        return Err(ErrorCode::ParseInvalidFormat, "Wasm module has invalid magic header. Expected '\\0asm': " + wasm_path.string());
    }

    return Ok();
}

Result<GuestModuleInfo> InspectGuestModule(const fs::path& wasm_path) {
    GuestModuleInfo info;
    auto magic = ValidateWasmMagic(wasm_path);
    if (!magic) {
        return Err(magic.error());
    }
    info.valid_magic = true;

#if defined(WOKI_EXTENSION_WITH_WASMTIME)
    auto bytes = ReadWasmBytes(wasm_path);
    if (!bytes) {
        return Err(bytes.error());
    }

    wasmtime::Engine engine;
    auto module = wasmtime::Module::compile(engine, wasmtime::Span<uint8_t>{bytes->data(), bytes->size()});
    if (!module) {
        return Err(ErrorCode::ParseInvalidFormat, "Failed to compile wasm module: " + module.err_ref().message());
    }

    for (const wasmtime::ExportType::Ref& export_type : module.ok_ref().exports()) {
        MarkExport(info, export_type.name());
    }
#endif

    return Ok(info);
}

Result<void> ValidateGuestModule(const fs::path& wasm_path, const Manifest& manifest) {
    auto info = InspectGuestModule(wasm_path);
    if (!info) {
        return Err(info.error());
    }

#if defined(WOKI_EXTENSION_WITH_WASMTIME)
    if (!info->ext_api_version) {
        return Err(ErrorCode::ValidationInvalidState, "Wasm module is missing export ext_api_version.");
    }
    if (!info->ext_init) {
        return Err(ErrorCode::ValidationInvalidState, "Wasm module is missing export ext_init.");
    }
    if (!info->ext_on_tick) {
        return Err(ErrorCode::ValidationInvalidState, "Wasm module is missing export ext_on_tick.");
    }
    if (!info->ext_on_event) {
        return Err(ErrorCode::ValidationInvalidState, "Wasm module is missing export ext_on_event.");
    }
    if (!info->ext_on_unload) {
        return Err(ErrorCode::ValidationInvalidState, "Wasm module is missing export ext_on_unload.");
    }
    if (!manifest.commands.empty() && !info->ext_on_command) {
        return Err(ErrorCode::ValidationInvalidState, "Manifest contributes commands but wasm does not export ext_on_command.");
    }
#else
    (void)manifest;
#endif

    return Ok();
}

} // namespace woki::ext::wasm
