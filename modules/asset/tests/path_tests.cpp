#include <catch2/catch_test_macros.hpp>

#include <woki/asset.hpp>

TEST_CASE("Asset paths normalize safe lexical components") {
    const auto path = woki::asset::AssetPath::Parse("shaders//./lit.wgsl");

    REQUIRE(path);
    REQUIRE(path->String() == "shaders/lit.wgsl");
}

TEST_CASE("Asset paths reject unsafe forms") {
    REQUIRE_FALSE(woki::asset::AssetPath::Parse(""));
    REQUIRE_FALSE(woki::asset::AssetPath::Parse("/etc/passwd"));
    REQUIRE_FALSE(woki::asset::AssetPath::Parse("../secret"));
    REQUIRE_FALSE(woki::asset::AssetPath::Parse("shaders/../secret"));
    REQUIRE_FALSE(woki::asset::AssetPath::Parse("shaders\\secret"));
    REQUIRE_FALSE(woki::asset::AssetPath::Parse("C:/secret"));
}
