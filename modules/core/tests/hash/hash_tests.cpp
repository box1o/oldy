#include <catch2/catch_test_macros.hpp>

#include <woki/core.hpp>

TEST_CASE("HashString is stable for identical values") {
    const auto a = woki::HashString("hello");
    const auto b = woki::HashString("hello");
    const auto c = woki::HashString("world");

    REQUIRE(a == b);
    REQUIRE(a != c);
}

TEST_CASE("HashCombine updates seed") {
    std::size_t seed = 0;
    woki::HashCombine(seed, 42);
    REQUIRE(seed != 0);
}

TEST_CASE("StringId hashes strings consistently") {
    const woki::StringId a("player");
    const woki::StringId b("player");
    const woki::StringId c("enemy");

    REQUIRE(a == b);
    REQUIRE(a != c);
    REQUIRE_FALSE(a.Empty());
}

TEST_CASE("SHA-256 matches known vectors") {
    REQUIRE(woki::Sha256("").Hex() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    REQUIRE(woki::Sha256("abc").Hex() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    REQUIRE(woki::Sha256("The quick brown fox jumps over the lazy dog").Hex() == "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
}

TEST_CASE("Content hashes parse canonical and uppercase hex") {
    const auto hash = woki::Sha256("shader");
    const auto parsed = woki::ContentHash::Parse(hash.Hex());
    const auto uppercase = woki::ContentHash::Parse("E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855");

    REQUIRE(parsed);
    REQUIRE(*parsed == hash);
    REQUIRE(uppercase);
    REQUIRE(*uppercase == woki::Sha256(""));
    REQUIRE_FALSE(woki::ContentHash::Parse("abcd"));
    REQUIRE_FALSE(woki::ContentHash::Parse(std::string(64, 'z')));
}
