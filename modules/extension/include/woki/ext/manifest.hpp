#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <string_view>

#include <woki/core.hpp>

#include "perm.hpp"
#include "command.hpp"
#include "application_event.hpp"

namespace woki::ext {

inline constexpr u32 kApiVersion = 1u; // Must match WOKI_EXT_API_VERSION in sdk/version.h
inline constexpr std::size_t kMaxManifestBytes = 64u * 1024u;
inline constexpr std::size_t kMaxManifestIdBytes = 255u;
inline constexpr std::size_t kMaxManifestNameBytes = 256u;
inline constexpr std::size_t kMaxManifestVersionBytes = 128u;
inline constexpr std::size_t kMaxRuntimePathBytes = 4096u;
inline constexpr std::size_t kMaxManifestCommands = 256u;
inline constexpr std::size_t kMaxCommandIdBytes = 255u;
inline constexpr std::size_t kMaxCommandTitleBytes = 256u;
inline constexpr std::size_t kMaxCommandCategoryBytes = 128u;
inline constexpr std::size_t kMaxActivationEvents = AllApplicationEventTypes().size();

struct ActivationMetadata {
    bool startup{};
    bool tick{};
    std::vector<ApplicationEventType> events;
};

struct Manifest {
    std::string id;
    std::string name;
    std::string version;
    u32 api_version{kApiVersion};
    std::filesystem::path wasm_path{"extension.wasm"};
    RequestedCapabilities requested_capabilities;
    ActivationMetadata activation;
    std::vector<CommandContribution> commands;
};

[[nodiscard]] Result<Manifest> LoadManifest(const std::filesystem::path& path);
[[nodiscard]] Result<void> ValidateManifest(const Manifest& manifest);
[[nodiscard]] Result<void> ValidateManifestForPackage(const Manifest& manifest, std::string_view package_id);
[[nodiscard]] bool IsValidExtensionId(std::string_view id) noexcept;
[[nodiscard]] bool HasPermission(const Manifest& manifest, Permission permission) noexcept;
[[nodiscard]] bool ActivatesOn(const Manifest& manifest, ApplicationEventType event) noexcept;

} // namespace woki::ext
