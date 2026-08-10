#include <cerrno>
#include <cstring>
#include <ostream>
#include <system_error>

#ifdef _WIN32
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

[[nodiscard]] std::wstring QuoteArgument(std::wstring_view argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring_view::npos)
        return std::wstring(argument);

    std::wstring quoted{L'"'};
    std::size_t backslashes = 0;
    for (const wchar_t ch : argument) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(ch);
        } else {
            quoted.append(backslashes, L'\\');
            quoted.push_back(ch);
        }
        backslashes = 0;
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
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
    wide_arguments.reserve(arguments.size());
    for (const std::string& argument : arguments) {
        wide_arguments.push_back(Utf8ToWide(argument));
        if (wide_arguments.back().empty() && !argument.empty()) {
            diagnostics.Error("Process argument is not valid UTF-8");
            return false;
        }
    }
    std::wstring command_line;
    for (const std::wstring& argument : wide_arguments) {
        if (!command_line.empty())
            command_line.push_back(L' ');
        command_line += QuoteArgument(argument);
    }
    STARTUPINFOW startup{};
    startup.cb = static_cast<DWORD>(sizeof(startup));
    PROCESS_INFORMATION process{};
    if (CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process) == 0) {
        diagnostics.Err() << "Failed to start " << arguments.front() << ": " << std::error_code(static_cast<int>(GetLastError()), std::system_category()).message() << '\n';
        return false;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD status = 1;
    const bool got_status = GetExitCodeProcess(process.hProcess, &status) != 0;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (!got_status) {
        diagnostics.Err() << "Failed to read exit status from " << arguments.front() << '\n';
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
