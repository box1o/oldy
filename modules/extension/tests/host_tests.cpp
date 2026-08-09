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
    REQUIRE(escaped.error().Code() == woki::ErrorCode::ValidationInvalidState);
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
