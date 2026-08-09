#include <string>
#include <fstream>
#include <filesystem>
#include <string_view>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/ext.hpp>

namespace {

namespace fs = std::filesystem;

[[nodiscard]] fs::path MakeTempDir(std::string_view name) {
    const fs::path root = fs::temp_directory_path() / "woki_extension_host_tests" / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

[[nodiscard]] woki::ext::Record MakeRecord(const fs::path& root) {
    woki::ext::Record record;
    record.id = "woki.hello";
    record.manifest.id = "woki.hello";
    record.manifest.name = "Hello";
    record.manifest.version = "0.1.0";
    record.manifest.permissions = {
        woki::ext::Permission::Log,
        woki::ext::Permission::Paths,
        woki::ext::Permission::Storage,
    };
    record.package.data_root = root / "data";
    record.package.cache_root = root / "cache";
    return record;
}

} // namespace

TEST_CASE("Extension host api resolves declared paths") {
    auto record = MakeRecord(MakeTempDir("paths"));
    const woki::ext::host::HostApi host(record);

    auto data = host.DataPath();
    REQUIRE(data.has_value());
    REQUIRE(*data == record.package.data_root);

    auto cache = host.CachePath();
    REQUIRE(cache.has_value());
    REQUIRE(*cache == record.package.cache_root);
}

TEST_CASE("Extension host api rejects undeclared permissions") {
    auto record = MakeRecord(MakeTempDir("denied"));
    record.manifest.permissions = {woki::ext::Permission::Log};
    const woki::ext::host::HostApi host(record);

    auto data = host.DataPath();
    REQUIRE_FALSE(data.has_value());
    REQUIRE(data.error().Code() == woki::ErrorCode::FileAccessDenied);
    REQUIRE(data.error().Message().contains("paths"));
}

TEST_CASE("Extension host api sandboxes storage paths") {
    auto record = MakeRecord(MakeTempDir("storage"));
    const woki::ext::host::HostApi host(record);

    const std::array<woki::u8, 3> bytes{1, 2, 3};
    auto written = host.WriteFile("nested/file.bin", bytes);
    REQUIRE(written.has_value());

    auto read = host.ReadFile("nested/file.bin");
    REQUIRE(read.has_value());
    REQUIRE(read->size() == bytes.size());
    REQUIRE((*read)[0] == 1);
    REQUIRE((*read)[1] == 2);
    REQUIRE((*read)[2] == 3);

    auto escaped = host.ReadFile("../outside.bin");
    REQUIRE_FALSE(escaped.has_value());
    REQUIRE(escaped.error().Code() == woki::ErrorCode::InvalidArgument);
}

TEST_CASE("Extension host api appends storage files") {
    auto record = MakeRecord(MakeTempDir("append"));
    const woki::ext::host::HostApi host(record);

    const std::array<woki::u8, 3> first{'o', 'n', 'e'};
    const std::array<woki::u8, 3> second{'t', 'w', 'o'};

    REQUIRE(host.AppendFile("events.log", first).has_value());
    REQUIRE(host.AppendFile("events.log", second).has_value());

    auto read = host.ReadFile("events.log");
    REQUIRE(read.has_value());
    REQUIRE(read->size() == 6);
    REQUIRE(std::string_view(reinterpret_cast<const char*>(read->data()), read->size()) == "onetwo");
}

TEST_CASE("Extension host api requires config permission for reads and writes") {
    auto record = MakeRecord(MakeTempDir("config_denied"));
    const woki::ext::host::HostApi host(record);

    auto written = host.WriteConfig("theme", "dark");
    REQUIRE_FALSE(written.has_value());
    REQUIRE(written.error().Code() == woki::ErrorCode::FileAccessDenied);
    REQUIRE(written.error().Message().contains("config"));
    REQUIRE_FALSE(fs::exists(record.package.data_root / "config" / "theme"));

    auto read = host.ReadConfig("theme");
    REQUIRE_FALSE(read.has_value());
    REQUIRE(read.error().Code() == woki::ErrorCode::FileAccessDenied);
    REQUIRE(read.error().Message().contains("config"));
}

TEST_CASE("Extension host api round trips config at key and value limits") {
    auto record = MakeRecord(MakeTempDir("config_limits"));
    record.manifest.permissions.push_back(woki::ext::Permission::Config);
    const woki::ext::host::HostApi host(record);
    const std::string key(woki::ext::limits::kMaxConfigKeyBytes, 'k');
    const std::string value(woki::ext::limits::kMaxConfigValueBytes, 'v');

    REQUIRE(host.WriteConfig(key, value).has_value());
    auto read = host.ReadConfig(key);
    REQUIRE(read.has_value());
    REQUIRE(*read == value);
}

TEST_CASE("Extension host api rejects invalid config keys") {
    auto record = MakeRecord(MakeTempDir("config_keys"));
    record.manifest.permissions.push_back(woki::ext::Permission::Config);
    const woki::ext::host::HostApi host(record);

    for (const std::string& key : {std::string{}, std::string{"nested/key"}, std::string{"."}, std::string{".."}, std::string(woki::ext::limits::kMaxConfigKeyBytes + 1, 'k')}) {
        auto written = host.WriteConfig(key, "value");
        REQUIRE_FALSE(written.has_value());
        REQUIRE(written.error().Code() == woki::ErrorCode::InvalidArgument);
    }
    REQUIRE_FALSE(fs::exists(record.package.data_root / "config"));
}

TEST_CASE("Extension host api rejects storage symlink escapes") {
    const fs::path root = MakeTempDir("storage_symlink");
    auto record = MakeRecord(root);
    fs::create_directories(record.package.data_root);
    std::error_code error;
    fs::create_directory_symlink(root, record.package.data_root / "escape", error);
    if (error) {
        SKIP("Directory symlinks are unavailable");
    }
    const woki::ext::host::HostApi host(record);

    const std::array<woki::u8, 1> byte{1};
    auto written = host.WriteFile("escape/outside.bin", byte);
    REQUIRE_FALSE(written.has_value());
    REQUIRE(written.error().Code() == woki::ErrorCode::FileAccessDenied);
    REQUIRE_FALSE(fs::exists(root / "outside.bin"));
}

TEST_CASE("Extension host api caps the resulting append file size") {
    auto record = MakeRecord(MakeTempDir("append_total_limit"));
    fs::create_directories(record.package.data_root);
    std::ofstream(record.package.data_root / "full.bin", std::ios::binary);
    fs::resize_file(record.package.data_root / "full.bin", woki::ext::limits::kMaxFileBytes);
    const woki::ext::host::HostApi host(record);

    const std::array<woki::u8, 1> byte{1};
    auto appended = host.AppendFile("full.bin", byte);
    REQUIRE_FALSE(appended.has_value());
    REQUIRE(appended.error().Code() == woki::ErrorCode::ValidationOutOfRange);
    REQUIRE(fs::file_size(record.package.data_root / "full.bin") == woki::ext::limits::kMaxFileBytes);
}

TEST_CASE("Extension host api rejects oversized config values without replacing existing data") {
    auto record = MakeRecord(MakeTempDir("config_value"));
    record.manifest.permissions.push_back(woki::ext::Permission::Config);
    const woki::ext::host::HostApi host(record);

    REQUIRE(host.WriteConfig("theme", "dark").has_value());
    auto oversized = host.WriteConfig("theme", std::string(woki::ext::limits::kMaxConfigValueBytes + 1, 'v'));
    REQUIRE_FALSE(oversized.has_value());
    REQUIRE(oversized.error().Code() == woki::ErrorCode::ValidationOutOfRange);

    auto read = host.ReadConfig("theme");
    REQUIRE(read.has_value());
    REQUIRE(*read == "dark");
}
