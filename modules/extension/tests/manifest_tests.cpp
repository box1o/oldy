#include <fstream>
#include <filesystem>
#include <string_view>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/manifest.hpp>

namespace {

namespace fs = std::filesystem;

[[nodiscard]] fs::path MakeTempDir(std::string_view name) {
    const fs::path root = fs::temp_directory_path() / "woki_extension_tests" / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void WriteFile(const fs::path& path, std::string_view contents) {
    std::ofstream output(path);
    REQUIRE(output.good());
    output << contents;
}

constexpr std::string_view kValidManifest = R"(
id: woki.hello
name: Hello
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions:
  - log
)";

} // namespace

TEST_CASE("Extension manifest loads a valid manifest") {
    const fs::path root = MakeTempDir("valid_manifest");
    const fs::path path = root / "manifest.yaml";
    WriteFile(path, kValidManifest);

    auto manifest = woki::ext::LoadManifest(path);
    REQUIRE(manifest.has_value());
    REQUIRE(manifest->id == "woki.hello");
    REQUIRE(manifest->name == "Hello");
    REQUIRE(manifest->version == "0.1.0");
    REQUIRE(manifest->api_version == woki::ext::kApiVersion);
    REQUIRE(manifest->wasm_path == "extension.wasm");
    REQUIRE(woki::ext::HasPermission(*manifest, woki::ext::Permission::Log));
}

TEST_CASE("Extension manifest loads command contributions") {
    const fs::path root = MakeTempDir("command_contributions");
    const fs::path path = root / "manifest.yaml";
    WriteFile(path, R"(
id: woki.hello
name: Hello
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions:
  - log
contributes:
  commands:
    - id: woki.hello.say
      title: Say Hello
      category: Examples
)");

    auto manifest = woki::ext::LoadManifest(path);
    REQUIRE(manifest.has_value());
    REQUIRE(manifest->commands.size() == 1);
    REQUIRE(manifest->commands.front().id == "woki.hello.say");
    REQUIRE(manifest->commands.front().title == "Say Hello");
    REQUIRE(manifest->commands.front().category == "Examples");
}

TEST_CASE("Extension manifest parses explicit activation and defaults to lazy only") {
    const fs::path root = MakeTempDir("activation");
    WriteFile(root / "manifest.yaml", R"(
id: woki.hello
name: Hello
version: 1.0.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions: [events]
activation:
  startup: true
  tick: true
  events: [window.resized, app.resume]
)");
    auto manifest = woki::ext::LoadManifest(root / "manifest.yaml");
    REQUIRE(manifest);
    CHECK(manifest->activation.startup);
    CHECK(manifest->activation.tick);
    CHECK(woki::ext::ActivatesOn(*manifest, woki::ext::ApplicationEventType::WindowResized));
    CHECK(woki::ext::ActivatesOn(*manifest, woki::ext::ApplicationEventType::AppResume));

    WriteFile(root / "manifest.yaml", kValidManifest);
    manifest = woki::ext::LoadManifest(root / "manifest.yaml");
    REQUIRE(manifest);
    CHECK_FALSE(manifest->activation.startup);
    CHECK_FALSE(manifest->activation.tick);
    CHECK(manifest->activation.events.empty());
}

TEST_CASE("Extension manifest rejects invalid activation metadata") {
    const fs::path root = MakeTempDir("invalid_activation");
    WriteFile(root / "manifest.yaml", std::string(kValidManifest) + "activation:\n  events: [window.resized]\n");
    auto manifest = woki::ext::LoadManifest(root / "manifest.yaml");
    REQUIRE_FALSE(manifest);
    CHECK(manifest.error().Message().contains("events' permission"));

    WriteFile(root / "manifest.yaml", std::string(kValidManifest) + "activation:\n  events: [not.an-event]\n");
    manifest = woki::ext::LoadManifest(root / "manifest.yaml");
    REQUIRE_FALSE(manifest);
    CHECK(manifest.error().Message().contains("not.an-event"));
}

TEST_CASE("Extension manifest rejects command ids outside the extension namespace") {
    const fs::path root = MakeTempDir("bad_command_id");
    const fs::path path = root / "manifest.yaml";
    WriteFile(path, R"(
id: woki.hello
name: Hello
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions:
  - log
contributes:
  commands:
    - id: woki.other.say
      title: Say Hello
)");

    auto manifest = woki::ext::LoadManifest(path);
    REQUIRE_FALSE(manifest.has_value());
    REQUIRE(manifest.error().Message().contains("woki.other.say"));
    REQUIRE(manifest.error().Message().contains("woki.hello.example"));
}

