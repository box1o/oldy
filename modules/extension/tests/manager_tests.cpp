#include <array>
#include <string>
#include <fstream>
#include <filesystem>
#include <string_view>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/ext.hpp>

namespace {

namespace fs = std::filesystem;

class FakeBackend final : public woki::ext::RuntimeBackend {
public:
    [[nodiscard]] woki::Result<void> Load(woki::ext::Record& record) override {
        if (!fail_load_prefix.empty() && record.id.starts_with(fail_load_prefix)) {
            return woki::Err(woki::ErrorCode::InvalidState, "selected load failure");
        }
        record.tier = woki::ext::RuntimeTier::Wasm;
        ++loads;
        return woki::Ok();
    }

    [[nodiscard]] woki::Result<void> Initialize(woki::ext::Record&) override {
        ++initializes;
        return woki::Ok();
    }

    void Tick(woki::ext::Record&, woki::f64) override {
        ++ticks;
    }

    void DispatchEvent(woki::ext::Record&, woki::u32 event_type, std::span<const woki::u8>) override {
        last_event = event_type;
    }

    [[nodiscard]] woki::Result<void> DispatchCommand(woki::ext::Record&, std::string_view command_id, std::span<const woki::u8> payload) override {
        last_command = command_id;
        last_command_payload_size = payload.size();
        ++commands;
        return woki::Ok();
    }

    void Unload(woki::ext::Record&) override {
        ++unloads;
    }

    int loads{0};
    int initializes{0};
    int ticks{0};
    int commands{0};
    int unloads{0};
    woki::u32 last_event{0};
    std::string last_command;
    std::size_t last_command_payload_size{0};
    std::string fail_load_prefix;
};

[[nodiscard]] fs::path MakeTempDir(std::string_view name) {
    const fs::path root = fs::temp_directory_path() / "woki_extension_manager_tests" / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void WriteFile(const fs::path& path, std::string_view contents) {
    std::ofstream output(path);
    REQUIRE(output.good());
    output << contents;
}

void WritePackage(const fs::path& package) {
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
  - events
contributes:
  commands:
    - id: woki.hello.say
      title: Say Hello
)");
    WriteFile(package / "extension.wasm", "");
}

void WritePackage(const fs::path& package, std::string_view id) {
    fs::create_directories(package);
    WriteFile(package / "manifest.yaml", "id: " + std::string(id) + R"(
name: Test
version: 0.1.0
apiVersion: 1
runtime:
  wasm: extension.wasm
permissions: []
)");
    WriteFile(package / "extension.wasm", "");
}

} // namespace

TEST_CASE("Extension manager scans loads ticks and unloads records") {
    const fs::path root = MakeTempDir("flow");
    WritePackage(root / "extensions" / "woki.hello");

    FakeBackend backend;
    woki::ext::Manager manager(&backend);
    manager.SetRoots({
        .extensions = root / "extensions",
        .data = root / "ext-data",
        .cache = root / "cache" / "woki" / "ext",
    });

    auto scanned = manager.Scan();
    REQUIRE(scanned.has_value());
    REQUIRE(manager.Records().size() == 1);

    auto loaded = manager.Load("woki.hello");
    REQUIRE(loaded.has_value());
    REQUIRE(backend.loads == 1);
    REQUIRE(backend.initializes == 1);
    REQUIRE(manager.Find("woki.hello")->state == woki::ext::State::Active);

    manager.Tick(16.0);
    REQUIRE(backend.ticks == 1);

    manager.DispatchEvent(9, {});
    REQUIRE(backend.last_event == 9);

    const std::array<woki::u8, 3> payload{1, 2, 3};
    auto commanded = manager.ExecuteCommand("woki.hello.say", payload);
    REQUIRE(commanded.has_value());
    REQUIRE(backend.commands == 1);
    REQUIRE(backend.last_command == "woki.hello.say");
    REQUIRE(backend.last_command_payload_size == payload.size());

    manager.UnloadAll();
    REQUIRE(backend.unloads == 1);
    REQUIRE(manager.Find("woki.hello")->state == woki::ext::State::Unloaded);
}

TEST_CASE("Extension manager reports unknown command ids") {
    const fs::path root = MakeTempDir("missing_command");
    WritePackage(root / "extensions" / "woki.hello");

    FakeBackend backend;
    woki::ext::Manager manager(&backend);
    manager.SetRoots({
        .extensions = root / "extensions",
        .data = root / "ext-data",
        .cache = root / "cache" / "woki" / "ext",
    });

    REQUIRE(manager.Scan().has_value());
    auto commanded = manager.ExecuteCommand("woki.hello.missing");
    REQUIRE_FALSE(commanded.has_value());
    REQUIRE(commanded.error().Code() == woki::ErrorCode::FileNotFound);
}

