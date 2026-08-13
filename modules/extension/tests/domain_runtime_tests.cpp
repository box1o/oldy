#include <array>
#include <fstream>
#include <filesystem>
#include <type_traits>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/runtime.hpp>
#include <woki/ext/registry.hpp>
#include <woki/ext/internal/command_index.hpp>

namespace {

struct Calls {
    int creates{};
    int initializes{};
    int ticks{};
    int events{};
    int commands{};
    int unloads{};
    bool fail_tick{};
    bool fail_initialize{};
    bool fail_command{};
};

class FakeInstance final : public woki::ext::RuntimeInstance {
public:
    explicit FakeInstance(Calls& calls)
        : calls_(calls) {}

    woki::Result<void> Initialize() override {
        ++calls_.initializes;
        return calls_.fail_initialize ? woki::Err(woki::ErrorCode::InvalidState, "init failed") : woki::Ok();
    }

    woki::Result<void> Tick(woki::f64) override {
        ++calls_.ticks;
        return calls_.fail_tick ? woki::Err(woki::ErrorCode::InvalidState, "tick failed") : woki::Ok();
    }

    woki::Result<void> DispatchEvent(woki::u32, std::span<const woki::u8>) override {
        ++calls_.events;
        return woki::Ok();
    }

    woki::Result<void> DispatchCommand(std::string_view, std::span<const woki::u8>) override {
        ++calls_.commands;
        return calls_.fail_command ? woki::Err(woki::ErrorCode::InvalidState, "command failed") : woki::Ok();
    }

    void Unload() noexcept override {
        if (!unloaded_) {
            ++calls_.unloads;
            unloaded_ = true;
        }
    }

private:
    Calls& calls_;
    bool unloaded_{};
};

class FakeEngine final : public woki::ext::RuntimeEngine {
public:
    explicit FakeEngine(Calls& calls)
        : calls_(calls) {}

    woki::Result<woki::scope<woki::ext::RuntimeInstance>> Create(const woki::ext::ExtensionPackage&, woki::ext::host::HostApi) override {
        ++calls_.creates;
        return woki::Ok(woki::scope<woki::ext::RuntimeInstance>(woki::createScope<FakeInstance>(calls_)));
    }

private:
    Calls& calls_;
};

[[nodiscard]] woki::ext::ExtensionPackage MakePackage(std::string id = "woki.test") {
    woki::ext::Manifest manifest;
    manifest.id = id;
    manifest.name = "Test";
    manifest.version = "1.0.0";
    manifest.api_version = 1;
    manifest.requested_capabilities.permissions = {woki::ext::Permission::Events};
    const auto root = std::filesystem::temp_directory_path() / "woki_domain_runtime_packages" / id;
    woki::ext::PackageLayout layout;
    layout.install_root = root;
    layout.manifest = root / "manifest.yaml";
    layout.wasm = root / "extension.wasm";
    layout.data_root = std::filesystem::temp_directory_path() / id / "data";
    layout.config_root = std::filesystem::temp_directory_path() / id / "config";
    layout.cache_root = std::filesystem::temp_directory_path() / id / "cache";
    std::filesystem::create_directories(layout.install_root);
    std::ofstream(layout.manifest) << "manifest";
    std::ofstream(layout.wasm, std::ios::binary) << "wasm";
    auto package = woki::ext::ExtensionPackage::Create(id, std::move(manifest), std::move(layout));
    REQUIRE(package);
    return std::move(*package);
}

} // namespace

static_assert(std::is_same_v<decltype(std::declval<const woki::ext::Registry&>().Packages()), std::span<const woki::ext::ExtensionPackage>>);

TEST_CASE("Extension registry rejects duplicate package ids") {
    woki::ext::Registry registry;
    REQUIRE(registry.Add(MakePackage()).has_value());
    auto duplicate = registry.Add(MakePackage());
    REQUIRE_FALSE(duplicate.has_value());
    REQUIRE(registry.Packages().size() == 1);
}

TEST_CASE("Runtime owns and isolates extension sessions") {
    Calls calls;
    woki::ext::Runtime runtime(woki::createScope<FakeEngine>(calls));
    auto first = MakePackage("woki.first");
    auto second = MakePackage("woki.second");
    REQUIRE(runtime.Load(first).has_value());
    REQUIRE(runtime.Load(second).has_value());
    REQUIRE(calls.creates == 2);
    REQUIRE(calls.initializes == 2);
    runtime.DispatchEvent(first.Id(), 4, {});
    REQUIRE(calls.events == 1);
    REQUIRE(runtime.DispatchCommand(second.Id(), "woki.second.run", {}).has_value());
    REQUIRE(calls.commands == 1);
    calls.fail_tick = true;
    runtime.Tick(16.0);
    REQUIRE(runtime.Statuses().size() == 2);
    REQUIRE(runtime.Statuses()[0].state == woki::ext::ExtensionState::Failed);
    REQUIRE(runtime.Statuses()[1].state == woki::ext::ExtensionState::Failed);
    REQUIRE(calls.unloads == 2);
}

