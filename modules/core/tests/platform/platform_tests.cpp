#include <catch2/catch_test_macros.hpp>

#include <woki/core.hpp>

TEST_CASE("PlatformId has stable descriptive values") {
    const auto platform = woki::CurrentPlatformId();
    REQUIRE(platform.os != woki::OperatingSystem::Unknown);
    REQUIRE(platform.architecture != woki::Architecture::Unknown);
    REQUIRE_FALSE(platform.ToString().empty());
}

TEST_CASE("Platform string helpers are not empty") {
    REQUIRE(!std::string_view(woki::ToString(woki::CurrentCompiler())).empty());
    REQUIRE(!std::string_view(woki::ToString(woki::CurrentArchitecture())).empty());
    REQUIRE(!std::string_view(woki::ToString(woki::CurrentOperatingSystem())).empty());
}