TEST_CASE("Extension manager reports missing extension ids") {
    FakeBackend backend;
    woki::ext::Manager manager(&backend);

    auto loaded = manager.Load("woki.missing");
    REQUIRE_FALSE(loaded.has_value());
    REQUIRE(loaded.error().Code() == woki::ErrorCode::FileNotFound);
    REQUIRE(loaded.error().Message().contains("woki.missing"));
}

TEST_CASE("Extension manager installs unpacked packages and refreshes records") {
    const fs::path root = MakeTempDir("install");
    const fs::path source = root / "source";
    WritePackage(source);

    FakeBackend backend;
    woki::ext::Manager manager(&backend);
    manager.SetRoots({
        .extensions = root / "extensions",
        .data = root / "ext-data",
        .cache = root / "cache" / "woki" / "ext",
    });

    auto installed = manager.InstallUnpacked(source);
    REQUIRE(installed.has_value());
    REQUIRE(manager.Records().size() == 1);
    REQUIRE(manager.Find("woki.hello") != nullptr);
    REQUIRE(manager.Find("woki.hello")->state == woki::ext::State::PermissionChecked);
}

TEST_CASE("Extension manager can own and swap runtime backends") {
    auto owned = woki::createScope<FakeBackend>();
    FakeBackend* owned_ptr = owned.get();
    woki::ext::Manager manager(std::move(owned));

    const fs::path root = MakeTempDir("owned_backend");
    WritePackage(root / "extensions" / "woki.hello");
    manager.SetRoots({
        .extensions = root / "extensions",
        .data = root / "ext-data",
        .cache = root / "cache" / "woki" / "ext",
    });

    REQUIRE(manager.Scan().has_value());
    REQUIRE(manager.Load("woki.hello").has_value());
    REQUIRE(owned_ptr->loads == 1);

    auto replacement = woki::createScope<FakeBackend>();
    FakeBackend* replacement_ptr = replacement.get();
    manager.SetBackend(std::move(replacement));

    manager.Find("woki.hello")->state = woki::ext::State::PermissionChecked;
    REQUIRE(manager.Load("woki.hello").has_value());
    REQUIRE(replacement_ptr->loads == 1);
}

TEST_CASE("Extension manager unloads active records before rescanning") {
    const fs::path root = MakeTempDir("rescan_cleanup");
    WritePackage(root / "extensions" / "woki.hello");
    FakeBackend backend;
    woki::ext::Manager manager(&backend);
    manager.SetRoots({root / "extensions", root / "ext-data", root / "cache"});

    REQUIRE(manager.Scan().has_value());
    REQUIRE(manager.Load("woki.hello").has_value());
    REQUIRE(manager.Scan().has_value());
    REQUIRE(backend.unloads == 1);
    REQUIRE(manager.Find("woki.hello")->state == woki::ext::State::PermissionChecked);
}

TEST_CASE("Extension manager LoadAll continues after failures and aggregates errors") {
    const fs::path root = MakeTempDir("load_all_failures");
    WritePackage(root / "extensions" / "woki.bad-one", "woki.bad-one");
    WritePackage(root / "extensions" / "woki.bad-two", "woki.bad-two");
    WritePackage(root / "extensions" / "woki.good", "woki.good");
    FakeBackend backend;
    backend.fail_load_prefix = "woki.bad";
    woki::ext::Manager manager(&backend);
    manager.SetRoots({root / "extensions", root / "ext-data", root / "cache"});

    REQUIRE(manager.Scan().has_value());
    auto loaded = manager.LoadAll();
    REQUIRE_FALSE(loaded.has_value());
    REQUIRE(loaded.error().Message().contains("woki.bad-one"));
    REQUIRE(loaded.error().Message().contains("woki.bad-two"));
    REQUIRE(manager.Find("woki.bad-one")->state == woki::ext::State::Failed);
    REQUIRE(manager.Find("woki.bad-two")->state == woki::ext::State::Failed);
    INFO(manager.Find("woki.good")->error);
    REQUIRE(manager.Find("woki.good")->state == woki::ext::State::Active);
    REQUIRE(backend.loads == 1);
    REQUIRE(backend.initializes == 1);
}