TEST_CASE("Extension manifest rejects invalid id") {
    woki::ext::Manifest manifest;
    manifest.id = "Woki.Hello";
    manifest.name = "Hello";
    manifest.version = "0.1.0";
    manifest.requested_capabilities.permissions = {woki::ext::Permission::Log};

    auto valid = woki::ext::ValidateManifest(manifest);
    REQUIRE_FALSE(valid.has_value());
    REQUIRE(valid.error().Code() == woki::ErrorCode::ValidationInvalidState);
    REQUIRE(valid.error().Message().contains("id"));
    REQUIRE(valid.error().Message().contains("woki.hello"));
}

TEST_CASE("Extension manifest rejects malformed id segments") {
    woki::ext::Manifest manifest;
    manifest.id = "woki.-hello";
    manifest.name = "Hello";
    manifest.version = "0.1.0";
    manifest.requested_capabilities.permissions = {woki::ext::Permission::Log};

    auto valid = woki::ext::ValidateManifest(manifest);
    REQUIRE_FALSE(valid.has_value());
    REQUIRE(valid.error().Code() == woki::ErrorCode::ValidationInvalidState);
}

TEST_CASE("Extension manifest rejects path traversal") {
    woki::ext::Manifest manifest;
    manifest.id = "woki.hello";
    manifest.name = "Hello";
    manifest.version = "0.1.0";
    manifest.wasm_path = "../extension.wasm";
    manifest.requested_capabilities.permissions = {woki::ext::Permission::Log};

    auto valid = woki::ext::ValidateManifest(manifest);
    REQUIRE_FALSE(valid.has_value());
    REQUIRE(valid.error().Code() == woki::ErrorCode::ValidationInvalidState);
}

TEST_CASE("Extension manifest rejects unknown permission from YAML") {
    const fs::path root = MakeTempDir("unknown_permission");
    const fs::path path = root / "manifest.yaml";
    WriteFile(path, R"(
id: woki.hello
name: Hello
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions:
  - network
)");

    auto manifest = woki::ext::LoadManifest(path);
    REQUIRE_FALSE(manifest.has_value());
    REQUIRE(manifest.error().Code() == woki::ErrorCode::ValidationInvalidState);
    REQUIRE(manifest.error().Message().contains("network"));
    REQUIRE(manifest.error().Message().contains("log"));
}

TEST_CASE("Extension manifest validates package directory name") {
    woki::ext::Manifest manifest;
    manifest.id = "woki.hello";
    manifest.name = "Hello";
    manifest.version = "0.1.0";
    manifest.requested_capabilities.permissions = {woki::ext::Permission::Log};

    auto valid = woki::ext::ValidateManifestForPackage(manifest, "woki.other");
    REQUIRE_FALSE(valid.has_value());
    REQUIRE(valid.error().Code() == woki::ErrorCode::ValidationInvalidState);
    REQUIRE(valid.error().Message().contains("woki.other"));
    REQUIRE(valid.error().Message().contains("woki.hello"));
}

TEST_CASE("Extension manifest explains missing fields") {
    const fs::path root = MakeTempDir("missing_fields");
    const fs::path path = root / "manifest.yaml";
    WriteFile(path, R"(
id: woki.hello
name: Hello
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
)");

    auto manifest = woki::ext::LoadManifest(path);
    REQUIRE_FALSE(manifest.has_value());
    REQUIRE(manifest.error().Code() == woki::ErrorCode::ParseMissingField);
    REQUIRE(manifest.error().Message().contains("permissions"));
    REQUIRE(manifest.error().Message().contains("- log"));
}

TEST_CASE("Extension manifest rejects fields excluded by the schema") {
    const fs::path root = MakeTempDir("unknown_field");
    const fs::path path = root / "manifest.yaml";
    WriteFile(path, std::string(kValidManifest) + "unknown: true\n");

    auto manifest = woki::ext::LoadManifest(path);
    REQUIRE_FALSE(manifest.has_value());
    REQUIRE(manifest.error().Code() == woki::ErrorCode::ParseUnexpectedToken);
    REQUIRE(manifest.error().Message().contains("unknown"));
}

TEST_CASE("Extension manifest rejects duplicate permissions") {
    const fs::path root = MakeTempDir("duplicate_permission");
    const fs::path path = root / "manifest.yaml";
    WriteFile(path, R"(
id: woki.hello
name: Hello
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions:
  - log
  - log
)");

    auto manifest = woki::ext::LoadManifest(path);
    REQUIRE_FALSE(manifest.has_value());
    REQUIRE(manifest.error().Message().contains("duplicate permission"));
}

TEST_CASE("Extension manifest rejects runtime paths with dot components") {
    auto manifest = woki::ext::Manifest{
        .id = "woki.hello",
        .name = "Hello",
        .version = "0.1.0",
        .wasm_path = "nested/./extension.wasm",
        .requested_capabilities = {},
        .activation = {},
        .commands = {},
    };
    REQUIRE_FALSE(woki::ext::ValidateManifest(manifest).has_value());
}

