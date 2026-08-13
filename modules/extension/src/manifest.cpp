#include <cctype>
#include <string>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <string_view>
#include <woki/config.hpp>
#include <initializer_list>

#include "woki/ext/manifest.hpp"
#include "woki/ext/path_safety.hpp"

#include "version.h"

namespace woki::ext {

static_assert(kApiVersion == WOKI_EXT_API_VERSION);

namespace {

namespace fs = std::filesystem;
using Json = config::Json;

[[nodiscard]] std::string MissingFieldMessage(std::string_view field, std::string_view example) {
    return "Manifest is missing required field '" + std::string(field) + "'. Add:\n" + std::string(example);
}

[[nodiscard]] std::string WrongTypeMessage(std::string_view field, std::string_view expected, std::string_view example) {
    return "Manifest field '" + std::string(field) + "' must be " + std::string(expected) + ". Use:\n" + std::string(example);
}

[[nodiscard]] bool IsAsciiLowerDigitDashDot(std::string_view value) noexcept {
    return std::ranges::all_of(value, [](unsigned char ch) { return std::islower(ch) != 0 || std::isdigit(ch) != 0 || ch == '-' || ch == '.'; });
}

[[nodiscard]] bool IsValidId(std::string_view id) noexcept {
    if (id.empty() || id.front() == '.' || id.back() == '.' || !id.contains('.')) {
        return false;
    }
    if (!IsAsciiLowerDigitDashDot(id) || id.contains("..")) {
        return false;
    }

    std::size_t segment_start = 0;
    for (std::size_t index = 0; index <= id.size(); ++index) {
        if (index != id.size() && id[index] != '.') {
            continue;
        }

        const std::string_view segment = id.substr(segment_start, index - segment_start);
        if (segment.empty() || segment.front() == '-' || segment.back() == '-') {
            return false;
        }
        segment_start = index + 1;
    }

    return true;
}

[[nodiscard]] bool IsValidCommandId(std::string_view extension_id, std::string_view command_id) {
    return command_id.size() > extension_id.size() && command_id.starts_with(extension_id) && command_id[extension_id.size()] == '.' && IsValidId(command_id);
}

[[nodiscard]] bool IsNumericIdentifier(std::string_view value) noexcept {
    if (value.empty() || (value.size() > 1 && value.front() == '0')) {
        return false;
    }
    return std::ranges::all_of(value, [](unsigned char ch) { return std::isdigit(ch) != 0; });
}

[[nodiscard]] bool IsSemverIdentifier(std::string_view value) noexcept {
    return !value.empty() && std::ranges::all_of(value, [](unsigned char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-'; });
}

[[nodiscard]] bool IsSemverIdentifierList(std::string_view value, bool strict_numeric) noexcept {
    while (!value.empty()) {
        const std::size_t separator = value.find('.');
        const std::string_view identifier = value.substr(0, separator);
        const bool numeric = !identifier.empty() && std::ranges::all_of(identifier, [](unsigned char ch) { return std::isdigit(ch) != 0; });
        if (!IsSemverIdentifier(identifier) || (strict_numeric && numeric && !IsNumericIdentifier(identifier))) {
            return false;
        }
        if (separator == std::string_view::npos) {
            return true;
        }
        value.remove_prefix(separator + 1);
    }
    return false;
}

[[nodiscard]] bool IsSemver(std::string_view version) noexcept {
    if (version.empty() || version.size() > kMaxManifestVersionBytes) {
        return false;
    }

    const std::size_t build_separator = version.find('+');
    if (build_separator != std::string_view::npos) {
        if (version.find('+', build_separator + 1) != std::string_view::npos || !IsSemverIdentifierList(version.substr(build_separator + 1), false)) {
            return false;
        }
        version = version.substr(0, build_separator);
    }

    const std::size_t prerelease_separator = version.find('-');
    if (prerelease_separator != std::string_view::npos) {
        if (!IsSemverIdentifierList(version.substr(prerelease_separator + 1), true)) {
            return false;
        }
        version = version.substr(0, prerelease_separator);
    }

    for (int component = 0; component < 3; ++component) {
        const std::size_t separator = version.find('.');
        if (!IsNumericIdentifier(version.substr(0, separator))) {
            return false;
        }
        if (component == 2) {
            return separator == std::string_view::npos;
        }
        if (separator == std::string_view::npos) {
            return false;
        }
        version.remove_prefix(separator + 1);
    }
    return false;
}

[[nodiscard]] Result<void> ValidateRuntimePath(const fs::path& path) {
    if (path.empty()) {
        return Err(ErrorCode::ParseMissingField, MissingFieldMessage("runtime.wasm", "runtime:\n  wasm: extension.wasm"));
    }
#ifdef _WIN32
    if (path.native().contains(L'\\')) {
        return Err(ErrorCode::ValidationInvalidState, "Manifest field 'runtime.wasm' must be a portable relative path without '.', '..', or backslashes. Use a file inside the package, "
                                                      "for example:\nruntime:\n  wasm: extension.wasm");
    }
#endif
    if (!IsSafeRelativePath(path)) {
        return Err(ErrorCode::ValidationInvalidState, "Manifest field 'runtime.wasm' must be a portable relative path without '.', '..', or backslashes. Use a file inside the package, "
                                                      "for example:\nruntime:\n  wasm: extension.wasm");
    }
    return Ok();
}

[[nodiscard]] Result<std::string> ReadManifest(const fs::path& path) {
    std::error_code error;
    if (fs::is_symlink(fs::symlink_status(path, error))) {
        return Err(ErrorCode::FileAccessDenied, "Extension manifest must not be a symbolic link: " + path.string());
    }
    if (!fs::is_regular_file(path, error)) {
        return Err(ErrorCode::FileNotFound, "Extension manifest is missing. Create manifest.yaml with required fields: id, name, "
                                            "version, apiVersion, runtime.wasm, permissions.");
    }

    const auto size = fs::file_size(path, error);
    if (error) {
        return Err(ErrorCode::FileReadError, error.message());
    }
    if (size > kMaxManifestBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Manifest exceeds 64 KiB. Remove generated data or move large metadata into assets.");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input.good()) {
        return Err(ErrorCode::FileReadError, "Failed to open extension manifest: " + path.string());
    }
    std::string contents(kMaxManifestBytes + 1, '\0');
    input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    contents.resize(static_cast<std::size_t>(input.gcount()));
    if (contents.size() > kMaxManifestBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Manifest exceeds 64 KiB. Remove generated data or move large metadata into assets.");
    }
    if (input.bad()) {
        return Err(ErrorCode::FileReadError, "Failed to read extension manifest: " + path.string());
    }
    return Ok(std::move(contents));
}

[[nodiscard]] Result<std::string> RequiredString(const Json& root, const char* key, std::string_view example) {
    const Json node = root[key];
    if (!node) {
        return Err(ErrorCode::ParseMissingField, MissingFieldMessage(key, example));
    }
    if (!node.is_string()) {
        return Err(ErrorCode::ParseTypeMismatch, WrongTypeMessage(key, "a string", example));
    }
    return Ok(node.get<std::string>());
}

[[nodiscard]] Result<u32> RequiredApiVersion(const Json& root) {
    const Json node = root["apiVersion"];
    if (!node) {
        return Err(ErrorCode::ParseMissingField, MissingFieldMessage("apiVersion", "apiVersion: 1"));
    }
    if (!node.is_number_unsigned()) {
        return Err(ErrorCode::ParseTypeMismatch, WrongTypeMessage("apiVersion", "an integer", "apiVersion: 1"));
    }

    return Ok(node.get<u32>());
}

[[nodiscard]] Result<fs::path> RequiredWasmPath(const Json& root) {
    const Json runtime = root["runtime"];
    if (!runtime) {
        return Err(ErrorCode::ParseMissingField, MissingFieldMessage("runtime", "runtime:\n  wasm: extension.wasm"));
    }
    if (!runtime.is_object()) {
        return Err(ErrorCode::ParseTypeMismatch, WrongTypeMessage("runtime", "a map", "runtime:\n  wasm: extension.wasm"));
    }

    const Json wasm = runtime["wasm"];
    if (!wasm) {
        return Err(ErrorCode::ParseMissingField, MissingFieldMessage("runtime.wasm", "runtime:\n  wasm: extension.wasm"));
    }
    if (!wasm.is_string()) {
        return Err(ErrorCode::ParseTypeMismatch, WrongTypeMessage("runtime.wasm", "a relative path string", "runtime:\n  wasm: extension.wasm"));
    }
    const std::string text = wasm.get<std::string>();
    if (text.contains('\\')) {
        return Err(ErrorCode::ValidationInvalidState, "Manifest field 'runtime.wasm' must not contain backslashes.");
    }
    return Ok(fs::path(text));
}

[[nodiscard]] Result<std::vector<Permission>> ParsePermissions(const Json& root) {
    const Json permissions = root["permissions"];
    if (!permissions) {
        return Err(ErrorCode::ParseMissingField, MissingFieldMessage("permissions", "permissions:\n  - log"));
    }
    if (!permissions.is_array()) {
        return Err(ErrorCode::ParseTypeMismatch, WrongTypeMessage("permissions", "a sequence", "permissions:\n  - log"));
    }
    if (permissions.size() > AllPermissions().size()) {
        return Err(ErrorCode::ValidationOutOfRange, "Manifest exceeds the number of supported permissions.");
    }

    std::vector<Permission> parsed;
    parsed.reserve(permissions.size());
    for (const Json& permission_node : permissions) {
        if (!permission_node.is_string()) {
            return Err(ErrorCode::ParseTypeMismatch, "Each manifest permission must be a string. Use:\npermissions:\n  - log");
        }

        auto permission = ParsePermission(permission_node.get<std::string>());
        if (!permission) {
            return Err(permission.error());
        }
        if (std::ranges::find(parsed, *permission) != parsed.end()) {
            return Err(ErrorCode::ValidationInvalidState, "Manifest contains duplicate permission: " + std::string(ToString(*permission)));
        }
        parsed.push_back(*permission);
    }

    return Ok(std::move(parsed));
}

[[nodiscard]] Result<std::vector<GuestLibrary>> ParseLibraries(const Json& root) {
    const Json libraries = root["libraries"];
    if (!libraries)
        return Ok(std::vector<GuestLibrary>{});
    if (!libraries.is_array())
        return Err(ErrorCode::ParseTypeMismatch, WrongTypeMessage("libraries", "a sequence", "libraries:\n  - math"));
    if (libraries.size() > kMaxManifestLibraries)
        return Err(ErrorCode::ValidationOutOfRange, "Manifest exceeds the number of supported guest libraries.");

    std::vector<GuestLibrary> parsed;
    parsed.reserve(libraries.size());
    for (const Json& node : libraries) {
        if (!node.is_string())
            return Err(ErrorCode::ParseTypeMismatch, "Each manifest library must be a string. Supported libraries are math and ecs.");
        const std::string name = node.get<std::string>();
        const auto library = name == "math" ? GuestLibrary::Math : name == "ecs" ? GuestLibrary::Ecs : static_cast<GuestLibrary>(255);
        if (library != GuestLibrary::Math && library != GuestLibrary::Ecs)
            return Err(ErrorCode::ValidationInvalidState, "Unknown manifest library '" + name + "'. Supported libraries are math and ecs.");
        if (std::ranges::find(parsed, library) != parsed.end())
            return Err(ErrorCode::ValidationInvalidState, "Manifest contains duplicate library: " + name);
        parsed.push_back(library);
    }
    return Ok(std::move(parsed));
}

[[nodiscard]] Result<ActivationMetadata> ParseActivation(const Json& root) {
    const Json activation = root["activation"];
    if (!activation)
        return Ok(ActivationMetadata{});
    if (!activation.is_object())
        return Err(ErrorCode::ParseTypeMismatch, WrongTypeMessage("activation", "a map", "activation:\n  startup: true"));

    ActivationMetadata parsed;
    for (const auto& [key, target] : {std::pair{"startup", &parsed.startup}, std::pair{"tick", &parsed.tick}}) {
        const Json value = activation[key];
        if (!value)
            continue;
        if (!value.is_boolean())
            return Err(ErrorCode::ParseTypeMismatch, WrongTypeMessage(std::string("activation.") + key, "a boolean", std::string(key) + ": true"));
        *target = value.get<bool>();
    }

    return Ok(std::move(parsed));
}

[[nodiscard]] Result<void> RejectUnknownFields(const Json& map, std::initializer_list<std::string_view> allowed, std::string_view field) {
    for (const auto& [key, value] : map.items()) {
        static_cast<void>(value);
        if (std::ranges::find(allowed, key) == allowed.end()) {
            return Err(ErrorCode::ParseUnexpectedToken, "Unknown manifest field '" + (field.empty() ? key : std::string(field) + "." + key) + "'.");
        }
    }
    return Ok();
}

[[nodiscard]] Result<void> ValidateKnownFields(const Json& root) {
    auto known = RejectUnknownFields(root, {"$schema", "id", "name", "version", "apiVersion", "runtime", "libraries", "permissions", "activation", "contributes"}, {});
    if (!known) {
        return known;
    }
    if (const Json runtime = root["runtime"]; runtime && runtime.is_object()) {
        if (auto valid = RejectUnknownFields(runtime, {"wasm"}, "runtime"); !valid) {
            return valid;
        }
    }
    if (const Json activation = root["activation"]; activation && activation.is_object()) {
        if (auto valid = RejectUnknownFields(activation, {"startup", "tick"}, "activation"); !valid)
            return valid;
    }
    if (const Json contributes = root["contributes"]; contributes && contributes.is_object()) {
        if (auto valid = RejectUnknownFields(contributes, {"commands"}, "contributes"); !valid) {
            return valid;
        }
        if (const Json commands = contributes["commands"]; commands && commands.is_array()) {
            for (const Json& command : commands) {
                if (command.is_object()) {
                    if (auto valid = RejectUnknownFields(command, {"id", "title", "category"}, "contributes.commands[]"); !valid) {
                        return valid;
                    }
                }
            }
        }
    }
    return Ok();
}

[[nodiscard]] Result<std::vector<CommandContribution>> ParseCommands(const Json& root) {
    const Json contributes = root["contributes"];
    if (!contributes) {
        return Ok(std::vector<CommandContribution>{});
    }
    if (!contributes.is_object()) {
        return Err(ErrorCode::ParseTypeMismatch, WrongTypeMessage("contributes", "a map", "contributes:\n  commands:\n    - id: woki.hello.say\n      title: Say Hello"));
    }

    const Json commands = contributes["commands"];
    if (!commands) {
        return Ok(std::vector<CommandContribution>{});
    }
    if (!commands.is_array()) {
        return Err(ErrorCode::ParseTypeMismatch, WrongTypeMessage("contributes.commands", "a sequence", "contributes:\n  commands:\n    - id: woki.hello.say\n      title: Say Hello"));
    }
    if (commands.size() > kMaxManifestCommands) {
        return Err(ErrorCode::ValidationOutOfRange, "Manifest exceeds 256 command contributions.");
    }

    std::vector<CommandContribution> parsed;
    parsed.reserve(commands.size());
    for (const Json& command_node : commands) {
        if (!command_node.is_object()) {
            return Err(ErrorCode::ParseTypeMismatch, "Each command contribution must be a map. Use:\n"
                                                     "contributes:\n  commands:\n    - id: woki.hello.say\n      title: Say Hello");
        }

        auto id = RequiredString(command_node, "id", "id: woki.hello.say");
        if (!id) {
            return Err(id.error());
        }

        auto title = RequiredString(command_node, "title", "title: Say Hello");
        if (!title) {
            return Err(title.error());
        }

        CommandContribution command{
            .id = std::move(*id),
            .title = std::move(*title),
            .category = {},
        };

        const Json category = command_node["category"];
        if (category) {
            if (!category.is_string()) {
                return Err(ErrorCode::ParseTypeMismatch, WrongTypeMessage("category", "a string", "category: Tools"));
            }
            command.category = category.get<std::string>();
        }

        parsed.push_back(std::move(command));
    }

    return Ok(std::move(parsed));
}

} // namespace

Result<Manifest> LoadManifest(const fs::path& path) {
    auto contents = ReadManifest(path);
    if (!contents) {
        return Err(contents.error());
    }

    {
        auto document = config::Document::ParseYaml(*contents, path.string(), {.bytes = kMaxManifestBytes, .depth = 32, .nodes = 4096, .members = 4096, .string_bytes = 16 * 1024});
        if (!document)
            return Err(ErrorCode::ParseInvalidFormat, config::FormatDiagnostics(document.error()));
        const auto* schema = config::Registry::Global().Find("extension.manifest", 1);
        const auto schema_diagnostics = config::Validate(*document, *schema);
        if (!schema_diagnostics.empty())
            return Err(ErrorCode::ParseInvalidFormat, config::FormatDiagnostics(schema_diagnostics));
        const Json root = Json::FromDocument(std::move(*document));
        if (!root.is_object()) {
            return Err(ErrorCode::ParseInvalidFormat, "Manifest must be a YAML map. Minimal example:\nid: woki.hello\nname: "
                                                      "Hello\nversion: 0.1.0\napiVersion: 1\nruntime:\n  wasm: "
                                                      "extension.wasm\npermissions:\n  - log");
        }

        if (auto known = ValidateKnownFields(root); !known) {
            return Err(known.error());
        }

        Manifest manifest;

        auto id = RequiredString(root, "id", "id: woki.hello");
        if (!id) {
            return Err(id.error());
        }
        manifest.id = std::move(*id);

        auto name = RequiredString(root, "name", "name: Hello");
        if (!name) {
            return Err(name.error());
        }
        manifest.name = std::move(*name);

        auto version = RequiredString(root, "version", "version: 0.1.0");
        if (!version) {
            return Err(version.error());
        }
        manifest.version = std::move(*version);

        auto api_version = RequiredApiVersion(root);
        if (!api_version) {
            return Err(api_version.error());
        }
        manifest.api_version = *api_version;

        auto wasm_path = RequiredWasmPath(root);
        if (!wasm_path) {
            return Err(wasm_path.error());
        }
        manifest.wasm_path = std::move(*wasm_path);

        auto libraries = ParseLibraries(root);
        if (!libraries)
            return Err(libraries.error());
        manifest.libraries = std::move(*libraries);

        auto permissions = ParsePermissions(root);
        if (!permissions) {
            return Err(permissions.error());
        }
        manifest.requested_capabilities.permissions = std::move(*permissions);

        auto activation = ParseActivation(root);
        if (!activation)
            return Err(activation.error());
        manifest.activation = std::move(*activation);

        auto commands = ParseCommands(root);
        if (!commands) {
            return Err(commands.error());
        }
        manifest.commands = std::move(*commands);

        auto valid = ValidateManifest(manifest);
        if (!valid) {
            return Err(valid.error());
        }

        return Ok(std::move(manifest));
    }
}

Result<void> ValidateManifest(const Manifest& manifest) {
    if (manifest.id.size() > kMaxManifestIdBytes || !IsValidId(manifest.id)) {
        return Err(ErrorCode::ValidationInvalidState, "Manifest field 'id' is invalid. Use lowercase reverse-DNS-style segments with "
                                                      "letters, digits, dots, and dashes, for example:\nid: woki.hello");
    }
    if (manifest.name.empty()) {
        return Err(ErrorCode::ParseMissingField, MissingFieldMessage("name", "name: Hello"));
    }
    if (manifest.name.size() > kMaxManifestNameBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Manifest field 'name' exceeds 256 bytes.");
    }
    if (!IsSemver(manifest.version)) {
        return Err(ErrorCode::ValidationInvalidState, "Manifest field 'version' is invalid. Use a Semantic Versioning 2.0.0 value, for "
                                                      "example:\nversion: 0.1.0");
    }
    if (manifest.api_version != kApiVersion) {
        return Err(ErrorCode::ValidationInvalidState, "Manifest field 'apiVersion' is unsupported. For this build use:\napiVersion: 1");
    }
    if (manifest.libraries.size() > kMaxManifestLibraries)
        return Err(ErrorCode::ValidationOutOfRange, "Manifest exceeds the number of supported guest libraries.");
    for (std::size_t index = 0; index < manifest.libraries.size(); ++index) {
        const GuestLibrary library = manifest.libraries[index];
        if (library != GuestLibrary::Math && library != GuestLibrary::Ecs)
            return Err(ErrorCode::ValidationInvalidState, "Manifest contains an unknown guest library.");
        if (std::ranges::find(manifest.libraries.begin(), manifest.libraries.begin() + static_cast<std::ptrdiff_t>(index), library) != manifest.libraries.begin() + static_cast<std::ptrdiff_t>(index))
            return Err(ErrorCode::ValidationInvalidState, "Manifest contains a duplicate guest library.");
    }
    const auto& permissions = manifest.requested_capabilities.permissions;
    if (permissions.size() > AllPermissions().size())
        return Err(ErrorCode::ValidationOutOfRange, "Manifest exceeds the number of supported permissions.");
    for (std::size_t index = 0; index < permissions.size(); ++index) {
        if (std::ranges::find(permissions.begin(), permissions.begin() + static_cast<std::ptrdiff_t>(index), permissions[index]) != permissions.begin() + static_cast<std::ptrdiff_t>(index))
            return Err(ErrorCode::ValidationInvalidState, "Manifest contains duplicate permission: " + std::string(ToString(permissions[index])));
    }

    if (manifest.commands.size() > kMaxManifestCommands) {
        return Err(ErrorCode::ValidationOutOfRange, "Manifest exceeds 256 command contributions.");
    }
    std::vector<std::string_view> command_ids;
    command_ids.reserve(manifest.commands.size());
    for (const CommandContribution& command : manifest.commands) {
        if (command.id.size() > kMaxCommandIdBytes || !IsValidCommandId(manifest.id, command.id)) {
            return Err(ErrorCode::ValidationInvalidState, "Manifest command id '" + command.id
                                                              + "' is invalid. Use a lowercase reverse-DNS id prefixed by the extension id, "
                                                                "for example:\ncontributes:\n  commands:\n    - id: "
                                                              + manifest.id + ".example\n      title: Example");
        }
        if (command.title.empty()) {
            return Err(ErrorCode::ParseMissingField, "Manifest command '" + command.id
                                                         + "' is missing a non-empty title. Add:\n"
                                                           "contributes:\n  commands:\n    - id: "
                                                         + command.id + "\n      title: Example");
        }
        if (command.title.size() > kMaxCommandTitleBytes || command.category.size() > kMaxCommandCategoryBytes) {
            return Err(ErrorCode::ValidationOutOfRange, "Manifest command text exceeds its size limit: " + command.id);
        }
        if (std::ranges::find(command_ids, std::string_view(command.id)) != command_ids.end()) {
            return Err(ErrorCode::ValidationInvalidState, "Manifest contains duplicate command id: " + command.id);
        }
        command_ids.push_back(command.id);
    }

    if (manifest.wasm_path.generic_string().size() > kMaxRuntimePathBytes)
        return Err(ErrorCode::ValidationOutOfRange, "Manifest field 'runtime.wasm' exceeds 4096 bytes.");
    return ValidateRuntimePath(manifest.wasm_path);
}

Result<void> ValidateManifestForPackage(const Manifest& manifest, std::string_view package_id) {
    auto valid = ValidateManifest(manifest);
    if (!valid) {
        return valid;
    }
    if (manifest.id != package_id) {
        return Err(ErrorCode::ValidationInvalidState, "Manifest field 'id' must match the package directory name. Directory is '" + std::string(package_id) + "', manifest id is '" + manifest.id + "'.");
    }
    return Ok();
}

bool HasPermission(const Manifest& manifest, Permission permission) noexcept {
    return HasPermission(manifest.requested_capabilities, permission);
}

bool IsValidExtensionId(std::string_view id) noexcept {
    return id.size() <= kMaxManifestIdBytes && IsValidId(id);
}

} // namespace woki::ext
