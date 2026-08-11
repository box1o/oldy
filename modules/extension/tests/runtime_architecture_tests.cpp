#include <map>
#include <array>
#include <string>
#include <vector>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <filesystem>
#include <string_view>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/runtime.hpp>
#include <woki/ext/registry.hpp>
#include <woki/ext/internal/command_index.hpp>
#include <woki/ext/internal/event_service.hpp>
#include <woki/ext/internal/runtime_test_access.hpp>

namespace {

namespace fs = std::filesystem;

struct InstanceCalls {
    int created{};
    int initialized{};
    int ticks{};
    int events{};
    int commands{};
    int unloads{};
    int destroyed{};
    bool fail_create{};
    bool fail_init{};
    bool fail_tick{};
    bool fail_event{};
    bool emit_on_create{};
    bool emit_on_initialize{};
    bool emit_on_unload{};
    bool events_granted{};
    std::optional<woki::ErrorCode> command_error;
    woki::f64 delta{};
    woki::u32 event_type{};
    std::vector<woki::u8> payload;
    std::string command;
    std::vector<woki::u32> subscriptions;
    std::vector<std::string> named_subscriptions;
    std::string event_topic;
};

class TrackingInstance final : public woki::ext::RuntimeInstance {
public:
    TrackingInstance(InstanceCalls& calls, woki::ext::host::HostApi host)
        : calls_(calls),
          host_(std::move(host)) {}

    ~TrackingInstance() override {
        ++calls_.destroyed;
    }

    woki::Result<void> Initialize() override {
        ++calls_.initialized;
        if (calls_.emit_on_initialize) {
            const std::array<woki::u8, 1> payload{6};
            (void)host_.EmitEvent(woki::ext::ExtensionEventId(6), payload);
        }
        return calls_.fail_init ? woki::Err(woki::ErrorCode::InvalidState, "initialize failed") : woki::Ok();
    }

    woki::Result<void> Tick(woki::f64 delta) override {
        ++calls_.ticks;
        calls_.delta = delta;
        return calls_.fail_tick ? woki::Err(woki::ErrorCode::InvalidState, "tick failed") : woki::Ok();
    }

    woki::Result<void> DispatchEvent(woki::u32 type, std::span<const woki::u8> payload) override {
        ++calls_.events;
        calls_.event_type = type;
        calls_.payload.assign(payload.begin(), payload.end());
        return calls_.fail_event ? woki::Err(woki::ErrorCode::InvalidState, "event failed") : woki::Ok();
    }

    woki::Result<void> DispatchNamedEvent(std::string_view topic, std::span<const woki::u8> payload) override {
        ++calls_.events;
        calls_.event_topic = topic;
        calls_.payload.assign(payload.begin(), payload.end());
        return calls_.fail_event ? woki::Err(woki::ErrorCode::InvalidState, "event failed") : woki::Ok();
    }

    woki::Result<void> DispatchCommand(std::string_view command, std::span<const woki::u8> payload) override {
        ++calls_.commands;
        calls_.command = command;
        calls_.payload.assign(payload.begin(), payload.end());
        return calls_.command_error ? woki::Err(*calls_.command_error, "command failed") : woki::Ok();
    }

    void Unload() noexcept override {
        ++calls_.unloads;
        if (calls_.emit_on_unload) {
            const std::array<woki::u8, 1> payload{7};
            (void)host_.EmitEvent(woki::ext::ExtensionEventId(7), payload);
        }
    }

private:
    InstanceCalls& calls_;
    woki::ext::host::HostApi host_;
};

class TrackingEngine final : public woki::ext::RuntimeEngine {
public:
    explicit TrackingEngine(std::map<std::string, InstanceCalls>& calls)
        : calls_(calls) {}

