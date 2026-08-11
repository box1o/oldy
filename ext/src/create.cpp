#include <cctype>
#include <string>
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <filesystem>

#include <woki/ext/manifest.hpp>

#include "cli_internal.hpp"

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
    output.close();
    if (!output) {
        throw std::runtime_error("Failed to write file: " + path.string());
    }
}

[[nodiscard]] std::string Manifest(std::string_view id, std::string_view name) {
    std::string quoted_name(name);
    std::size_t position = 0;
    while ((position = quoted_name.find('\'', position)) != std::string::npos) {
        quoted_name.insert(position, 1, '\'');
        position += 2;
    }
    return "id: " + std::string(id) + R"yaml(
name: ')yaml"
           + quoted_name + R"yaml('
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions:
  - log
activation:
  startup: true
)yaml";
}

[[nodiscard]] std::string PluginSource(std::string_view lang) {
    const bool cpp = lang == "cpp";
    if (cpp) {
        return R"cpp(#include <woki/ext/plugin.hpp>

using namespace woki::ext;

class Extension final : public Plugin {
public:
    Status OnLoad(Context& context) noexcept {
        return context.GetLog().Info("hello from wokiext");
    }

    void OnEvent(Context&, Event&) noexcept {}

    Status OnCommand(Context& context, StringView command, Bytes) noexcept {
        return context.GetLog().Info(command);
    }
};

WOKI_PLUGIN(Extension)
)cpp";
    }
    return std::string("#include <woki/ext/sdk/version.h>\n#include <woki/ext/sdk/ext.h>\n#include <woki/ext/sdk/host_imports.h>\n"
                       "#include <woki/ext/sdk/guest_alloc.h>\n\n")
           + R"(
WOKI_EXPORT("ext_api_version")
uint32_t ext_api_version(void) { return WOKI_EXT_API_VERSION; }

WOKI_EXPORT("ext_init")
int32_t ext_init(void) {
    static const char kMessage[] = "hello from wokiext";
    return host_log(WOKI_EXT_LOG_INFO, kMessage, sizeof(kMessage) - 1);
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
)";
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
add_wokiext(extension
    LANGUAGE )"
           + std::string(cpp ? "CXX" : "C") + R"(
    MANIFEST manifest.yaml
    SOURCES )"
           + source + R"(
)
)";
}

} // namespace

Status Create(Context& context, const CreateOptions& options) {
    if (options.name.empty()) {
        context.diagnostics.Error("Extension name is required");
        return Status::Usage;
    }
    if (options.lang != "c" && options.lang != "cpp") {
        context.diagnostics.Error("--lang must be c or cpp");
        return Status::Usage;
    }

    const std::string dir_name = Slug(options.name, '-');
    const std::string id = options.id.empty() ? ToId(options.name) : options.id;
    if (!woki::ext::IsValidExtensionId(id)) {
        context.diagnostics.Error("Extension id must use lowercase reverse-DNS-style segments, for example woki.hello");
        return Status::Usage;
    }
    const fs::path root = options.out_dir / dir_name;

    std::error_code error;
    if (context.filesystem.Exists(root, error)) {
        context.diagnostics.Err() << "Refusing to overwrite existing directory: " << root << '\n';
        return Status::Error;
    }

    context.filesystem.CreateDirectories(root / "src", error);
    if (error) {
        context.diagnostics.Error(error.message());
        return Status::Error;
    }

    WriteFile(root / "manifest.yaml", Manifest(id, options.name));
    WriteFile(root / "CMakeLists.txt", ExtensionCMake(options.lang));
    WriteFile(root / "src" / (options.lang == "cpp" ? "plugin.cpp" : "plugin.c"), PluginSource(options.lang));

    context.diagnostics.Out() << "Created extension project: " << root << '\n';
    return Status::Ok;
}

} // namespace wokiext
