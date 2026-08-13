#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <string>

#include "../build_config.hpp"

namespace woki {

enum class OperatingSystem : u8 {
    Unknown = 0,
    Windows,
    MacOs,
    Linux,
    Android,
    Ios,
    Web,
};

enum class Architecture : u8 {
    Unknown = 0,
    X86,
    X64,
    Arm32,
    Arm64,
    Wasm32,
};

enum class CompilerId : u8 {
    Unknown = 0,
    Clang,
    Gcc,
    Msvc,
    Emscripten,
};

[[nodiscard]] constexpr const char* ToString(OperatingSystem os) noexcept {
    switch (os) {
        case OperatingSystem::Unknown:
            return "Unknown";
        case OperatingSystem::Windows:
            return "Windows";
        case OperatingSystem::MacOs:
            return "MacOS";
        case OperatingSystem::Linux:
            return "Linux";
        case OperatingSystem::Android:
            return "Android";
        case OperatingSystem::Ios:
            return "iOS";
        case OperatingSystem::Web:
            return "Web";
    }

    return "Unknown";
}

[[nodiscard]] constexpr const char* ToString(Architecture architecture) noexcept {
    switch (architecture) {
        case Architecture::Unknown:
            return "Unknown";
        case Architecture::X86:
            return "x86";
        case Architecture::X64:
            return "x64";
        case Architecture::Arm32:
            return "arm32";
        case Architecture::Arm64:
            return "arm64";
        case Architecture::Wasm32:
            return "wasm32";
    }

    return "Unknown";
}

[[nodiscard]] constexpr const char* ToString(CompilerId compiler) noexcept {
    switch (compiler) {
        case CompilerId::Unknown:
            return "Unknown";
        case CompilerId::Clang:
            return "Clang";
        case CompilerId::Gcc:
            return "GCC";
        case CompilerId::Msvc:
            return "MSVC";
        case CompilerId::Emscripten:
            return "Emscripten";
    }

    return "Unknown";
}

[[nodiscard]] constexpr OperatingSystem CurrentOperatingSystem() noexcept {
#if defined(__EMSCRIPTEN__)
    return OperatingSystem::Web;
#elif defined(_WIN32)
    return OperatingSystem::Windows;
#elif defined(__ANDROID__)
    return OperatingSystem::Android;
#elif defined(__APPLE__)
    return OperatingSystem::MacOs;
#elif defined(__linux__)
    return OperatingSystem::Linux;
#else
    return OperatingSystem::Unknown;
#endif
}

[[nodiscard]] constexpr Architecture CurrentArchitecture() noexcept {
#if defined(__EMSCRIPTEN__)
    return Architecture::Wasm32;
#elif defined(__x86_64__) || defined(_M_X64)
    return Architecture::X64;
#elif defined(__i386__) || defined(_M_IX86)
    return Architecture::X86;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return Architecture::Arm64;
#elif defined(__arm__) || defined(_M_ARM)
    return Architecture::Arm32;
#else
    return Architecture::Unknown;
#endif
}

[[nodiscard]] constexpr CompilerId CurrentCompiler() noexcept {
#if defined(__EMSCRIPTEN__)
    return CompilerId::Emscripten;
#elif defined(__clang__)
    return CompilerId::Clang;
#elif defined(__GNUC__)
    return CompilerId::Gcc;
#elif defined(_MSC_VER)
    return CompilerId::Msvc;
#else
    return CompilerId::Unknown;
#endif
}

struct PlatformId {
    OperatingSystem os = CurrentOperatingSystem();
    Architecture architecture = CurrentArchitecture();
    CompilerId compiler = CurrentCompiler();
    BuildMode build_mode = BuildConfig::Mode();

    [[nodiscard]] std::string ToString() const {
        return std::string(::woki::ToString(os)) + "-" + ::woki::ToString(architecture) + "-" + ::woki::ToString(compiler) + "-" + ::woki::ToString(build_mode);
    }
};

[[nodiscard]] constexpr PlatformId CurrentPlatformId() noexcept {
    return PlatformId{};
}

[[nodiscard]] constexpr bool IsWebPlatform() noexcept {
    return CurrentOperatingSystem() == OperatingSystem::Web;
}

[[nodiscard]] constexpr bool IsWindowsPlatform() noexcept {
    return CurrentOperatingSystem() == OperatingSystem::Windows;
}

[[nodiscard]] constexpr bool IsApplePlatform() noexcept {
    return CurrentOperatingSystem() == OperatingSystem::MacOs || CurrentOperatingSystem() == OperatingSystem::Ios;
}

[[nodiscard]] constexpr bool IsDesktopPlatform() noexcept {
    return CurrentOperatingSystem() == OperatingSystem::Windows || CurrentOperatingSystem() == OperatingSystem::MacOs || CurrentOperatingSystem() == OperatingSystem::Linux;
}

} // namespace woki
