#include <array>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <filesystem>
#include <string_view>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/limits.hpp>
#include <woki/ext/host/api.hpp>
#include <woki/ext/host/cabi.hpp>
#include <woki/ext/internal/event_service.hpp>

namespace {

namespace fs = std::filesystem;
using woki::ext::Permission;
using woki::ext::host::HostApi;

[[nodiscard]] fs::path TempRoot(std::string_view name) {
    const fs::path root = fs::temp_directory_path() / "woki_host_api_cabi_tests" / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

[[nodiscard]] HostApi MakeHost(const fs::path& root, std::vector<Permission> permissions) {
    return HostApi({"woki.test", std::move(permissions), root / "data", root / "config", root / "cache", {}, {}});
}

} // namespace

TEST_CASE("HostApi permission gates each capability independently") {
    const fs::path root = TempRoot("permission_matrix");
    const std::array<woki::u8, 1> byte{7};

    auto none = MakeHost(root, {});
    REQUIRE_FALSE(none.Allows(Permission::Storage));
    REQUIRE(none.DataPath().error().Code() == woki::ErrorCode::FileAccessDenied);
    REQUIRE(none.CachePath().error().Code() == woki::ErrorCode::FileAccessDenied);
    REQUIRE(none.ReadFile("value").error().Code() == woki::ErrorCode::FileAccessDenied);
    REQUIRE(none.WriteFile("value", byte).error().Code() == woki::ErrorCode::FileAccessDenied);
    REQUIRE(none.AppendFile("value", byte).error().Code() == woki::ErrorCode::FileAccessDenied);
    REQUIRE(none.ReadConfig("key").error().Code() == woki::ErrorCode::FileAccessDenied);
    REQUIRE(none.WriteConfig("key", "value").error().Code() == woki::ErrorCode::FileAccessDenied);

    auto paths = MakeHost(root, {Permission::Paths});
    REQUIRE(paths.DataPath() == root / "data");
    REQUIRE(paths.CachePath() == root / "cache");
    REQUIRE_FALSE(paths.WriteFile("value", byte));

    auto storage = MakeHost(root, {Permission::Storage});
    REQUIRE(storage.WriteFile("nested/value", byte));
    REQUIRE(storage.ReadFile("nested/value")->front() == 7);
    REQUIRE_FALSE(storage.DataPath());

    auto config = MakeHost(root, {Permission::Config});
    REQUIRE(config.WriteConfig("theme.name", "dark"));
    REQUIRE(*config.ReadConfig("theme.name") == "dark");
    REQUIRE_FALSE(config.WriteFile("value", byte));
}

TEST_CASE("HostApi enforces path, symlink, and size boundaries without corrupting data") {
    const fs::path root = TempRoot("boundaries");
    auto host = MakeHost(root, {Permission::Storage, Permission::Config});
    const std::array<woki::u8, 1> byte{1};

    for (const fs::path& path : {fs::path{}, fs::path{"."}, fs::path{"../escape"}, fs::path{"a/../b"}, fs::path{"a\\b"}, fs::path{"/absolute"}}) {
        const auto result = host.WriteFile(path, byte);
        REQUIRE_FALSE(result);
        REQUIRE(result.error().Code() == woki::ErrorCode::InvalidArgument);
    }
    for (const std::string& key : {std::string{}, std::string{"."}, std::string{".."}, std::string{"a/b"}, std::string(woki::ext::limits::kMaxConfigKeyBytes + 1, 'k')}) {
        const auto result = host.WriteConfig(key, "value");
        REQUIRE_FALSE(result);
        REQUIRE(result.error().Code() == woki::ErrorCode::InvalidArgument);
    }

    const std::string max_key(woki::ext::limits::kMaxConfigKeyBytes, 'k');
    const std::string max_value(woki::ext::limits::kMaxConfigValueBytes, 'v');
    REQUIRE(host.WriteConfig(max_key, max_value));
    REQUIRE(*host.ReadConfig(max_key) == max_value);
    REQUIRE(host.WriteConfig("stable", "old"));
    REQUIRE_FALSE(host.WriteConfig("stable", std::string(woki::ext::limits::kMaxConfigValueBytes + 1, 'x')));
    REQUIRE(*host.ReadConfig("stable") == "old");

    fs::create_directories(root / "data");
    std::ofstream(root / "data" / "full", std::ios::binary);
    fs::resize_file(root / "data" / "full", woki::ext::limits::kMaxFileBytes);
    REQUIRE_FALSE(host.AppendFile("full", byte));
    REQUIRE(fs::file_size(root / "data" / "full") == woki::ext::limits::kMaxFileBytes);
    std::ofstream(root / "data" / "oversized", std::ios::binary).close();
    fs::resize_file(root / "data" / "oversized", woki::ext::limits::kMaxFileBytes + 1);
    REQUIRE_FALSE(host.ReadFile("oversized"));

    std::error_code error;
    fs::create_directory_symlink(root, root / "data" / "escape", error);
    if (!error) {
        const auto escaped = host.WriteFile("escape/outside", byte);
        REQUIRE_FALSE(escaped);
        REQUIRE(escaped.error().Code() == woki::ErrorCode::FileAccessDenied);
        REQUIRE_FALSE(fs::exists(root / "outside"));
    }

    error.clear();
    fs::create_symlink(root / "outside-file", root / "data" / "linked-file", error);
    if (!error) {
        const auto linked = host.WriteFile("linked-file", byte);
        REQUIRE_FALSE(linked);
        REQUIRE(linked.error().Code() == woki::ErrorCode::FileAccessDenied);
    }
}

TEST_CASE("HostApi serializes concurrent appends at the actual byte limit") {
    const fs::path root = TempRoot("concurrent_append_limit");
    auto host = MakeHost(root, {Permission::Storage});
    fs::create_directories(root / "data");
    std::ofstream(root / "data" / "state", std::ios::binary).close();
    fs::resize_file(root / "data" / "state", woki::ext::limits::kMaxFileBytes - 1);
    const std::array<woki::u8, 1> byte{'x'};
    std::array<bool, 2> succeeded{};
    std::thread first([&] { succeeded[0] = host.AppendFile("state", byte).has_value(); });
    std::thread second([&] { succeeded[1] = host.AppendFile("state", byte).has_value(); });
    first.join();
    second.join();

    REQUIRE(succeeded[0] != succeeded[1]);
    REQUIRE(fs::file_size(root / "data" / "state") == woki::ext::limits::kMaxFileBytes);
}

TEST_CASE("HostApi rejects symlink aliases in a storage root component") {
    const fs::path root = TempRoot("root_alias");
    fs::create_directories(root / "real-data");
    std::error_code error;
    fs::create_directory_symlink(root / "real-data", root / "data", error);
    if (error)
        SKIP("Directory symlinks are unavailable");
    auto host = MakeHost(root, {Permission::Storage});
    const std::array<woki::u8, 1> byte{1};
    const auto result = host.WriteFile("value", byte);
    REQUIRE_FALSE(result);
    REQUIRE(result.error().Code() == woki::ErrorCode::FileAccessDenied);
    REQUIRE_FALSE(fs::exists(root / "real-data" / "value"));
}

TEST_CASE("C ABI maps permissions, invalid pointers, limits, and missing values") {
    using namespace woki::ext::host::cabi;
    const fs::path root = TempRoot("cabi_status_matrix");
    auto denied = MakeHost(root, {});
    std::array<char, 512> text{};
    woki::u32 length = 0;

    REQUIRE(Log(denied, 1, "x", 1) == kDenied);
    REQUIRE(PathData(denied, text.data(), static_cast<woki::u32>(text.size())) == kDenied);
    REQUIRE(FileRead(denied, "x", nullptr, &length) == kDenied);
    REQUIRE(ConfigGet(denied, "x", text.data(), static_cast<woki::u32>(text.size())) == kDenied);

    auto host = MakeHost(root, {Permission::Log, Permission::Paths, Permission::Storage, Permission::Config});
    REQUIRE(Log(host, 1, nullptr, 1) == kInvalid);
    REQUIRE(Log(host, 1, "x", static_cast<woki::u32>(woki::ext::limits::kMaxLogBytes + 1)) == kNoSpace);
    REQUIRE(PathData(host, nullptr, 10) == kInvalid);
    REQUIRE(PathData(host, text.data(), 1) == kNoSpace);
    REQUIRE(FileRead(host, "x", nullptr, nullptr) == kInvalid);
    REQUIRE(FileWrite(host, "x", nullptr, 1) == kInvalid);
    REQUIRE(FileAppend(host, "x", nullptr, 1) == kInvalid);
    REQUIRE(ConfigSet(host, nullptr, "x", 1) == kInvalid);
    REQUIRE(ConfigSet(host, "key", nullptr, 1) == kInvalid);
    REQUIRE(ConfigGet(host, "missing", text.data(), static_cast<woki::u32>(text.size())) == kNotFound);
    REQUIRE(FileRead(host, "../escape", nullptr, &length) == kInvalid);
}

TEST_CASE("C ABI supports exact buffers, size discovery, length-delimited paths, and append") {
    using namespace woki::ext::host::cabi;
    const fs::path root = TempRoot("cabi_round_trip");
    auto host = MakeHost(root, {Permission::Paths, Permission::Storage, Permission::Config});
    const std::array<woki::u8, 3> first{'a', 'b', 'c'};
    const std::array<woki::u8, 2> second{'d', 'e'};

    REQUIRE(FileWrite(host, "state.bin.trailing", 9, first.data(), static_cast<woki::u32>(first.size())) == kOk);
    REQUIRE(FileAppend(host, "state.bin", second.data(), static_cast<woki::u32>(second.size())) == kOk);

    woki::u32 length = 0;
    REQUIRE(FileRead(host, "state.bin", nullptr, &length) == kNoSpace);
    REQUIRE(length == 5);
    std::array<woki::u8, 5> output{};
    length = static_cast<woki::u32>(output.size());
    REQUIRE(FileRead(host, "state.bin", output.data(), &length) == kOk);
    REQUIRE(std::string_view(reinterpret_cast<const char*>(output.data()), output.size()) == "abcde");

    REQUIRE(ConfigSet(host, "theme", "dark", 4) == kOk);
    std::array<char, 5> config{};
    REQUIRE(ConfigGet(host, "theme", config.data(), static_cast<woki::u32>(config.size())) == kOk);
    REQUIRE(std::string_view(config.data()) == "dark");
    REQUIRE(ConfigGet(host, "theme", config.data(), 4) == kNoSpace);

    std::array<char, 1024> path{};
    REQUIRE(PathCache(host, path.data(), static_cast<woki::u32>(path.size())) == kOk);
    REQUIRE(std::string_view(path.data()) == (root / "cache").string());
}

namespace {

class RecordingEventBus final : public woki::ext::host::EventBus {
public:
    void Publish(const woki::ext::host::Event& event) override {
        events.push_back(event);
    }

