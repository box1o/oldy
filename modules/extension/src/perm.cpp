#include <array>
#include <string>
#include <algorithm>
#include <string_view>

#include "woki/ext/perm.hpp"

namespace woki::ext {

namespace {

struct PermissionName final {
    Permission permission;
    std::string_view name;
};

constexpr std::array<PermissionName, 5> kPermissionNames{{
    {Permission::Log, "log"},
    {Permission::Paths, "paths"},
    {Permission::Storage, "storage"},
    {Permission::Config, "config"},
    {Permission::Events, "events"},
}};

} // namespace

std::string_view ToString(Permission permission) noexcept {
    const auto it = std::ranges::find(kPermissionNames, permission, &PermissionName::permission);
    if (it == kPermissionNames.end()) {
        return "unknown";
    }
    return it->name;
}

Result<Permission> ParsePermission(std::string_view text) {
    const auto it = std::ranges::find(kPermissionNames, text, &PermissionName::name);
    if (it == kPermissionNames.end()) {
        return Err(ErrorCode::ValidationInvalidState, "Unknown extension permission '" + std::string(text) + "'. Use one of: log, paths, storage, config, events.");
    }
    return Ok(it->permission);
}

bool IsKnownPermission(std::string_view text) noexcept {
    return std::ranges::find(kPermissionNames, text, &PermissionName::name) != kPermissionNames.end();
}

bool HasPermission(const RequestedCapabilities& capabilities, Permission permission) noexcept {
    return std::ranges::find(capabilities.permissions, permission) != capabilities.permissions.end();
}

bool HasPermission(const EffectiveCapabilities& capabilities, Permission permission) noexcept {
    return std::ranges::find(capabilities.permissions, permission) != capabilities.permissions.end();
}

} // namespace woki::ext
