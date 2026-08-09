#include <fstream>
#include <filesystem>
#include <string_view>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/ext.hpp>

namespace {

namespace fs = std::filesystem;

[[nodiscard]] fs::path MakeTempDir(std::string_view name) {
    const fs::path root = fs::temp_directory_path() / "woki_extension_registry_tests" / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void WriteFile(const fs::path& path, std::string_view contents) {
    std::ofstream output(path);
    REQUIRE(output.good());
    output << contents;
}

} // namespace

TEST_CASE("Extension registry scans valid package directories") {
    const fs::path root = MakeTempDir("valid_scan");
    const fs::path package = root / "extensions" / "woki.hello";
    fs::create_directories(package);

    WriteFile(package / "manifest.yaml", R"(
id: woki.hello
name: Hello
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions:
  - log
)");
    WriteFile(package / "extension.wasm", "");

    woki::ext::Registry registry;
    registry.SetRoots({
        .extensions = root / "extensions",
        .data = root / "ext-data",
        .cache = root / "cache" / "woki" / "ext",
    });

    auto scanned = registry.Scan();
    REQUIRE(scanned.has_value());
    REQUIRE(registry.Records().size() == 1);
    REQUIRE(registry.Records()[0].id == "woki.hello");
    REQUIRE(registry.Records()[0].state == woki::ext::State::PermissionChecked);
}

TEST_CASE("Extension registry keeps failed records with useful error text") {
    const fs::path root = MakeTempDir("failed_scan");
    const fs::path package = root / "extensions" / "woki.bad";
    fs::create_directories(package);

    WriteFile(package / "manifest.yaml", R"(
id: woki.bad
name: Bad
version: 0.1.0
apiVersion: 1
runtime:
  wasm: missing.wasm
permissions:
  - log
)");

    woki::ext::Registry registry;
    registry.SetRoots({
        .extensions = root / "extensions",
        .data = root / "ext-data",
        .cache = root / "cache" / "woki" / "ext",
    });

    auto scanned = registry.Scan();
    REQUIRE(scanned.has_value());
    REQUIRE(registry.Records().size() == 1);
    REQUIRE(registry.Records()[0].state == woki::ext::State::Failed);
    REQUIRE(registry.Records()[0].error.contains("missing.wasm"));
}

TEST_CASE("Extension registry source scan allows short development folder names") {
    const fs::path root = MakeTempDir("source_scan");
    const fs::path package = root / "extensions" / "evlog";
    fs::create_directories(package);

    WriteFile(package / "manifest.yaml", R"(
id: woki.evlog
name: Event Log
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions:
  - log
  - storage
  - events
)");
    WriteFile(package / "extension.wasm", "");

    woki::ext::Registry registry;
    registry.SetRoots({
        .extensions = root / "installed",
        .data = root / "ext-data",
        .cache = root / "cache" / "woki" / "ext",
    });

    auto scanned = registry.ScanSource(root / "extensions");
    REQUIRE(scanned.has_value());
    REQUIRE(registry.Records().size() == 1);
    REQUIRE(registry.Records()[0].id == "woki.evlog");
    REQUIRE(registry.Records()[0].package.install_root == package);
    REQUIRE(registry.Records()[0].state == woki::ext::State::PermissionChecked);
}
