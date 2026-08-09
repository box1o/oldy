#include <limits>
#include <string>
#include <cstdlib>
#include <optional>
#include <filesystem>

#include "woki/path/path.hpp"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <unistd.h>
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <climits>
#include <unistd.h>
#endif

namespace woki::paths {

namespace {

[[nodiscard]] auto AppendAppName(Path base, std::string_view app_name) -> Path {
    if (!app_name.empty()) {
        base /= app_name;
    }
    return base;
}

[[nodiscard]] auto EnvironmentPath(const char* name) -> std::optional<Path> {
#if defined(_MSC_VER)
    char* buffer = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr || *buffer == '\0') {
        if (buffer != nullptr) {
            std::free(buffer);
        }
        return std::nullopt;
    }

    const Path path(buffer);
    std::free(buffer);
    return path;
#else
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return std::nullopt;
    }

    return Path(value);
#endif
}

} // namespace

Result<Path> ExecutablePath() {
#if defined(_WIN32)
    std::string buffer(MAX_PATH, '\0');
    while (buffer.size() <= std::numeric_limits<DWORD>::max()) {
        const DWORD length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return Err(ErrorCode::FileReadError, "Failed to query executable path");
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            return Ok(Path(buffer));
        }
        buffer.resize(buffer.size() * 2);
    }
    return Err(ErrorCode::FileReadError, "Executable path is too long");
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        return Err(ErrorCode::FileReadError, "Failed to query executable path");
    }
    return Ok(Path(buffer).lexically_normal());
#elif defined(__linux__)
    std::string buffer(PATH_MAX, '\0');
    while (true) {
        const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (length < 0) {
            return Err(ErrorCode::FileReadError, "Failed to query executable path");
        }
        if (static_cast<std::size_t>(length) < buffer.size()) {
            buffer.resize(static_cast<std::size_t>(length));
            return Ok(Path(buffer));
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    return Err(ErrorCode::InvalidState, "ExecutablePath is unsupported on this platform");
#endif
}

Result<Path> WorkingDirectory() {
    try {
        return Ok(std::filesystem::current_path());
    } catch (const std::filesystem::filesystem_error& exception) {
        return Err(ErrorCode::FileReadError, exception.what());
    }
}

Result<Path> TemporaryDirectory() {
    try {
        return Ok(std::filesystem::temp_directory_path());
    } catch (const std::filesystem::filesystem_error& exception) {
        return Err(ErrorCode::FileReadError, exception.what());
    }
}

Result<Path> HomeDirectory() {
#if defined(_WIN32)
    if (auto home = EnvironmentPath("USERPROFILE")) {
        return Ok(*home);
    }

    if (auto drive = EnvironmentPath("HOMEDRIVE")) {
        if (auto path = EnvironmentPath("HOMEPATH")) {
            return Ok(Path(drive->string() + path->string()));
        }
    }
#else
    if (auto home = EnvironmentPath("HOME")) {
        return Ok(*home);
    }
#endif

    return Err(ErrorCode::FileNotFound, "Home directory is unavailable");
}

Result<Path> ConfigDirectory(std::string_view app_name) {
#if defined(_WIN32)
    if (auto app_data = EnvironmentPath("APPDATA")) {
        return Ok(AppendAppName(*app_data, app_name));
    }
#elif defined(__APPLE__)
    auto home = HomeDirectory();
    if (!home) {
        return Err(home.error());
    }
    return Ok(AppendAppName(*home / "Library" / "Preferences", app_name));
#else
    if (auto xdg = EnvironmentPath("XDG_CONFIG_HOME")) {
        return Ok(AppendAppName(*xdg, app_name));
    }

    auto home = HomeDirectory();
    if (!home) {
        return Err(home.error());
    }
    return Ok(AppendAppName(*home / ".config", app_name));
#endif

    return Err(ErrorCode::FileNotFound, "Config directory is unavailable");
}

Result<Path> DataDirectory(std::string_view app_name) {
#if defined(_WIN32)
    if (auto app_data = EnvironmentPath("APPDATA")) {
        return Ok(AppendAppName(*app_data, app_name));
    }
#elif defined(__APPLE__)
    auto home = HomeDirectory();
    if (!home) {
        return Err(home.error());
    }
    return Ok(AppendAppName(*home / "Library" / "Application Support", app_name));
#else
    if (auto xdg = EnvironmentPath("XDG_DATA_HOME")) {
        return Ok(AppendAppName(*xdg, app_name));
    }

    auto home = HomeDirectory();
    if (!home) {
        return Err(home.error());
    }
    return Ok(AppendAppName(*home / ".local" / "share", app_name));
#endif

    return Err(ErrorCode::FileNotFound, "Data directory is unavailable");
}

Result<Path> CacheDirectory(std::string_view app_name) {
#if defined(_WIN32)
    if (auto local_app_data = EnvironmentPath("LOCALAPPDATA")) {
        return Ok(AppendAppName(*local_app_data, app_name));
    }
#elif defined(__APPLE__)
    auto home = HomeDirectory();
    if (!home) {
        return Err(home.error());
    }
    return Ok(AppendAppName(*home / "Library" / "Caches", app_name));
#else
    if (auto xdg = EnvironmentPath("XDG_CACHE_HOME")) {
        return Ok(AppendAppName(*xdg, app_name));
    }

    auto home = HomeDirectory();
    if (!home) {
        return Err(home.error());
    }
    return Ok(AppendAppName(*home / ".cache", app_name));
#endif

    return Err(ErrorCode::FileNotFound, "Cache directory is unavailable");
}

Result<Path> LogsDirectory(std::string_view app_name) {
    auto cache = CacheDirectory(app_name);
    if (!cache) {
        return Err(cache.error());
    }

    return Ok(*cache / "logs");
}

Result<DefaultAppPaths> AppPaths(std::string_view app_name) {
    auto executable = ExecutablePath();
    if (!executable) {
        return Err(executable.error());
    }

    auto working = WorkingDirectory();
    if (!working) {
        return Err(working.error());
    }

    auto temporary = TemporaryDirectory();
    if (!temporary) {
        return Err(temporary.error());
    }

    auto home = HomeDirectory();
    if (!home) {
        return Err(home.error());
    }

    auto config = ConfigDirectory(app_name);
    if (!config) {
        return Err(config.error());
    }

    auto data = DataDirectory(app_name);
    if (!data) {
        return Err(data.error());
    }

    auto cache = CacheDirectory(app_name);
    if (!cache) {
        return Err(cache.error());
    }

    auto logs = LogsDirectory(app_name);
    if (!logs) {
        return Err(logs.error());
    }

    return Ok(DefaultAppPaths{
        .executable = *executable,
        .working = *working,
        .temporary = *temporary,
        .home = *home,
        .config = *config,
        .data = *data,
        .cache = *cache,
        .logs = *logs,
    });
}

Path Join(const Path& lhs, const Path& rhs) {
    return lhs / rhs;
}

Result<Path> Normalize(const Path& path) {
    try {
        return Ok(std::filesystem::absolute(path).lexically_normal());
    } catch (const std::filesystem::filesystem_error& exception) {
        return Err(ErrorCode::FileReadError, exception.what());
    }
}

Result<void> EnsureDirectory(const Path& path) {
    try {
        if (!std::filesystem::create_directories(path) && !std::filesystem::is_directory(path)) {
            return Err(ErrorCode::FileWriteError, "Path exists and is not a directory");
        }
        return Ok();
    } catch (const std::filesystem::filesystem_error& exception) {
        return Err(ErrorCode::FileWriteError, exception.what());
    }
}

} // namespace woki::paths
