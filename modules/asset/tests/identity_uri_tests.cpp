#include <array>
#include <unordered_set>

#include <catch2/catch_test_macros.hpp>

#include <woki/asset.hpp>

using namespace woki;
using namespace woki::asset;

TEST_CASE("AssetId parses canonical UUIDs and preserves UUID version semantics") {
    const auto parsed = AssetId::Parse("123e4567-e89b-42d3-a456-426614174000");
    REQUIRE(parsed);
    REQUIRE(parsed->String() == "123e4567-e89b-42d3-a456-426614174000");
    REQUIRE_FALSE(AssetId::Parse("123E4567-E89B-42D3-A456-426614174000"));
    REQUIRE_FALSE(AssetId::Parse("00000000-0000-0000-0000-000000000000"));
    REQUIRE_FALSE(AssetId::Parse("short"));

    const auto named = AssetId::FromName("project://textures/wood");
    REQUIRE(named == AssetId::FromName("project://textures/wood"));
    REQUIRE(named != AssetId::FromName("project://textures/metal"));
    REQUIRE((named.Bytes()[6] >> 4U) == 5U);
    REQUIRE((named.Bytes()[8] & 0xC0U) == 0x80U);
    const auto reparsed = AssetId::Parse(named.String());
    REQUIRE(reparsed);
    REQUIRE(*reparsed == named);
    REQUIRE(std::unordered_set<AssetId>{named}.contains(named));
}

TEST_CASE("AssetUri accepts only canonical scheme authority and path forms") {
    const auto engine = AssetUri::Parse("engine://shaders/lit.wgsl");
    const auto plugin = AssetUri::Parse("plugin://org.example/textures/a.png");
    REQUIRE(engine);
    REQUIRE(plugin);
    REQUIRE(engine->Scheme() == AssetScheme::Engine);
    REQUIRE(engine->Authority().empty());
    REQUIRE(plugin->Scheme() == AssetScheme::Plugin);
    REQUIRE(plugin->Authority() == "org.example");
    REQUIRE(plugin->Path().String() == "textures/a.png");

    constexpr std::array invalid{
        "http://example/a",
        "plugin://missing-path",
        "plugin://../a",
        "engine:///absolute",
        "engine://a/../b",
        "engine://a%2fb",
        "Engine://a",
        "engine://a\\b",
    };
    for (const auto uri : invalid) {
        CAPTURE(uri);
        REQUIRE_FALSE(AssetUri::Parse(uri));
    }
}
