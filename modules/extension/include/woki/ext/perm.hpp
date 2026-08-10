#pragma once

#include <array>
#include <vector>
#include <string_view>

#include <woki/core.hpp>

namespace woki::ext {

enum class Permission : u8 {
    Log,
    Paths,
    Storage,
    Config,
    Events,
};

struct RequestedCapabilities {
    std::vector<Permission> permissions;
};

struct EffectiveCapabilities {
    std::vector<Permission> permissions;
};

[[nodiscard]] bool HasPermission(const RequestedCapabilities& capabilities, Permission permission) noexcept;
[[nodiscard]] bool HasPermission(const EffectiveCapabilities& capabilities, Permission permission) noexcept;

[[nodiscard]] constexpr std::array<Permission, 5> AllPermissions() noexcept {
    return {
        Permission::Log,
        Permission::Paths,
        Permission::Storage,
        Permission::Config,
        Permission::Events,
    };
}

[[nodiscard]] std::string_view ToString(Permission permission) noexcept;
[[nodiscard]] Result<Permission> ParsePermission(std::string_view text);
[[nodiscard]] bool IsKnownPermission(std::string_view text) noexcept;

} // namespace woki::ext
