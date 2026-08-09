#pragma once

// IWYU pragma: private, include "woki/ext/ext.hpp"

#include <string>
#include <vector>
#include <filesystem>
#include <string_view>

#include <woki/core.hpp>

#include "perm.hpp"
#include "command.hpp"

namespace woki::ext {

inline constexpr u32 kApiVersion = 1u; // Must match WOKI_EXT_API_VERSION in sdk/version.h
inline constexpr std::size_t kMaxManifestBytes = 64u * 1024u;

struct Manifest {
    std::string id;
    std::string name;
    std::string version;
    u32 api_version{kApiVersion};
    std::filesystem::path wasm_path{"extension.wasm"};
    std::vector<Permission> permissions;
    std::vector<CommandContribution> commands;
};

[[nodiscard]] Result<Manifest> LoadManifest(const std::filesystem::path& path);
[[nodiscard]] Result<void> ValidateManifest(const Manifest& manifest);
[[nodiscard]] Result<void> ValidateManifestForPackage(const Manifest& manifest, std::string_view package_id);
[[nodiscard]] bool HasPermission(const Manifest& manifest, Permission permission) noexcept;

} // namespace woki::ext
