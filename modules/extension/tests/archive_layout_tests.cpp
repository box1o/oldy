#include <catch2/catch_test_macros.hpp>

#include <woki/ext/package.hpp>

TEST_CASE("Extension archive layout allowlist") {
    using woki::ext::IsAllowedArchiveEntry;

    REQUIRE(IsAllowedArchiveEntry(std::filesystem::path{"manifest.yaml"}, "extension.wasm"));
    REQUIRE(IsAllowedArchiveEntry(std::filesystem::path{"extension.wasm"}, "extension.wasm"));
    REQUIRE(IsAllowedArchiveEntry(std::filesystem::path{"assets/icon.png"}, "extension.wasm"));
    REQUIRE_FALSE(IsAllowedArchiveEntry(std::filesystem::path{"extension.native/foo"}, "extension.wasm"));
    REQUIRE_FALSE(IsAllowedArchiveEntry(std::filesystem::path{"signature"}, "extension.wasm"));
    REQUIRE_FALSE(IsAllowedArchiveEntry(std::filesystem::path{"assets/CON"}, "extension.wasm"));
    REQUIRE_FALSE(IsAllowedArchiveEntry(std::filesystem::path{"../evil"}, "extension.wasm"));
    REQUIRE_FALSE(IsAllowedArchiveEntry(std::filesystem::path{"plugin.cpp"}, "extension.wasm"));
}
