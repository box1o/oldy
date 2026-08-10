#include <fstream>
#include <filesystem>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/limits.hpp>
#include <woki/ext/host/api.hpp>

TEST_CASE("Host config writes atomically within the separate config root") {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "woki_host_storage_tests";
    fs::remove_all(root);
    woki::ext::host::HostApi host({"woki.test", {woki::ext::Permission::Config}, root / "data", root / "config", root / "cache", {}, {}});

    REQUIRE(host.WriteConfig("theme", "light").has_value());
    REQUIRE(host.WriteConfig("theme", "dark").has_value());
    REQUIRE(*host.ReadConfig("theme") == "dark");
    REQUIRE(fs::is_regular_file(root / "config" / "theme"));
    REQUIRE_FALSE(fs::exists(root / "data"));
    REQUIRE_FALSE(fs::exists(root / "cache"));
    for (const fs::directory_entry& entry : fs::directory_iterator(root / "config")) {
        REQUIRE(entry.path().filename() == "theme");
    }
}

TEST_CASE("Host config rejection does not replace an existing value") {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "woki_host_storage_limit_tests";
    fs::remove_all(root);
    woki::ext::host::HostApi host({"woki.test", {woki::ext::Permission::Config}, root / "data", root / "config", root / "cache", {}, {}});

    REQUIRE(host.WriteConfig("theme", "stable").has_value());
    const std::string oversized(woki::ext::limits::kMaxConfigValueBytes + 1, 'x');
    REQUIRE_FALSE(host.WriteConfig("theme", oversized).has_value());
    REQUIRE(*host.ReadConfig("theme") == "stable");
}