    woki::Result<woki::scope<woki::ext::RuntimeInstance>> Create(const woki::ext::ExtensionPackage& package, woki::ext::host::HostApi host) override {
        auto& calls = calls_[package.Id()];
        ++calls.created;
        calls.events_granted = host.Allows(woki::ext::Permission::Events);
        if (calls.emit_on_create) {
            const std::array<woki::u8, 1> payload{5};
            (void)host.EmitEvent(woki::ext::ExtensionEventId(5), payload);
        }
        if (calls.fail_create)
            return woki::Err(woki::ErrorCode::InvalidState, "create failed");
        for (const auto event_type : calls.subscriptions) {
            if (auto subscribed = host.SubscribeEvent(event_type); !subscribed)
                return woki::Err(subscribed.error());
        }
        for (const auto& topic : calls.named_subscriptions) {
            if (auto subscribed = host.SubscribeNamedEvent(topic); !subscribed)
                return woki::Err(subscribed.error());
        }
        return woki::Ok(woki::scope<woki::ext::RuntimeInstance>(woki::createScope<TrackingInstance>(calls, std::move(host))));
    }

private:
    std::map<std::string, InstanceCalls>& calls_;
};

class DenyAllPolicy final : public woki::ext::CapabilityPolicy {
public:
    woki::Result<woki::ext::EffectiveCapabilities> Grant(const woki::ext::Manifest&) const override {
        return woki::Ok(woki::ext::EffectiveCapabilities{});
    }
};

class RejectPolicy final : public woki::ext::CapabilityPolicy {
public:
    woki::Result<woki::ext::EffectiveCapabilities> Grant(const woki::ext::Manifest&) const override {
        return woki::Err(woki::ErrorCode::FileAccessDenied, "blocked by host policy");
    }
};

class NullEngine final : public woki::ext::RuntimeEngine {
public:
    woki::Result<woki::scope<woki::ext::RuntimeInstance>> Create(const woki::ext::ExtensionPackage&, woki::ext::host::HostApi) override {
        return woki::Ok(woki::scope<woki::ext::RuntimeInstance>{});
    }
};

class RecordingBus final : public woki::ext::host::EventBus {
public:
    void Publish(const woki::ext::host::Event& event) override {
        events.push_back(event);
    }

