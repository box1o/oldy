#include <cctype>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <filesystem>

#include "wokiext/cli.hpp"

namespace wokiext {

namespace {

namespace fs = std::filesystem;

[[nodiscard]] std::string Slug(std::string_view text, char separator) {
    std::string out;
    bool previous_separator = false;
    for (const char raw_ch : text) {
        const auto ch = static_cast<unsigned char>(raw_ch);
        if (std::isalnum(ch) != 0) {
            out.push_back(static_cast<char>(std::tolower(ch)));
            previous_separator = false;
            continue;
        }
        if (!previous_separator && !out.empty()) {
            out.push_back(separator);
            previous_separator = true;
        }
    }
    while (!out.empty() && out.back() == separator) {
        out.pop_back();
    }
    return out.empty() ? "extension" : out;
}

[[nodiscard]] std::string ToId(std::string_view name) {
    return "woki." + Slug(name, '.');
}

void WriteFile(const fs::path& path, std::string_view contents) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.good()) {
        throw std::runtime_error("Failed to create file: " + path.string());
    }
    output << contents;
}

[[nodiscard]] std::string Manifest(std::string_view id, std::string_view name) {
    return "id: " + std::string(id) + R"yaml(
name: )yaml"
           + std::string(name) + R"yaml(
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions:
  - log
)yaml";
}

[[nodiscard]] std::string PluginSource(std::string_view lang) {
    const bool cpp = lang == "cpp";
    const std::string init_message = cpp ? "    static constexpr char kMessage[] = \"hello from wokiext\";\n" : "    static const char kMessage[] = \"hello from wokiext\";\n";
    const std::string extern_open = cpp ? "extern \"C\" {\n\n" : "";
    const std::string extern_close = cpp ? "\n} // extern \"C\"\n" : "";

    return std::string("#include \"version.h\"\n#include \"ext.h\"\n#include \"host_imports.h\"\n"
                       "#include \"guest_alloc.h\"\n\n")
           + extern_open +
           R"(
WOKI_EXPORT("ext_api_version")
uint32_t ext_api_version(void) { return WOKI_EXT_API_VERSION; }

WOKI_EXPORT("ext_init")
int32_t ext_init(void) {
)" + init_message
           +
           R"(    return host_log(WOKI_EXT_LOG_INFO, kMessage, sizeof(kMessage) - 1);
}

WOKI_EXPORT("ext_on_tick")
void ext_on_tick(double dt_ms) { (void)dt_ms; }

WOKI_EXPORT("ext_on_event")
void ext_on_event(uint32_t type, const uint8_t* payload, uint32_t len) {
    (void)type;
    (void)payload;
    (void)len;
}

WOKI_EXPORT("ext_on_command")
int32_t ext_on_command(const char* command_id, uint32_t command_len,
    const uint8_t* payload, uint32_t len) {
    (void)payload;
    (void)len;
    return host_log(WOKI_EXT_LOG_INFO, command_id, command_len);
}

WOKI_EXPORT("ext_on_unload")
void ext_on_unload(void) {}
)" + extern_close;
}

[[nodiscard]] std::string ExtensionCMake(std::string_view lang) {
    const bool cpp = lang == "cpp";
    const std::string source = cpp ? "src/plugin.cpp" : "src/plugin.c";

    return "cmake_minimum_required(VERSION 3.25)\n"
           "\n"
           "if(NOT WOKI_CMAKE_DIR)\n"
           "    message(FATAL_ERROR \"Configure with wokiext build or set WOKI_CMAKE_DIR\")\n"
           "endif()\n"
           "\n"
           "include(\"${WOKI_CMAKE_DIR}/ExtensionProject.cmake\")\n"
           "project(woki_extension LANGUAGES "
           + std::string(cpp ? "CXX" : "C") +
           R"()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include("${WOKI_CMAKE_DIR}/ExtensionWasm.cmake")
add_wokiext()"
           + source + R"()
)";
}

} // namespace

Status Create(const CreateOptions& options) {
    if (options.name.empty()) {
        std::cerr << "Extension name is required\n";
        return Status::Usage;
    }
    if (options.lang != "c" && options.lang != "cpp") {
        std::cerr << "--lang must be c or cpp\n";
        return Status::Usage;
    }

    const std::string dir_name = Slug(options.name, '-');
    const std::string id = options.id.empty() ? ToId(options.name) : options.id;
    const fs::path root = options.out_dir / dir_name;

    std::error_code error;
    if (fs::exists(root, error)) {
        std::cerr << "Refusing to overwrite existing directory: " << root << '\n';
        return Status::Error;
    }

    fs::create_directories(root / "src", error);
    if (error) {
        std::cerr << error.message() << '\n';
        return Status::Error;
    }

    WriteFile(root / "manifest.yaml", Manifest(id, options.name));
    WriteFile(root / "CMakeLists.txt", ExtensionCMake(options.lang));
    WriteFile(root / "src" / (options.lang == "cpp" ? "plugin.cpp" : "plugin.c"), PluginSource(options.lang));

    std::cout << "Created extension project: " << root << '\n';
    return Status::Ok;
}

} // namespace wokiext