    std::vector<woki::ext::host::Event> events;
};

} // namespace

TEST_CASE("HostApi records session subscriptions and queues validated guest events") {
    using namespace woki::ext::host;
    auto session = std::make_shared<EventSession>();
    auto service = std::make_shared<EventService>();
    RecordingEventBus bus;
    service->SetBus(&bus);
    HostApi host({"woki.test", {Permission::Events}, {}, {}, {}, session, service});

    REQUIRE(host.SubscribeEvent(17));
    REQUIRE(host.SubscribeEvent(17));
    REQUIRE(session->IsSubscribed(17));
    REQUIRE_FALSE(session->IsSubscribed(18));
    REQUIRE(host.SubscribeEvent(kWildcardEventType));
    REQUIRE(session->IsSubscribed(18));

    const std::array<woki::u8, 2> payload{4, 5};
    REQUIRE_FALSE(host.EmitEvent(17, payload));
    REQUIRE_FALSE(host.EmitEvent(kWildcardEventType, payload));
    REQUIRE(host.EmitEvent(woki::ext::ExtensionEventId(17), payload));
    REQUIRE(bus.events.empty());
    service->Drain();
    REQUIRE(bus.events.size() == 1);
    REQUIRE(bus.events[0].type == woki::ext::ExtensionEventId(17));
    REQUIRE(bus.events[0].payload == std::vector<woki::u8>{4, 5});
    REQUIRE(bus.events[0].origin.kind == EventOriginKind::Extension);
    REQUIRE(bus.events[0].origin.extension_id == "woki.test");

    HostApi denied({"woki.denied", {}, {}, {}, {}, std::make_shared<EventSession>(), service});
    REQUIRE(cabi::EventSubscribe(denied, 1) == cabi::kDenied);
    REQUIRE(cabi::EventEmit(denied, kExtensionEventNamespace | 1, nullptr, 0) == cabi::kDenied);
}

TEST_CASE("named events preserve exact topics validate ownership and queue in order") {
    using namespace woki::ext::host;
    auto session = std::make_shared<EventSession>();
    auto service = std::make_shared<EventService>();
    RecordingEventBus bus;
    service->SetBus(&bus);
    HostApi host({"org.example.tool", {Permission::Events}, {}, {}, {}, session, service});

    REQUIRE(cabi::EventSubscribeNamed(host, "org.example.first", 17) == cabi::kOk);
    REQUIRE(cabi::EventSubscribeNamed(host, "org.example.second", 18) == cabi::kOk);
    CHECK(session->IsSubscribed("org.example.first"));
    CHECK(session->IsSubscribed("org.example.second"));
    CHECK_FALSE(session->IsSubscribed("org.example.third"));

    const std::array<woki::u8, 1> first{1};
    const std::array<woki::u8, 1> second{2};
    REQUIRE(host.EmitNamedEvent("org.example.tool.first", first));
    REQUIRE(host.EmitEvent(woki::ext::ExtensionEventId(9), second));
    REQUIRE(host.EmitNamedEvent("org.example.tool.second", second));
    CHECK_FALSE(host.EmitNamedEvent("org.example.other.event", first));
    CHECK(cabi::EventEmitNamed(host, "Org.example.tool.bad", 20, nullptr, 0) == cabi::kInvalid);
    const char embedded_nul[] = {'o', 'r', 'g', '.', 'e', 'x', '\0', 'x'};
    CHECK(cabi::EventSubscribeNamed(host, embedded_nul, sizeof(embedded_nul)) == cabi::kInvalid);

    service->Drain();
    REQUIRE(bus.events.size() == 3);
    CHECK(bus.events[0].topic == "org.example.tool.first");
    CHECK_FALSE(bus.events[1].topic.has_value());
    CHECK(bus.events[2].topic == "org.example.tool.second");
    CHECK(bus.events[0].origin.extension_id == "org.example.tool");
}
