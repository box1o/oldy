#include <cerrno>
#include <cstring>
#include <ostream>
#include <system_error>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;
#endif

#include "cli_internal.hpp"

namespace wokiext {

#ifdef _WIN32
namespace {

[[nodiscard]] std::wstring Utf8ToWide(std::string_view value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), size) != size) {
        return {};
    }
    return result;
}

} // namespace
#endif

bool SystemProcessRunner::Run(std::span<const std::string> arguments, Diagnostics& diagnostics) {
    if (arguments.empty()) {
        diagnostics.Error("Cannot start a process without arguments");
        return false;
    }
#ifdef _WIN32
    std::vector<std::wstring> wide_arguments;
    std::vector<const wchar_t*> argv;
    wide_arguments.reserve(arguments.size());
    argv.reserve(arguments.size() + 1);
    for (const std::string& argument : arguments) {
        wide_arguments.push_back(Utf8ToWide(argument));
        if (wide_arguments.back().empty() && !argument.empty()) {
            diagnostics.Error("Process argument is not valid UTF-8");
            return false;
        }
    }
    for (const std::wstring& argument : wide_arguments)
        argv.push_back(argument.c_str());
    argv.push_back(nullptr);
    const intptr_t status = _wspawnvp(_P_WAIT, argv.front(), argv.data());
    if (status == -1) {
        diagnostics.Err() << "Failed to start " << arguments.front() << ": " << std::error_code(errno, std::generic_category()).message() << '\n';
        return false;
    }
    if (status != 0) {
        diagnostics.Err() << arguments.front() << " exited with code " << status << '\n';
        return false;
    }
#else
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (const std::string& argument : arguments)
        argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);
    pid_t child = -1;
    const int spawn_error = posix_spawnp(&child, argv.front(), nullptr, nullptr, argv.data(), environ);
    if (spawn_error != 0) {
        diagnostics.Err() << "Failed to start " << arguments.front() << ": " << std::strerror(spawn_error) << '\n';
        return false;
    }
    int status = 0;
    while (waitpid(child, &status, 0) == -1) {
        if (errno == EINTR)
            continue;
        diagnostics.Err() << "Failed to wait for " << arguments.front() << ": " << std::strerror(errno) << '\n';
        return false;
    }
    if (WIFSIGNALED(status)) {
        diagnostics.Err() << arguments.front() << " terminated by signal " << WTERMSIG(status) << '\n';
        return false;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        if (WIFEXITED(status))
            diagnostics.Err() << arguments.front() << " exited with code " << WEXITSTATUS(status) << '\n';
        else
            diagnostics.Err() << arguments.front() << " terminated unexpectedly\n";
        return false;
    }
#endif
    return true;
}

} // namespace wokiext
