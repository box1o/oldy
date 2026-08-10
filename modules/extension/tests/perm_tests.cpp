#include <catch2/catch_test_macros.hpp>

#include <woki/ext/perm.hpp>

TEST_CASE("Extension permissions parse known names") {
    auto permission = woki::ext::ParsePermission("log");
    REQUIRE(permission.has_value());
    REQUIRE(*permission == woki::ext::Permission::Log);
    REQUIRE(woki::ext::ToString(*permission) == "log");
}

TEST_CASE("Extension permissions reject unknown names") {
    auto permission = woki::ext::ParsePermission("network");
    REQUIRE_FALSE(permission.has_value());
    REQUIRE(permission.error().Code() == woki::ErrorCode::ValidationInvalidState);
}
