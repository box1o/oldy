#include <string>
#include <vector>
#include <cstdlib>
#include <optional>
#include <filesystem>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <unistd.h>
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

#include "tool_paths.hpp"
#include "cli_internal.hpp"

namespace wokiext {

namespace {

[[nodiscard]] std::string PathArgument(const std::filesystem::path& path) {
#ifdef _WIN32
    const std::u8string value = path.generic_u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
#else
    return path.string();
#endif
}

[[nodiscard, maybe_unused]] std::optional<std::string> Environment(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
        return std::nullopt;
    }
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
#endif
}

[[nodiscard]] std::optional<std::filesystem::path> EnvironmentPath(const char* name) {
#ifdef _WIN32
    std::wstring wide_name;
    while (*name != '\0')
        wide_name.push_back(static_cast<wchar_t>(*name++));
    wchar_t* value = nullptr;
    std::size_t size = 0;
    if (_wdupenv_s(&value, &size, wide_name.c_str()) != 0 || value == nullptr) {
        return std::nullopt;
    }
    std::filesystem::path result(value);
    std::free(value);
    return result;
#else
    if (const auto value = Environment(name))
        return std::filesystem::path(*value);
    return std::nullopt;
#endif
}

[[nodiscard]] bool IsExecutable(const std::filesystem::path& path) {
#ifdef _WIN32
    return std::filesystem::is_regular_file(path);
#else
    return std::filesystem::is_regular_file(path) && ::access(path.c_str(), X_OK) == 0;
#endif
}

[[nodiscard]] std::filesystem::path RunningExecutablePath() {
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length != 0 && length < buffer.size()) {
        buffer.resize(length);
        return std::filesystem::path(buffer).lexically_normal();
    }
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
        std::error_code error;
        const auto path = std::filesystem::canonical(buffer.data(), error);
        if (!error)
            return path;
    }
#elif defined(__linux__)
    std::vector<char> buffer(4096);
    while (buffer.size() <= 1024 * 1024) {
        const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (length < 0)
            break;
        if (static_cast<std::size_t>(length) < buffer.size())
            return std::filesystem::path(std::string(buffer.data(), static_cast<std::size_t>(length))).lexically_normal();
        buffer.resize(buffer.size() * 2);
    }
#endif
    return {};
}

[[nodiscard]] std::filesystem::path ExecutablePath(const std::filesystem::path& executable) {
    if (const auto running = RunningExecutablePath(); !running.empty() && IsExecutable(running))
        return running;
    if (executable.has_parent_path()) {
        const auto path = std::filesystem::absolute(executable).lexically_normal();
        return IsExecutable(path) ? path : std::filesystem::path{};
    }

#ifdef _WIN32
    const auto search = [&executable](const wchar_t* extension) -> std::filesystem::path {
        std::wstring buffer(32768, L'\0');
        const DWORD length = SearchPathW(nullptr, executable.c_str(), extension, static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
        if (length == 0 || length >= buffer.size()) {
            return {};
        }
        buffer.resize(length);
        return std::filesystem::path(buffer).lexically_normal();
    };
    if (executable.has_extension()) {
        return search(nullptr);
    }

    std::wstring path_extensions;
    if (const auto configured = EnvironmentPath("PATHEXT")) {
        path_extensions = configured->native();
    }
    if (path_extensions.empty()) {
        path_extensions = L".COM;.EXE;.BAT;.CMD";
    }
    std::wstring_view remaining{path_extensions};
    while (true) {
        const std::size_t separator = remaining.find(L';');
        const std::wstring extension(remaining.substr(0, separator));
        if (!extension.empty()) {
            if (std::filesystem::path found = search(extension.c_str()); !found.empty()) {
                return found;
            }
        }
        if (separator == std::wstring_view::npos) {
            break;
        }
        remaining.remove_prefix(separator + 1);
    }
    return search(nullptr);
#else
    const auto path_environment = Environment("PATH");
    if (!path_environment) {
        return {};
    }
    constexpr char kPathSeparator = ':';
    std::string_view paths{*path_environment};
    while (true) {
        const std::size_t separator = paths.find(kPathSeparator);
        const std::filesystem::path candidate = std::filesystem::path(paths.substr(0, separator)) / executable;
        if (IsExecutable(candidate)) {
            return std::filesystem::absolute(candidate).lexically_normal();
        }
        if (separator == std::string_view::npos) {
            break;
        }
        paths.remove_prefix(separator + 1);
    }
#endif
    return {};
}

} // namespace

std::filesystem::path FindCMakeModuleDir(const std::filesystem::path& executable) {
    if (const auto configured = EnvironmentPath("WOKI_CMAKE_DIR")) {
        const std::filesystem::path candidate{*configured};
        if (std::filesystem::is_regular_file(candidate / "ExtensionProject.cmake") && std::filesystem::is_regular_file(candidate / "ExtensionWasm.cmake")) {
            return std::filesystem::absolute(candidate).lexically_normal();
        }
    }

    const std::filesystem::path executable_path = ExecutablePath(executable);
    if (!executable_path.empty()) {
        const std::filesystem::path installed = (executable_path.parent_path() / kInstallCMakeDirFromBin).lexically_normal();
        if (std::filesystem::is_regular_file(installed / "ExtensionProject.cmake") && std::filesystem::is_regular_file(installed / "ExtensionWasm.cmake")) {
            return installed;
        }
    }

    const std::filesystem::path source{kSourceCMakeDir};
    if (std::filesystem::is_regular_file(source / "ExtensionProject.cmake") && std::filesystem::is_regular_file(source / "ExtensionWasm.cmake")) {
        return source.lexically_normal();
    }
    return {};
}

