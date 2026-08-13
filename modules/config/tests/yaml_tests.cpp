#include <array>

#include <catch2/catch_test_macros.hpp>

#include <woki/config.hpp>

using namespace woki::config;

TEST_CASE("restricted YAML maps supported scalars and source metadata into the shared DOM") {
    const auto document = Document::ParseYaml("name: test\nenabled: true\ncount: 3\nratio: 1.5\nnone: null\nitems:\n  - one\n", "test.yaml");
    REQUIRE(document);
    const ObjectView root(document->Root());
    REQUIRE(root.String("name") == "test");
    REQUIRE(root.Boolean("enabled") == true);
    REQUIRE(root.Unsigned("count") == 3);
    REQUIRE(root.Real("ratio") == 1.5);
    REQUIRE(root.Find("none")->Type() == Value::Kind::Null);
    REQUIRE(root.Array("items")->At(0)->Pointer() == "/items/0");
    REQUIRE(root.FindMember("enabled")->key_range.begin.line == 2);
    REQUIRE(document->Source() == "test.yaml");
}

TEST_CASE("restricted YAML rejects aliases anchors tags merge keys and duplicate keys") {
    constexpr std::array forbidden{
        "base: &base value\ncopy: *base\n",
        "value: !custom tagged\n",
        "base: {}\nobject:\n  <<: base\n",
    };
    for (const auto text : forbidden) {
        const auto parsed = Document::ParseYaml(text);
        REQUIRE_FALSE(parsed);
        REQUIRE(parsed.error().front().code == "CFG5002");
    }
    const auto duplicate = Document::ParseYaml("key: one\nkey: two\n");
    REQUIRE_FALSE(duplicate);
    REQUIRE(duplicate.error().front().code == "CFG5006");
    REQUIRE(duplicate.error().front().notes.size() == 1);
}

TEST_CASE("restricted YAML enforces byte structural member and scalar limits") {
    Limits limits;
    limits.bytes = 2;
    REQUIRE_FALSE(Document::ParseYaml("key: value", {}, limits));
    limits = {};
    limits.depth = 1;
    REQUIRE_FALSE(Document::ParseYaml("a:\n  b:\n    c: 1\n", {}, limits));
    limits = {};
    limits.nodes = 2;
    REQUIRE_FALSE(Document::ParseYaml("a: 1\nb: 2\n", {}, limits));
    limits = {};
    limits.members = 1;
    REQUIRE_FALSE(Document::ParseYaml("a: 1\nb: 2\n", {}, limits));
    limits = {};
    limits.string_bytes = 2;
    REQUIRE_FALSE(Document::ParseYaml("value: long\n", {}, limits));
}