TEST_CASE("Extension manifest rejects backslashes from the YAML scalar") {
    const fs::path root = MakeTempDir("yaml_backslash_path");
    const fs::path path = root / "manifest.yaml";
    WriteFile(path, R"(
id: woki.hello
name: Hello
version: 0.1.0
apiVersion: 1
runtime:
  wasm: 'nested\extension.wasm'
permissions: []
)");

    auto manifest = woki::ext::LoadManifest(path);
    REQUIRE_FALSE(manifest.has_value());
    REQUIRE(manifest.error().Message().contains("backslashes"));
}

TEST_CASE("Extension manifest enforces Semantic Versioning 2.0.0") {
    auto manifest = woki::ext::Manifest{
        .id = "woki.hello",
        .name = "Hello",
        .version = "1.2.3-alpha.1+build.7",
        .requested_capabilities = {},
        .activation = {},
        .commands = {},
    };
    REQUIRE(woki::ext::ValidateManifest(manifest).has_value());

    for (const std::string_view invalid : {"1", "1.2", "01.2.3", "1.02.3", "1.2.03", "1.2.3-01", "1.2.3+", "v1.2.3"}) {
        manifest.version = invalid;
        REQUIRE_FALSE(woki::ext::ValidateManifest(manifest).has_value());
    }
}

TEST_CASE("Extension manifest rejects duplicate YAML keys") {
    const fs::path root = MakeTempDir("duplicate_keys");
    const fs::path path = root / "manifest.yaml";
    WriteFile(path, R"(
id: woki.hello
id: woki.other
name: Hello
version: 1.0.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions: []
)");

    auto manifest = woki::ext::LoadManifest(path);
    REQUIRE_FALSE(manifest.has_value());
    INFO(manifest.error().Message());
    REQUIRE(manifest.error().Message().contains("map keys must be unique"));
}

TEST_CASE("Extension manifest enforces field size limits") {
    auto manifest = woki::ext::Manifest{
        .id = "woki.hello",
        .name = std::string(woki::ext::kMaxManifestNameBytes + 1, 'x'),
        .version = "1.0.0",
        .requested_capabilities = {},
        .activation = {},
        .commands = {},
    };
    auto valid = woki::ext::ValidateManifest(manifest);
    REQUIRE_FALSE(valid.has_value());
    REQUIRE(valid.error().Code() == woki::ErrorCode::ValidationOutOfRange);
}

TEST_CASE("Extension manifest programmatic validation matches schema constraints") {
    auto manifest = woki::ext::Manifest{
        .id = "woki.hello",
        .name = "Hello",
        .version = "1.0.0",
        .requested_capabilities = {{woki::ext::Permission::Log}},
        .activation = {},
        .commands = {{"woki.hello.run", "Run", "Tools"}},
    };
    REQUIRE(woki::ext::ValidateManifest(manifest));

    manifest.requested_capabilities.permissions.push_back(woki::ext::Permission::Log);
    REQUIRE_FALSE(woki::ext::ValidateManifest(manifest));
    manifest.requested_capabilities.permissions.pop_back();

    manifest.wasm_path = std::string(woki::ext::kMaxRuntimePathBytes + 1, 'x');
    REQUIRE_FALSE(woki::ext::ValidateManifest(manifest));
    manifest.wasm_path = "extension.wasm";

    manifest.commands.push_back(manifest.commands.front());
    REQUIRE_FALSE(woki::ext::ValidateManifest(manifest));
}

TEST_CASE("Extension manifest parser and schema agree on scalar and collection boundaries") {
    const fs::path schema_path = fs::path(WOKI_EXTENSION_SOURCE_DIR) / "schema" / "manifest.schema.json";
    std::ifstream schema_input(schema_path);
    REQUIRE(schema_input.good());
    const std::string schema{std::istreambuf_iterator<char>(schema_input), std::istreambuf_iterator<char>()};
    CHECK(schema.find("\"maxLength\": 4096") != std::string::npos);
    CHECK(schema.find("\"maxItems\": 5") != std::string::npos);
    CHECK(schema.find("\"uniqueItems\": true") != std::string::npos);

    const fs::path root = MakeTempDir("schema_boundaries");
    WriteFile(root / "manifest.yaml", R"(
id: woki.boundary
name: 'true: still a string'
version: 1.0.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions: [log, paths, storage, config, events]
contributes:
  commands:
    - id: woki.boundary.run
      title: Run
)");
    REQUIRE(woki::ext::LoadManifest(root / "manifest.yaml"));
}
