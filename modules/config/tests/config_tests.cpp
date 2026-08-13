#include <catch2/catch_test_macros.hpp>

#include <woki/config.hpp>

TEST_CASE("config JSONC preserves source metadata and exact numbers") {
    auto document = woki::config::Document::Parse(R"({"$schema":"test",/*c*/"n":18446744073709551615,})", "test.jsonc", woki::config::ParsePolicy::Authored());
    REQUIRE(document);
    woki::config::ObjectView root(document->Root());
    REQUIRE(root.Unsigned("n") == std::numeric_limits<woki::u64>::max());
    REQUIRE(root.Find("n")->Numeric()->lexeme == "18446744073709551615");
    REQUIRE(root.Find("n")->Pointer() == "/n");
}

TEST_CASE("config rejects duplicate decoded keys") {
    auto document = woki::config::Document::Parse(R"({"a":1,"\u0061":2})");
    REQUIRE_FALSE(document);
    REQUIRE(document.error().front().code == "CFG1019");
}

TEST_CASE("restricted YAML produces the shared DOM") {
    auto document = woki::config::Document::ParseYaml("window:\n  width: 1920\n  title: test\n");
    REQUIRE(document);
    const auto window = woki::config::ObjectView(document->Root()).Object("window");
    REQUIRE(window);
    REQUIRE(window->Unsigned("width") == 1920);
    REQUIRE(window->String("title") == "test");
}