    std::vector<woki::ext::host::Event> events;
};

[[nodiscard]] fs::path TempRoot(std::string_view name) {
    const fs::path root = fs::temp_directory_path() / "woki_runtime_architecture_tests" / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

[[nodiscard]] woki::ext::ExtensionPackage MakePackage(std::string id, std::vector<woki::ext::Permission> permissions = {}, std::vector<woki::ext::CommandContribution> commands = {}) {
    woki::ext::Manifest manifest;
    manifest.id = id;
    manifest.name = id;
    manifest.version = "1.0.0";
    manifest.requested_capabilities.permissions = std::move(permissions);
    manifest.commands = std::move(commands);
    const fs::path root = fs::temp_directory_path() / "woki_runtime_packages" / id;
    fs::create_directories(root);
    std::ofstream(root / "manifest.yaml") << "manifest";
    std::ofstream(root / "extension.wasm", std::ios::binary) << "wasm";
    auto package = woki::ext::ExtensionPackage::Create(id, std::move(manifest), {root, root / "manifest.yaml", root / "extension.wasm", root / "data", root / "config", root / "cache"});
    REQUIRE(package);
    return std::move(*package);
}

void WritePackage(const fs::path& directory, std::string_view id, std::string_view permissions = "[]", std::string_view commands = {}) {
    fs::create_directories(directory);
    std::ofstream manifest(directory / "manifest.yaml");
    REQUIRE(manifest.good());
    manifest << "id: " << id << "\nname: Test\nversion: 1.0.0\napiVersion: 1\nruntime:\n  wasm: extension.wasm\npermissions: " << permissions << "\n" << commands;
    std::ofstream wasm(directory / "extension.wasm", std::ios::binary);
    wasm.write("\0asm", 4);
}

[[nodiscard]] const woki::ext::ExtensionStatus* Status(const woki::ext::Runtime& runtime, std::string_view id) {
    for (const auto& status : runtime.Statuses())
        if (status.extension_id == id)
            return &status;
    return nullptr;
}

} // namespace

TEST_CASE("CommandIndex rejects duplicate ids atomically and clears records") {
    woki::ext::CommandIndex index;
    REQUIRE(index.Add("woki.first", {{"one", "One", ""}}));
    REQUIRE_FALSE(index.Add("woki.second", {{"two", "Two", ""}, {"one", "Duplicate", ""}}));
    REQUIRE(index.Records().size() == 1);
    REQUIRE(index.Find("two") == nullptr);
    REQUIRE_FALSE(index.Add("woki.third", {{"three", "Three", ""}, {"three", "Again", ""}}));
    REQUIRE(index.Records().size() == 1);
    REQUIRE(index.Find("one")->extension_id == "woki.first");
    index.Clear();
    REQUIRE(index.Records().empty());
}

TEST_CASE("Registry rejects duplicate IDs and scans installed failures deterministically") {
    const fs::path root = TempRoot("registry_installed");
    WritePackage(root / "extensions" / "woki.zed", "woki.zed");
    WritePackage(root / "extensions" / "woki.alpha", "woki.alpha");
    WritePackage(root / "extensions" / "wrong-folder", "woki.mismatch");
    fs::create_directories(root / "extensions" / "woki.invalid");
    std::ofstream(root / "extensions" / "woki.invalid" / "manifest.yaml") << "not: a manifest\n";

    woki::ext::Registry registry;
    REQUIRE(registry.Scan({root / "extensions", root / "data", root / "cache"}));
    REQUIRE(registry.Packages().size() == 2);
    REQUIRE(registry.Packages()[0].Id() == "woki.alpha");
    REQUIRE(registry.Packages()[1].Id() == "woki.zed");
    REQUIRE(registry.Failures().size() == 2);
    REQUIRE(registry.Find("woki.alpha") != nullptr);
    REQUIRE(registry.Find("missing") == nullptr);
    REQUIRE_FALSE(registry.Add(MakePackage("woki.alpha")));
    REQUIRE(registry.Packages().size() == 2);

    registry.Clear();
    REQUIRE(registry.Packages().empty());
    REQUIRE(registry.Failures().empty());
    const fs::path not_directory = root / "file";
    std::ofstream(not_directory) << "x";
    REQUIRE_FALSE(registry.Scan({not_directory, root / "data", root / "cache"}));
    REQUIRE(registry.Packages().empty());
}

TEST_CASE("Registry source scan accepts development folder names and isolates duplicate IDs") {
    const fs::path root = TempRoot("registry_source");
    WritePackage(root / "source" / "first", "woki.same");
    WritePackage(root / "source" / "second", "woki.same");
    WritePackage(root / "source" / "short", "woki.dev");

    woki::ext::Registry registry;
    REQUIRE(registry.ScanSource(root / "source", {root / "installed", root / "data", root / "cache"}));
    REQUIRE(registry.Packages().size() == 1);
    REQUIRE(registry.Find("woki.dev") != nullptr);
    REQUIRE(registry.Find("woki.dev")->Layout().install_root == root / "source" / "short");
    REQUIRE(registry.Find("woki.same") == nullptr);
    REQUIRE(registry.Find("woki.dev")->Layout().config_root == root / "ext-config" / "woki.dev");
    REQUIRE(registry.Failures().size() == 2);
    REQUIRE(registry.Failures().front().CandidateId() == "woki.same");
}

TEST_CASE("Registry source scan rejects aliases and overlap with runtime roots") {
    const fs::path root = TempRoot("registry_source_roots");
    const woki::ext::Roots roots{root / "installed", root / "data", root / "cache", root / "config"};
    fs::create_directories(root / "source");
    woki::ext::Registry registry;

    CHECK_FALSE(registry.ScanSource("relative-source", roots));
    CHECK_FALSE(registry.ScanSource(root / "data", roots));
    fs::create_directories(root / "installed");
    CHECK(registry.ScanSource(root / "installed", roots));

    std::error_code error;
    fs::create_directory_symlink(root / "source", root / "source-alias", error);
    if (!error)
        CHECK_FALSE(registry.ScanSource(root / "source-alias", roots));
}

TEST_CASE("Registry scan failure preserves the previous snapshot") {
    const fs::path root = TempRoot("registry_transaction");
    WritePackage(root / "extensions" / "woki.old", "woki.old");
    woki::ext::Registry registry;
    REQUIRE(registry.Scan({root / "extensions", root / "data", root / "cache"}));
    std::ofstream(root / "not-a-directory") << "x";
    REQUIRE_FALSE(registry.Scan({root / "not-a-directory", root / "other-data", root / "other-cache"}));
    REQUIRE(registry.Packages().size() == 1);
    REQUIRE(registry.Find("woki.old") != nullptr);
}

TEST_CASE("Runtime contains create, initialization, tick, event, and command failures") {
    std::map<std::string, InstanceCalls> calls;
    calls["woki.create"].fail_create = true;
    calls["woki.init"].fail_init = true;
    calls["woki.tick"].fail_tick = true;
    calls["woki.event"].fail_event = true;
    calls["woki.fatal-command"].command_error = woki::ErrorCode::InvalidState;
    calls["woki.guest-command"].command_error = woki::ErrorCode::FileAccessDenied;
    woki::ext::Runtime runtime(woki::createScope<TrackingEngine>(calls));

    REQUIRE_FALSE(runtime.Load(MakePackage("woki.create")));
    REQUIRE(calls["woki.create"].unloads == 0);
    REQUIRE(Status(runtime, "woki.create")->state == woki::ext::ExtensionState::Failed);
    REQUIRE_FALSE(runtime.Load(MakePackage("woki.init")));
    REQUIRE(calls["woki.init"].unloads == 1);
    REQUIRE(calls["woki.init"].destroyed == 1);

    for (const std::string id : {"woki.tick", "woki.event", "woki.fatal-command", "woki.guest-command"})
        REQUIRE(runtime.Load(MakePackage(id)));
    runtime.Tick(12.5);
    REQUIRE_FALSE(runtime.IsActive("woki.tick"));
    REQUIRE(calls["woki.tick"].unloads == 1);
    REQUIRE(Status(runtime, "woki.tick")->error == "tick failed");
    REQUIRE(Status(runtime, "woki.tick")->error_code == woki::ErrorCode::InvalidState);

    runtime.DispatchEvent("woki.event", 9, {std::array<woki::u8, 2>{3, 4}});
    REQUIRE_FALSE(runtime.IsActive("woki.event"));
    REQUIRE(calls["woki.event"].unloads == 1);
    REQUIRE_FALSE(runtime.DispatchCommand("woki.fatal-command", "run", {}));
    REQUIRE_FALSE(runtime.IsActive("woki.fatal-command"));
    REQUIRE(calls["woki.fatal-command"].unloads == 1);
    REQUIRE_FALSE(runtime.DispatchCommand("woki.guest-command", "run", {}));
    REQUIRE(runtime.IsActive("woki.guest-command"));
    REQUIRE(calls["woki.guest-command"].unloads == 0);
}

TEST_CASE("Runtime lifecycle is idempotent and unloads exactly once") {
    std::map<std::string, InstanceCalls> calls;
    {
        woki::ext::Runtime runtime(woki::createScope<TrackingEngine>(calls));
        const auto package = MakePackage("woki.life");
        REQUIRE(runtime.Load(package));
        REQUIRE_FALSE(runtime.Load(package));
        REQUIRE(calls["woki.life"].created == 1);
        runtime.Unload("missing");
        runtime.Unload("woki.life");
        runtime.Unload("woki.life");
        REQUIRE(calls["woki.life"].unloads == 1);
        REQUIRE(runtime.Statuses().empty());
        REQUIRE(runtime.Load(package));
        runtime.SetEngine(woki::createScope<TrackingEngine>(calls));
        REQUIRE(calls["woki.life"].unloads == 2);
        REQUIRE(runtime.Load(package));
    }
    REQUIRE(calls["woki.life"].unloads == 3);
    REQUIRE(calls["woki.life"].destroyed == 3);
}

TEST_CASE("Runtime rejects null engine instances") {
    woki::ext::Runtime runtime(woki::createScope<NullEngine>());
    REQUIRE_FALSE(runtime.Load(MakePackage("woki.null")));
    REQUIRE_FALSE(runtime.IsActive("woki.null"));
    REQUIRE(Status(runtime, "woki.null")->error.find("null instance") != std::string::npos);
}

TEST_CASE("Runtime validates package files before creating an instance") {
    std::map<std::string, InstanceCalls> calls;
    woki::ext::Runtime runtime(woki::createScope<TrackingEngine>(calls));
    auto package = MakePackage("woki.missing-runtime");
    fs::remove(package.Layout().wasm);
    auto loaded = runtime.Load(package);
    REQUIRE_FALSE(loaded);
    CHECK(loaded.error().Code() == woki::ErrorCode::FileNotFound);
    CHECK(calls["woki.missing-runtime"].created == 0);
    REQUIRE(Status(runtime, "woki.missing-runtime") != nullptr);
    CHECK(Status(runtime, "woki.missing-runtime")->state == woki::ext::ExtensionState::Failed);
}

TEST_CASE("ExtensionManager lazily activates commands and filters events by permission") {
    const fs::path root = TempRoot("manager_lazy");
    WritePackage(root / "extensions" / "woki.events", "woki.events", "[events]", "activation:\n  tick: true\n  events: [window.resized]\ncontributes:\n  commands:\n    - id: woki.events.run\n      title: Run\n");
    WritePackage(root / "extensions" / "woki.wild", "woki.wild", "[events]", "activation:\n  events: [window.resized]\n");
    WritePackage(root / "extensions" / "woki.unsubscribed", "woki.unsubscribed", "[events]", "activation:\n  events: [window.resized]\n");
    WritePackage(root / "extensions" / "woki.quiet", "woki.quiet", "[]", "activation:\n  tick: true\ncontributes:\n  commands:\n    - id: woki.quiet.run\n      title: Run Quietly\n");
    std::map<std::string, InstanceCalls> calls;
    const auto resize = static_cast<woki::u32>(woki::ext::ApplicationEventType::WindowResized);
    calls["woki.events"].subscriptions = {resize};
    calls["woki.events"].named_subscriptions = {"woki.source.ready"};
    calls["woki.wild"].subscriptions = {woki::ext::host::kWildcardEventType};
    auto manager = woki::ext::internal::ExtensionManagerAccess::Create(woki::createScope<TrackingEngine>(calls));
    manager.SetRoots({root / "extensions", root / "data", root / "cache"});
    REQUIRE(manager.Scan());
    REQUIRE(manager.Statuses().empty());

    const std::array<woki::u8, 2> payload{8, 9};
    REQUIRE(manager.ExecuteCommand("woki.events.run", payload));
    REQUIRE(calls["woki.events"].created == 1);
    REQUIRE(calls["woki.events"].command == "woki.events.run");
    REQUIRE(calls["woki.events"].payload == std::vector<woki::u8>{8, 9});
    REQUIRE_FALSE(manager.ExecuteCommand("missing"));

    REQUIRE(manager.Load("woki.quiet"));
    manager.DispatchEvent(resize, payload);
    REQUIRE(calls["woki.events"].events == 1);
    REQUIRE(calls["woki.events"].event_type == resize);
    REQUIRE(calls["woki.wild"].events == 1);
    REQUIRE(calls["woki.unsubscribed"].events == 0);
    REQUIRE(calls["woki.quiet"].events == 0);
    manager.DispatchEvent(static_cast<woki::u32>(woki::ext::ApplicationEventType::WindowMoved), payload);
    REQUIRE(calls["woki.events"].events == 1);
    REQUIRE(calls["woki.wild"].events == 1);
    manager.Tick(16.0);
    REQUIRE(calls["woki.events"].ticks == 1);
    REQUIRE(calls["woki.quiet"].ticks == 1);
    manager.DispatchNamedEvent("woki.source.ready", payload);
    REQUIRE(calls["woki.events"].events == 2);
    REQUIRE(calls["woki.events"].event_topic == "woki.source.ready");
}

TEST_CASE("ExtensionManager routes deprecated numeric extension events only to active subscribers") {
    const fs::path root = TempRoot("manager_numeric_compatibility");
    WritePackage(root / "extensions" / "woki.numeric", "woki.numeric", "[events]");
    std::map<std::string, InstanceCalls> calls;
    const woki::u32 numeric_type = woki::ext::ExtensionEventId(42);
    calls["woki.numeric"].subscriptions = {numeric_type};
    auto manager = woki::ext::internal::ExtensionManagerAccess::Create(woki::createScope<TrackingEngine>(calls));
    manager.SetRoots({root / "extensions", root / "data", root / "cache"});
    REQUIRE(manager.Scan());

    manager.DispatchEvent(numeric_type, {});
    CHECK(calls["woki.numeric"].created == 0);
    REQUIRE(manager.Load("woki.numeric"));
    manager.DispatchEvent(numeric_type, {});
    CHECK(calls["woki.numeric"].events == 1);
    CHECK(calls["woki.numeric"].event_type == numeric_type);
    manager.DispatchEvent(0x7fffffffu, {});
    CHECK(calls["woki.numeric"].events == 1);
}

TEST_CASE("EventService drains follow-up events and observes bus replacement") {
    woki::ext::host::EventService service;
    RecordingBus replacement;

    class ReplacingBus final : public woki::ext::host::EventBus {
    public:
        ReplacingBus(woki::ext::host::EventService& service, woki::ext::host::EventBus& replacement)
            : service_(service),
              replacement_(replacement) {}

        void Publish(const woki::ext::host::Event& event) override {
            events.push_back(event);
            service_.SetBus(&replacement_);
            REQUIRE(service_.Enqueue({2, {}, {}, {}}));
            service_.Drain();
        }

        std::vector<woki::ext::host::Event> events;

    private:
        woki::ext::host::EventService& service_;
        woki::ext::host::EventBus& replacement_;
    } first(service, replacement);

    service.SetBus(&first);
    REQUIRE(service.Enqueue({1, {}, {}, {}}));
    service.Drain();
    REQUIRE(first.events.size() == 1);
    CHECK(first.events.front().type == 1);
    REQUIRE(replacement.events.size() == 1);
    CHECK(replacement.events.front().type == 2);
}

TEST_CASE("EventService restores drain state after publisher exceptions") {
    woki::ext::host::EventService service;

    class ThrowingBus final : public woki::ext::host::EventBus {
    public:
        explicit ThrowingBus(woki::ext::host::EventService& service)
            : service_(service) {}

        void Publish(const woki::ext::host::Event& event) override {
            events.push_back(event);
            if (events.size() == 1) {
                REQUIRE(service_.Enqueue({2, {}, {}, {}}));
                throw std::runtime_error("publish failed");
            }
        }

        std::vector<woki::ext::host::Event> events;

    private:
        woki::ext::host::EventService& service_;
    } bus(service);

    service.SetBus(&bus);
    REQUIRE(service.Enqueue({1, {}, {}, {}}));
    CHECK_THROWS_AS(service.Drain(), std::runtime_error);
    service.Drain();
    REQUIRE(bus.events.size() == 2);
    CHECK(bus.events.back().type == 2);
}

TEST_CASE("ExtensionManager LoadAll aggregates failures and rescan unloads active instances") {
    const fs::path root = TempRoot("manager_load_all");
    WritePackage(root / "extensions" / "woki.bad-a", "woki.bad-a");
    WritePackage(root / "extensions" / "woki.bad-b", "woki.bad-b");
    WritePackage(root / "extensions" / "woki.good", "woki.good");
    std::map<std::string, InstanceCalls> calls;
    calls["woki.bad-a"].fail_create = true;
    calls["woki.bad-b"].fail_init = true;
    auto manager = woki::ext::internal::ExtensionManagerAccess::Create(woki::createScope<TrackingEngine>(calls));
    manager.SetRoots({root / "extensions", root / "data", root / "cache"});
    REQUIRE(manager.Scan());
    const auto loaded = manager.LoadAll();
    REQUIRE_FALSE(loaded);
    REQUIRE(loaded.error().Message().contains("woki.bad-a"));
    REQUIRE(loaded.error().Message().contains("woki.bad-b"));
    REQUIRE(calls["woki.good"].initialized == 1);
    const auto retried = manager.LoadAll();
    REQUIRE_FALSE(retried);
    REQUIRE(retried.error().Message().contains("woki.bad-a"));
    REQUIRE(retried.error().Message().contains("woki.bad-b"));
    REQUIRE(calls["woki.good"].initialized == 1);
    REQUIRE(manager.Statuses().size() == 3);

    REQUIRE(manager.Scan());
    REQUIRE(calls["woki.good"].unloads == 1);
    REQUIRE(manager.Statuses().empty());
    REQUIRE(manager.Packages().size() == 3);

    const fs::path source = root / "source";
    WritePackage(source / "dev", "woki.dev");
    REQUIRE(manager.ScanSource(source));
    REQUIRE(manager.Packages().size() == 1);
    REQUIRE(manager.Find("woki.dev") != nullptr);
}

TEST_CASE("ExtensionManager activates startup declarations and applies effective capability policy") {
    const fs::path root = TempRoot("manager_startup_policy");
    WritePackage(root / "extensions" / "woki.start", "woki.start", "[events]", "activation:\n  startup: true\n");
    WritePackage(root / "extensions" / "woki.lazy", "woki.lazy", "[events]");
    std::map<std::string, InstanceCalls> calls;
    auto manager = woki::ext::internal::ExtensionManagerAccess::Create(woki::createScope<TrackingEngine>(calls));
    manager.SetCapabilityPolicy(woki::createScope<DenyAllPolicy>());
    manager.SetRoots({root / "extensions", root / "data", root / "cache"});
    REQUIRE(manager.Scan());
    REQUIRE(manager.ActivateStartup());
    CHECK(calls["woki.start"].created == 1);
    CHECK_FALSE(calls["woki.start"].events_granted);
    CHECK(calls["woki.lazy"].created == 0);
}

TEST_CASE("ExtensionManager records policy rejections as failed statuses") {
    const fs::path root = TempRoot("manager_policy_rejection");
    WritePackage(root / "extensions" / "woki.denied-a", "woki.denied-a");
    WritePackage(root / "extensions" / "woki.denied-b", "woki.denied-b");
    std::map<std::string, InstanceCalls> calls;
    auto manager = woki::ext::internal::ExtensionManagerAccess::Create(woki::createScope<TrackingEngine>(calls));
    manager.SetCapabilityPolicy(woki::createScope<RejectPolicy>());
    manager.SetRoots({root / "extensions", root / "data", root / "cache"});
    REQUIRE(manager.Scan());

    REQUIRE_FALSE(manager.Load("woki.denied-a"));
    REQUIRE(manager.Statuses().size() == 1);
    CHECK(manager.Statuses().front().extension_id == "woki.denied-a");
    CHECK(manager.Statuses().front().state == woki::ext::ExtensionState::Failed);
    CHECK(manager.Statuses().front().error_code == woki::ErrorCode::FileAccessDenied);

    REQUIRE_FALSE(manager.LoadAll());
    REQUIRE(manager.Statuses().size() == 2);
    CHECK(std::ranges::all_of(manager.Statuses(), [](const auto& status) { return status.state == woki::ext::ExtensionState::Failed && status.error == "blocked by host policy"; }));
}

TEST_CASE("ExtensionManager scan commits registry and command index together") {
    const fs::path root = TempRoot("manager_transaction");
    WritePackage(root / "extensions" / "woki.old", "woki.old", "[]", "contributes:\n  commands:\n    - id: woki.old.run\n      title: Old\n");
    std::map<std::string, InstanceCalls> calls;
    auto manager = woki::ext::internal::ExtensionManagerAccess::Create(woki::createScope<TrackingEngine>(calls));
    manager.SetRoots({root / "extensions", root / "data", root / "cache"});
    REQUIRE(manager.Scan());
    const auto loaded = manager.Load("woki.old");
    if (!loaded)
        FAIL(loaded.error().Message());
    REQUIRE(loaded);

    fs::remove_all(root / "extensions");
    std::ofstream(root / "extensions") << "not a directory";
    REQUIRE_FALSE(manager.Scan());
    REQUIRE(manager.Find("woki.old") != nullptr);
    REQUIRE(std::ranges::any_of(manager.Commands(), [](const woki::ext::CommandRecord& record) { return record.command.id == "woki.old.run"; }));
    REQUIRE(manager.Statuses().size() == 1);
    REQUIRE(calls["woki.old"].unloads == 0);
}

TEST_CASE("ExtensionManager drains events emitted by every explicit unload boundary") {
    const fs::path root = TempRoot("manager_unload_events");
    WritePackage(root / "extensions" / "woki.events", "woki.events", "[events]");
    std::map<std::string, InstanceCalls> calls;
    calls["woki.events"].emit_on_unload = true;
    RecordingBus bus;
    auto manager = woki::ext::internal::ExtensionManagerAccess::Create(woki::createScope<TrackingEngine>(calls));
    manager.SetEventBus(&bus);
    manager.SetRoots({root / "extensions", root / "data", root / "cache"});

    const auto activate = [&] {
        REQUIRE(manager.Scan());
        REQUIRE(manager.Load("woki.events"));
    };

    SECTION("Unload") {
        activate();
        manager.Unload("woki.events");
    }
    SECTION("UnloadAll") {
        activate();
        manager.UnloadAll();
    }
    SECTION("SetRoots") {
        activate();
        manager.SetRoots({root / "other-extensions", root / "other-data", root / "other-cache"});
    }
    SECTION("Scan") {
        activate();
        REQUIRE(manager.Scan());
    }
    SECTION("ScanSource") {
        activate();
        const fs::path source = root / "source";
        WritePackage(source / "events", "woki.events", "[events]");
        REQUIRE(manager.ScanSource(source));
    }

    REQUIRE(bus.events.size() == 1);
    CHECK(bus.events.front().type == (woki::ext::host::kExtensionEventNamespace | 7));
    CHECK(bus.events.front().origin.extension_id == "woki.events");
}

TEST_CASE("ExtensionManager discards failed activation events and drains destruction unload events") {
    const fs::path root = TempRoot("manager_activation_event_transactions");
    WritePackage(root / "extensions" / "woki.events", "woki.events", "[events]");
    std::map<std::string, InstanceCalls> calls;
    RecordingBus bus;

    calls["woki.events"].emit_on_create = true;
    calls["woki.events"].emit_on_initialize = true;
    calls["woki.events"].fail_create = true;
    calls["woki.events"].fail_init = true;
    {
        auto manager = woki::ext::internal::ExtensionManagerAccess::Create(woki::createScope<TrackingEngine>(calls));
        manager.SetEventBus(&bus);
        manager.SetRoots({root / "extensions", root / "data", root / "cache"});
        REQUIRE(manager.Scan());
        CHECK_FALSE(manager.Load("woki.events"));
        CHECK(bus.events.empty());
        calls["woki.events"].fail_create = false;
        CHECK_FALSE(manager.Load("woki.events"));
    }
    CHECK(bus.events.empty());

    calls["woki.events"].fail_init = false;
    calls["woki.events"].emit_on_create = false;
    calls["woki.events"].emit_on_initialize = false;
    calls["woki.events"].emit_on_unload = true;
    {
        auto manager = woki::ext::internal::ExtensionManagerAccess::Create(woki::createScope<TrackingEngine>(calls));
        manager.SetEventBus(&bus);
        manager.SetRoots({root / "extensions", root / "data", root / "cache"});
        REQUIRE(manager.Scan());
        REQUIRE(manager.Load("woki.events"));
    }
    REQUIRE(bus.events.size() == 1);
    CHECK(bus.events.front().type == woki::ext::ExtensionEventId(7));
}

TEST_CASE("ExtensionManager reports a committed install without mutating the active catalog") {
    const fs::path root = TempRoot("manager_install_scan");
    WritePackage(root / "extensions" / "woki.old", "woki.old");
    const fs::path source = root / "incoming";
    WritePackage(source, "woki.new");

    std::map<std::string, InstanceCalls> calls;
    auto manager = woki::ext::internal::ExtensionManagerAccess::Create(woki::createScope<TrackingEngine>(calls));
    manager.SetRoots({root / "extensions", root / "data", root / "cache"});
    REQUIRE(manager.Scan());
    REQUIRE(manager.Load("woki.old"));

    const auto installed = manager.InstallUnpacked(source);
    REQUIRE(installed);
    CHECK(fs::equivalent(installed->install_root, root / "extensions" / "woki.new"));
    CHECK(fs::is_regular_file(installed->manifest));
    CHECK(manager.Find("woki.old") != nullptr);
    CHECK(manager.Find("woki.new") == nullptr);
    CHECK(manager.Statuses().size() == 1);
    CHECK(calls["woki.old"].unloads == 0);

    REQUIRE(manager.Scan());
    CHECK(manager.Find("woki.new") != nullptr);
    CHECK(manager.Statuses().empty());
    CHECK(calls["woki.old"].unloads == 1);
}

TEST_CASE("ExtensionManager install completes only missing roots") {
    const fs::path root = TempRoot("manager_partial_install_roots");
    const fs::path source = root / "incoming";
    const fs::path extensions = root / "chosen-extensions";
    WritePackage(source, "woki.partial");

    std::map<std::string, InstanceCalls> calls;
    auto manager = woki::ext::internal::ExtensionManagerAccess::Create(woki::createScope<TrackingEngine>(calls));
    manager.SetRoots({.extensions = extensions, .data = {}, .cache = {}, .config = {}});
    const auto installed = manager.InstallUnpacked(source);

    REQUIRE(installed);
    CHECK(fs::equivalent(installed->install_root, extensions / "woki.partial"));
}

TEST_CASE("Web bridge uses session handles, external config roots, and caller-owned errors") {
    std::ifstream input(fs::path(WOKI_EXTENSION_SOURCE_DIR) / "web" / "woki_ext.js");
    REQUIRE(input.good());
    const std::string source{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    REQUIRE(source.find("WokiExt.instances.set(handle, record)") != std::string::npos);
    REQUIRE(source.find("configPath: UTF8ToString(configPathPtr)") != std::string::npos);
    REQUIRE(source.find("safePath(record.configPath, key)") != std::string::npos);
    REQUIRE(source.find("stringToNewUTF8") == std::string::npos);
    REQUIRE(source.find("errors: new Map()") != std::string::npos);
    REQUIRE(source.find("WokiExt.errors.get(UTF8ToString(idPtr))") != std::string::npos);
    REQUIRE(source.find("FS.lookupPath(path, { follow: false })") != std::string::npos);
    REQUIRE(source.find("WokiExt.hostBytes(payloadPtr, payloadLen)") != std::string::npos);
    REQUIRE(source.find("WokiExt.hostBytes(wasmBytesPtr, wasmBytesLen).slice()") != std::string::npos);
    REQUIRE(source.find("FS.readFile(UTF8ToString(wasmPathPtr))") == std::string::npos);
    REQUIRE(source.find("cleanupError && !primaryFailed") != std::string::npos);
    REQUIRE(source.find("ext_free failed:") != std::string::npos);
    REQUIRE(source.find("_woki_web_host_event_subscribe(record.hostHandle, eventType)") != std::string::npos);
    REQUIRE(source.find("_woki_web_host_event_emit(record.hostHandle, eventType, hostPayload, payloadLen)") != std::string::npos);
    REQUIRE(source.find("_woki_web_host_event_subscribe_named(record.hostHandle, hostName, nameLen)") != std::string::npos);
    REQUIRE(source.find("_woki_web_host_event_emit_named(record.hostHandle, hostName, nameLen, hostPayload, payloadLen)") != std::string::npos);
    REQUIRE(source.find("exports.ext_on_event_named(nameGuestPtr, nameLen, payloadGuestPtr, payloadLen)") != std::string::npos);
    REQUIRE(source.find("hostHandle,") != std::string::npos);
    REQUIRE(source.find("HEAPU8.set(payload, hostPayload)") != std::string::npos);
    REQUIRE(source.find("host_event_subscribe = (_eventType)") == std::string::npos);
}
