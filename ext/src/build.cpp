#include <cerrno>
#include <cstdio>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <string_view>

#include "wokiext/cli.hpp"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

#include "tool_paths.hpp"

namespace wokiext {

namespace {

[[nodiscard]] bool RunProcess(std::vector<std::string> arguments) {
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (std::string& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

#ifdef _WIN32
    const intptr_t status = _spawnvp(_P_WAIT, argv.front(), argv.data());
    if (status == -1) {
        std::cerr << "Failed to start " << arguments.front() << ": " << std::strerror(errno) << '\n';
        return false;
    }
    if (status != 0) {
        std::cerr << arguments.front() << " exited with code " << status << '\n';
        return false;
    }
#else
    const pid_t child = fork();
    if (child == -1) {
        std::cerr << "Failed to start " << arguments.front() << ": " << std::strerror(errno) << '\n';
        return false;
    }
    if (child == 0) {
        execvp(argv.front(), argv.data());
        std::fprintf(stderr, "Failed to execute %s: %s\n", argv.front(), std::strerror(errno));
        _exit(127);
    }

    int status = 0;
    while (waitpid(child, &status, 0) == -1) {
        if (errno == EINTR) {
            continue;
        }
        std::cerr << "Failed to wait for " << arguments.front() << ": " << std::strerror(errno) << '\n';
        return false;
    }
    if (WIFSIGNALED(status)) {
        std::cerr << arguments.front() << " terminated by signal " << WTERMSIG(status) << '\n';
        return false;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        if (WIFEXITED(status)) {
            std::cerr << arguments.front() << " exited with code " << WEXITSTATUS(status) << '\n';
        } else {
            std::cerr << arguments.front() << " terminated unexpectedly\n";
        }
        return false;
    }
#endif

    return true;
}

[[nodiscard]] std::filesystem::path ExecutablePath(const std::filesystem::path& executable) {
    if (executable.has_parent_path()) {
        return std::filesystem::absolute(executable).lexically_normal();
    }

    const char* path_environment = std::getenv("PATH");
    if (path_environment == nullptr) {
        return {};
    }
#ifdef _WIN32
    constexpr char kPathSeparator = ';';
#else
    constexpr char kPathSeparator = ':';
#endif
    std::string_view paths{path_environment};
    while (!paths.empty()) {
        const std::size_t separator = paths.find(kPathSeparator);
        const std::filesystem::path candidate = std::filesystem::path(paths.substr(0, separator)) / executable;
        if (std::filesystem::is_regular_file(candidate)) {
            return std::filesystem::absolute(candidate).lexically_normal();
        }
        if (separator == std::string_view::npos) {
            break;
        }
        paths.remove_prefix(separator + 1);
    }
    return {};
}

[[nodiscard]] std::filesystem::path CMakeModuleDir(const std::filesystem::path& executable) {
    if (const char* configured = std::getenv("WOKI_CMAKE_DIR"); configured != nullptr) {
        const std::filesystem::path candidate{configured};
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

[[nodiscard]] std::filesystem::path SdkDir(const std::filesystem::path& executable) {
    if (const char* configured = std::getenv("WOKI_SDK_DIR"); configured != nullptr) {
        const std::filesystem::path candidate{configured};
        if (std::filesystem::is_regular_file(candidate / "ext.h")) {
            return std::filesystem::absolute(candidate).lexically_normal();
        }
    }

    const std::filesystem::path executable_path = ExecutablePath(executable);
    if (!executable_path.empty()) {
        const std::filesystem::path installed = (executable_path.parent_path() / kInstallSdkDirFromBin).lexically_normal();
        if (std::filesystem::is_regular_file(installed / "ext.h")) {
            return installed;
        }
    }

    const std::filesystem::path source{kSourceSdkDir};
    if (std::filesystem::is_regular_file(source / "ext.h")) {
        return source.lexically_normal();
    }
    return {};
}

} // namespace

Status Build(const BuildOptions& options) {
    const std::filesystem::path root = std::filesystem::absolute(options.path).lexically_normal();
    const std::filesystem::path build_dir = root / "build";

    if (!std::filesystem::is_regular_file(root / "CMakeLists.txt")) {
        std::cerr << "Extension project is missing CMakeLists.txt: " << root << '\n';
        return Status::Error;
    }

    const std::filesystem::path cmake_module_dir = CMakeModuleDir(options.executable);
    if (cmake_module_dir.empty()) {
        std::cerr << "Cannot locate Woki extension CMake modules; set WOKI_CMAKE_DIR\n";
        return Status::Error;
    }
    const std::filesystem::path sdk_dir = SdkDir(options.executable);
    if (sdk_dir.empty()) {
        std::cerr << "Cannot locate the Woki extension SDK; set WOKI_SDK_DIR\n";
        return Status::Error;
    }

    if (!RunProcess({"cmake", "-B", build_dir.string(), "-S", root.string(), "-DCMAKE_BUILD_TYPE=" + options.config, "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DWOKI_CMAKE_DIR=" + cmake_module_dir.string(),
            "-DWOKI_SDK_DIR=" + sdk_dir.string()})) {
        return Status::Error;
    }

    const std::filesystem::path compile_commands = build_dir / "compile_commands.json";
    if (std::filesystem::is_regular_file(compile_commands)) {
        std::error_code copy_error;
        std::filesystem::copy_file(compile_commands, root / "compile_commands.json", std::filesystem::copy_options::overwrite_existing, copy_error);
        if (copy_error) {
            std::cerr << "Warning: failed to export compile_commands.json: " << copy_error.message() << '\n';
        }
    }

    if (!RunProcess({"cmake", "--build", build_dir.string()})) {
        return Status::Error;
    }

    return Status::Ok;
}

} // namespace wokiext
