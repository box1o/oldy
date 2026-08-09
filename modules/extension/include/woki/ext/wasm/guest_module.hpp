#pragma once

// IWYU pragma: private, include "woki/ext/ext.hpp"

#include <string>
#include <filesystem>

#include "../manifest.hpp"

namespace woki::ext::wasm {

struct GuestModuleInfo {
    bool valid_magic{false};
    bool ext_api_version{false};
    bool ext_init{false};
    bool ext_on_tick{false};
    bool ext_on_event{false};
    bool ext_on_unload{false};
    bool ext_on_command{false};
    bool ext_alloc{false};
    bool ext_free{false};
};

[[nodiscard]] Result<void> ValidateWasmMagic(const std::filesystem::path& wasm_path);
[[nodiscard]] Result<GuestModuleInfo> InspectGuestModule(const std::filesystem::path& wasm_path);
[[nodiscard]] Result<void> ValidateGuestModule(const std::filesystem::path& wasm_path, const Manifest& manifest);

} // namespace woki::ext::wasm