std::filesystem::path FindSdkDir(const std::filesystem::path& executable) {
    if (const auto configured = EnvironmentPath("WOKI_SDK_DIR")) {
        const std::filesystem::path candidate{*configured};
        if (std::filesystem::is_regular_file(candidate / "woki/ext/sdk/ext.h")) {
            return std::filesystem::absolute(candidate).lexically_normal();
        }
    }

    const std::filesystem::path executable_path = ExecutablePath(executable);
    if (!executable_path.empty()) {
        const std::filesystem::path installed = (executable_path.parent_path() / kInstallSdkDirFromBin).lexically_normal();
        if (std::filesystem::is_regular_file(installed / "woki/ext/sdk/ext.h")) {
            return installed;
        }
    }

    const std::filesystem::path source{kSourceSdkDir};
    if (std::filesystem::is_regular_file(source / "woki/ext/sdk/ext.h")) {
        return source.lexically_normal();
    }
    return {};
}

std::vector<std::string> BuildConfigureArguments(const BuildOptions& options, const std::filesystem::path& module_dir, const std::filesystem::path& sdk_dir) {
    const auto root = std::filesystem::absolute(options.path).lexically_normal();
    const auto build_dir = root / "build";
    return {"cmake", "-G", "Ninja", "-B", PathArgument(build_dir), "-S", PathArgument(root), "-DCMAKE_BUILD_TYPE=" + options.config, "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DWOKI_CMAKE_DIR=" + PathArgument(module_dir),
        "-DWOKI_SDK_DIR=" + PathArgument(sdk_dir), "-DWOKI_EXTENSION_PACKAGE_DIR=" + PathArgument(build_dir / "packages")};
}

std::vector<std::string> BuildCompileArguments(const BuildOptions& options) {
    const auto root = std::filesystem::absolute(options.path).lexically_normal();
    return {"cmake", "--build", PathArgument(root / "build"), "--config", options.config};
}

std::filesystem::path BuiltPackagePath(const BuildOptions& options) {
    return std::filesystem::absolute(options.path).lexically_normal() / "build/package";
}

Status Build(Context& context, const BuildOptions& options) {
    const std::filesystem::path root = context.filesystem.Absolute(options.path);
    const std::filesystem::path build_dir = root / "build";
    BuildOptions resolved_options = options;
    resolved_options.path = root;

    const std::filesystem::path config_path{options.config};
    if (options.config.empty() || config_path.has_root_path() || config_path.has_parent_path() || config_path == "." || config_path == "..") {
        context.diagnostics.Error("Configuration name must be a single path component");
        return Status::Usage;
    }

    if (!context.filesystem.IsRegularFile(root / "CMakeLists.txt")) {
        context.diagnostics.Err() << "Extension project is missing CMakeLists.txt: " << root << '\n';
        return Status::Error;
    }

    std::error_code state_error;
    context.filesystem.Remove(build_dir / "package.state", state_error);
    if (state_error) {
        context.diagnostics.Error("Cannot invalidate the previously staged package: " + state_error.message());
        return Status::Error;
    }

    const std::filesystem::path cmake_module_dir = FindCMakeModuleDir(options.executable);
    if (cmake_module_dir.empty()) {
        context.diagnostics.Error("Cannot locate Woki extension CMake modules; set WOKI_CMAKE_DIR");
        return Status::Error;
    }
    const std::filesystem::path sdk_dir = FindSdkDir(options.executable);
    if (sdk_dir.empty()) {
        context.diagnostics.Error("Cannot locate the Woki extension SDK; set WOKI_SDK_DIR");
        return Status::Error;
    }

    const auto configure_arguments = BuildConfigureArguments(resolved_options, cmake_module_dir, sdk_dir);
    if (!context.processes.Run(configure_arguments, context.diagnostics)) {
        return Status::Error;
    }

    const std::filesystem::path compile_commands = build_dir / "compile_commands.json";
    if (context.filesystem.IsRegularFile(compile_commands)) {
        std::error_code copy_error;
        context.filesystem.CopyFile(compile_commands, root / "compile_commands.json", copy_error);
        if (copy_error) {
            context.diagnostics.Warning("failed to export compile_commands.json: " + copy_error.message());
        }
    }

    const auto build_arguments = BuildCompileArguments(resolved_options);
    if (!context.processes.Run(build_arguments, context.diagnostics)) {
        return Status::Error;
    }

    const std::filesystem::path configured_package = build_dir / "packages" / options.config;
    const std::vector<std::string> stage_arguments{
        "cmake",
        "-D",
        "SOURCE_DIR=" + PathArgument(configured_package),
        "-D",
        "DESTINATION_DIR=" + PathArgument(BuiltPackagePath(resolved_options)),
        "-D",
        "SOURCE_MANIFEST=" + PathArgument(root / "manifest.yaml"),
        "-D",
        "PROJECT_DIR=" + PathArgument(root),
        "-D",
        "SDK_DIR=" + PathArgument(sdk_dir),
        "-D",
        "CONFIG=" + options.config,
        "-P",
        PathArgument(cmake_module_dir / "StageExtensionPackage.cmake"),
    };
    if (!context.processes.Run(stage_arguments, context.diagnostics)) {
        return Status::Error;
    }

    return Verify(context, PathOptions{.path = BuiltPackagePath(resolved_options)});
}

} // namespace wokiext
