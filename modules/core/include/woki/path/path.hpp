#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <filesystem>
#include <string_view>

#include "../error/result.hpp"

namespace woki::paths {

using Path = std::filesystem::path;

// Platform-default locations for a given application name.
// These values are suitable as defaults and can be overridden by higher-level
// app configuration if needed.
struct DefaultAppPaths {
    Path executable;
    Path working;
    Path temporary;
    Path home;
    Path config;
    Path data;
    Path cache;
    Path logs;
};

[[nodiscard]] Result<Path> ExecutablePath();
[[nodiscard]] Result<Path> WorkingDirectory();
[[nodiscard]] Result<Path> TemporaryDirectory();
[[nodiscard]] Result<Path> HomeDirectory();

[[nodiscard]] Result<Path> ConfigDirectory(std::string_view app_name = {});
[[nodiscard]] Result<Path> DataDirectory(std::string_view app_name = {});
[[nodiscard]] Result<Path> CacheDirectory(std::string_view app_name = {});
[[nodiscard]] Result<Path> LogsDirectory(std::string_view app_name = {});

[[nodiscard]] Result<DefaultAppPaths> AppPaths(std::string_view app_name = {});

[[nodiscard]] Path Join(const Path& lhs, const Path& rhs);
[[nodiscard]] Result<Path> Normalize(const Path& path);
[[nodiscard]] Result<void> EnsureDirectory(const Path& path);

} // namespace woki::paths