TEST_CASE("Runtime contains initialization and command failures") {
    Calls calls;
    woki::ext::Runtime runtime(woki::createScope<FakeEngine>(calls));
    auto package = MakePackage();
    calls.fail_initialize = true;
    REQUIRE_FALSE(runtime.Load(package).has_value());
    REQUIRE(calls.unloads == 1);
    REQUIRE(runtime.Statuses().front().state == woki::ext::ExtensionState::Failed);
    calls.fail_initialize = false;
    REQUIRE(runtime.Load(package).has_value());
    calls.fail_command = true;
    REQUIRE_FALSE(runtime.DispatchCommand(package.Id(), "woki.test.run", {}).has_value());
    REQUIRE_FALSE(runtime.IsActive(package.Id()));
    REQUIRE(calls.unloads == 2);
}

TEST_CASE("Runtime reports a missing engine without mutating packages") {
    woki::ext::Runtime runtime;
    const auto package = MakePackage();
    REQUIRE_FALSE(runtime.Load(package).has_value());
    REQUIRE(runtime.Statuses().size() == 1);
    REQUIRE(runtime.Statuses().front().error.find("engine") != std::string::npos);
    REQUIRE(package.Id() == "woki.test");
}

TEST_CASE("Host API owns stable context independent of package storage") {
    const auto root = std::filesystem::temp_directory_path() / "woki_host_context_test";
    std::filesystem::remove_all(root);
    woki::ext::host::HostApi host({"woki.test", {woki::ext::Permission::Storage, woki::ext::Permission::Config, woki::ext::Permission::Paths}, root / "data", root / "config", root / "cache", {}});
    const std::array<woki::u8, 3> bytes{1, 2, 3};
    REQUIRE(host.WriteFile("nested/value.bin", bytes).has_value());
    REQUIRE(host.ReadFile("nested/value.bin")->size() == bytes.size());
    REQUIRE(host.WriteConfig("theme", "dark").has_value());
    REQUIRE(*host.ReadConfig("theme") == "dark");
    REQUIRE(host.DataPath().has_value());
    REQUIRE(host.CachePath().has_value());
}

TEST_CASE("Host API enforces permissions and path containment") {
    const auto root = std::filesystem::temp_directory_path() / "woki_host_denied_test";
    woki::ext::host::HostApi host({"woki.test", {}, root / "data", root / "config", root / "cache", {}});
    REQUIRE_FALSE(host.DataPath().has_value());
    REQUIRE_FALSE(host.ReadFile("value.bin").has_value());
    woki::ext::host::HostApi storage({"woki.test", {woki::ext::Permission::Storage}, root / "data", root / "config", root / "cache", {}});
    REQUIRE_FALSE(storage.ReadFile("../escape").has_value());
}

TEST_CASE("Command index rejects duplicate command ids") {
    woki::ext::CommandIndex index;
    const std::vector<woki::ext::CommandContribution> commands{{"woki.test.run", "Run", {}}};
    REQUIRE(index.Add("woki.test", commands).has_value());
    REQUIRE_FALSE(index.Add("woki.other", commands).has_value());
    REQUIRE(index.Find("woki.test.run")->extension_id == "woki.test");
}

TEST_CASE("Registry separates discovery failures from valid packages") {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "woki_registry_domain_test";
    fs::remove_all(root);
    fs::create_directories(root / "extensions" / "woki.good");
    fs::create_directories(root / "extensions" / "woki.bad");
    {
        std::ofstream manifest(root / "extensions" / "woki.good" / "manifest.yaml");
        manifest << "$schema: https://schemas.woki.dev/extension.manifest/v1.schema.json\nid: woki.good\nname: Good\nversion: 1.0.0\napiVersion: 1\nruntime:\n  wasm: extension.wasm\npermissions: []\n";
    }
    std::ofstream(root / "extensions" / "woki.good" / "extension.wasm", std::ios::binary).write("\0asm", 4);
    {
        std::ofstream manifest(root / "extensions" / "woki.bad" / "manifest.yaml");
        manifest << "$schema: https://schemas.woki.dev/extension.manifest/v1.schema.json\nid: woki.bad\nname: Bad\nversion: 1.0.0\napiVersion: 1\nruntime:\n  wasm: missing.wasm\npermissions: []\n";
    }
    woki::ext::Registry registry;
    REQUIRE(registry.Scan({root / "extensions", root / "data", root / "cache"}).has_value());
    if (!registry.Failures().empty())
        INFO(registry.Failures().front().Cause().Message());
    REQUIRE(registry.Packages().size() == 1);
    REQUIRE(registry.Packages().front().Id() == "woki.good");
    REQUIRE(registry.Failures().size() == 1);
    REQUIRE(registry.Failures().front().CandidateId() == "woki.bad");
}
