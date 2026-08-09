#pragma once

// IWYU pragma: private, include "woki/ext/ext.hpp"

#include <array>
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
