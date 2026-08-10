#include "wokiext/cli.hpp"

#ifdef _WIN32
#include <string>
#include <vector>
#include <windows.h>

int wmain(int argc, const wchar_t* const* argv) {
    std::vector<std::string> utf8;
    std::vector<const char*> arguments;
    utf8.reserve(static_cast<std::size_t>(argc));
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0)
            return 1;
        std::string value(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1, value.data(), size, nullptr, nullptr);
        value.resize(static_cast<std::size_t>(size - 1));
        utf8.push_back(std::move(value));
    }
    for (const std::string& value : utf8)
        arguments.push_back(value.c_str());
    return wokiext::Run(arguments);
}
#else
int main(int argc, const char* const* argv) {
    return wokiext::Run(std::span<const char* const>(argv, static_cast<std::size_t>(argc)));
}
#endif
