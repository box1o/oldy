#pragma once

#include <string>
#include <cstdlib>
#include <filesystem>
#include <string_view>

// Wasm integration tests compile tiny guest modules at runtime. Use LLVM clang
// (not AppleClang / MSVC) via WOKI_WASM_CLANG when the default is unsuitable.
inline std::string woki_test_wasm_clang() {
#ifdef WOKI_TEST_WASM_CLANG
    return WOKI_TEST_WASM_CLANG;
#endif
    if (const char* from_env = std::getenv("WOKI_WASM_CLANG")) {
        return from_env;
    }
    return "clang";
}

inline std::string woki_test_wasm_clang_dir() {
    const std::string clang = woki_test_wasm_clang();
    if (const auto slash = clang.rfind('/'); slash != std::string::npos) {
        return clang.substr(0, slash);
    }
    if (const auto slash = clang.rfind('\\'); slash != std::string::npos) {
        return clang.substr(0, slash);
    }
    return {};
}

inline std::string woki_test_wasm_llvm_lib_dir() {
#ifdef WOKI_TEST_LLVM_PREFIX
    return std::string(WOKI_TEST_LLVM_PREFIX) + "/lib";
#endif
    if (const char* from_env = std::getenv("WOKI_LLVM_PREFIX")) {
        return std::string(from_env) + "/lib";
    }
    const std::string dir = woki_test_wasm_clang_dir();
    if (dir.empty()) {
        return {};
    }
    return dir + "/../lib";
}

inline std::string woki_test_wasm_toolchain_flags() {
    const std::string dir = woki_test_wasm_clang_dir();
    if (dir.empty()) {
        return {};
    }
#if defined(_WIN32)
    return " -B\"" + dir + "\"";
#else
    return " -B" + dir;
#endif
}

inline std::string woki_test_wasm_compile_prefix() {
    const std::string bin_dir = woki_test_wasm_clang_dir();
    if (bin_dir.empty()) {
        return {};
    }
#if defined(_WIN32)
    return "set \"PATH=" + bin_dir + ";%PATH%\"&& ";
#else
    std::string prefix;
    const std::string lib_dir = woki_test_wasm_llvm_lib_dir();
    if (!lib_dir.empty()) {
        prefix += "DYLD_LIBRARY_PATH=\"" + lib_dir + ":$DYLD_LIBRARY_PATH\" ";
    }
    prefix += "PATH=\"" + bin_dir + ":$PATH\" ";
    return prefix;
#endif
}

inline std::string woki_test_wasm_ld_path() {
#ifdef WOKI_TEST_WASM_LD
    return WOKI_TEST_WASM_LD;
#endif
    if (const char* from_env = std::getenv("WOKI_WASM_LD")) {
        return from_env;
    }
    const std::string bin_dir = woki_test_wasm_clang_dir();
    if (bin_dir.empty()) {
        return {};
    }
#if defined(_WIN32)
    return "\"" + bin_dir + "\\wasm-ld.exe\"";
#else
    return "\"" + bin_dir + "/wasm-ld\"";
#endif
}

inline bool woki_test_compile_wasm(const std::string& source_path, const std::string& wasm_path, std::string_view export_flags) {
    namespace fs = std::filesystem;

    const std::string clang = woki_test_wasm_clang();
    const std::string bin_dir = woki_test_wasm_clang_dir();

    if (bin_dir.empty()) {
        std::string wl_exports;
        wl_exports.reserve(export_flags.size() + 16);
        for (std::string_view view = export_flags; !view.empty();) {
            constexpr std::string_view export_prefix = "--export=";
            if (view.starts_with(export_prefix)) {
                view.remove_prefix(export_prefix.size());
                const auto end = view.find_first_of(" \t");
                wl_exports += "-Wl,--export=";
                wl_exports.append(view.substr(0, end));
                wl_exports += ' ';
                view.remove_prefix(end == std::string_view::npos ? view.size() : end + 1);
                continue;
            }
            const auto end = view.find_first_of(" \t");
            wl_exports.append(view.substr(0, end));
            wl_exports += ' ';
            view.remove_prefix(end == std::string_view::npos ? view.size() : end + 1);
        }

        const std::string command = clang
                                    + " --target=wasm32-unknown-unknown -nostdlib -fno-builtin "
                                      "-Wl,--no-entry -Wl,--export-memory "
                                    + wl_exports + "-o \"" + wasm_path + "\" \"" + source_path + "\"";
        return std::system(command.c_str()) == 0;
    }

    const std::string wasm_ld = woki_test_wasm_ld_path();

    fs::path obj_path{wasm_path};
    obj_path.replace_extension(".o");

    const std::string compile = woki_test_wasm_compile_prefix() + "\"" + clang + "\"" + woki_test_wasm_toolchain_flags() + " --target=wasm32-unknown-unknown -nostdlib -fno-builtin -c \"" + source_path + "\" -o \""
                                + obj_path.string() + "\"";
    if (std::system(compile.c_str()) != 0) {
        return false;
    }

    std::string linker_exports(export_flags);
    for (size_t pos = 0; (pos = linker_exports.find("-Wl,", pos)) != std::string::npos;) {
        linker_exports.erase(pos, 4);
    }

    const std::string link = woki_test_wasm_compile_prefix() + wasm_ld + " -o \"" + wasm_path + "\" \"" + obj_path.string() + "\" --no-entry --allow-undefined --export-memory " + linker_exports;

    const bool ok = std::system(link.c_str()) == 0;
    std::error_code ec;
    fs::remove(obj_path, ec);
    return ok;
}
