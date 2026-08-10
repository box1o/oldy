#pragma once

// Host implementation detail. This header is not installed.

#include <span>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <string_view>

#include "../manifest.hpp"

namespace woki::ext::wasm {

enum class WasmValueType : u8 {
    I32,
    I64,
    F32,
    F64,
};

struct GuestFunctionSignature {
    std::vector<WasmValueType> parameters;
    std::vector<WasmValueType> results;

    [[nodiscard]] bool operator==(const GuestFunctionSignature&) const = default;
};

struct GuestImport {
    std::string module;
    std::string name;
    GuestFunctionSignature signature;
};

struct GuestModuleInfo {
    bool valid_magic{false};
    bool memory{false};
    u32 memory_minimum_pages{0};
    std::optional<u32> memory_maximum_pages;
    bool has_start{false};
    bool ext_api_version{false};
    bool ext_init{false};
    bool ext_on_tick{false};
    bool ext_on_event{false};
    bool ext_on_event_named{false};
    bool ext_on_unload{false};
    bool ext_on_command{false};
    bool ext_alloc{false};
    bool ext_free{false};
    std::vector<GuestImport> imports;
};

[[nodiscard]] Result<void> ValidateWasmMagic(const std::filesystem::path& wasm_path);
[[nodiscard]] Result<std::vector<u8>> LoadGuestModule(const std::filesystem::path& wasm_path);
[[nodiscard]] Result<GuestModuleInfo> InspectGuestModule(std::span<const u8> bytes);
[[nodiscard]] Result<GuestModuleInfo> InspectGuestModule(const std::filesystem::path& wasm_path);
[[nodiscard]] Result<void> ValidateGuestModule(std::span<const u8> bytes, const Manifest& manifest);
[[nodiscard]] Result<void> ValidateGuestModule(const std::filesystem::path& wasm_path, const Manifest& manifest);

} // namespace woki::ext::wasm
