#include <array>
#include <limits>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include <woki/config.hpp>

using namespace woki;
using namespace woki::config;

namespace {
bool HasCode(const std::vector<Diagnostic>& diagnostics, const std::string_view code) {
    return std::ranges::any_of(diagnostics, [&](const Diagnostic& diagnostic) { return diagnostic.code == code; });
}
} // namespace

TEST_CASE("JSONC policy distinguishes comments trailing commas and required schemas") {
    constexpr std::string_view text = "{\n  // note\n  \"$schema\": \"test\",\n  \"value\": 7,\n}";
    REQUIRE_FALSE(Document::Parse(text, "policy.json", ParsePolicy::Strict()));
    const auto authored = Document::Parse(text, "policy.json", ParsePolicy::Authored());
    REQUIRE(authored);
    REQUIRE(ObjectView(authored->Root()).Unsigned("value") == 7);
    REQUIRE_FALSE(Document::Parse(R"({"value":7})", "missing.json", ParsePolicy::Authored()));
}

TEST_CASE("duplicate decoded keys report both precise source locations") {
    const auto document = Document::Parse("{\n  \"a\": 1,\n  \"\\u0061\": 2\n}", "duplicate.json");
    REQUIRE_FALSE(document);
    REQUIRE(document.error().size() == 1);
    const auto& diagnostic = document.error().front();
    REQUIRE(diagnostic.code == "CFG1019");
    REQUIRE(diagnostic.source == "duplicate.json");
    REQUIRE(diagnostic.pointer == "/a");
    REQUIRE(diagnostic.range.begin.line == 3);
    REQUIRE(diagnostic.range.begin.column == 3);
    REQUIRE(diagnostic.notes.size() == 1);
    REQUIRE(diagnostic.notes[0].range->begin.line == 2);
}

TEST_CASE("UTF-8 and surrogate handling reject malformed scalar values") {
    const auto valid = Document::Parse(R"({"text":"caf\u00e9 \ud83d\ude00"})");
    REQUIRE(valid);
    REQUIRE(ObjectView(valid->Root()).String("text") == "caf\xC3\xA9 \xF0\x9F\x98\x80");

    const std::string invalid_source = std::string("{\"x\":\"") + static_cast<char>(0xC0) + static_cast<char>(0x80) + "\"}";
    const auto malformed = Document::Parse(invalid_source);
    REQUIRE_FALSE(malformed);
    REQUIRE(HasCode(malformed.error(), "CFG1002"));
    REQUIRE_FALSE(Document::Parse(R"({"x":"\ud800"})"));
    REQUIRE_FALSE(Document::Parse(R"({"x":"\udc00"})"));
}

TEST_CASE("value ranges line columns and escaped pointers are retained") {
    constexpr std::string_view text = "{\n  \"a/b~c\": [\n    true\n  ]\n}";
    const auto document = Document::Parse(text, "ranges.json");
    REQUIRE(document);
    const auto* value = ObjectView(document->Root()).Find("a/b~c");
    REQUIRE(value);
    REQUIRE(value->Pointer() == "/a~1b~0c");
    REQUIRE(value->Range().begin.line == 2);
    REQUIRE(value->Range().begin.column == 12);
    REQUIRE(ArrayView(*value).At(0)->Range().begin.line == 3);
    REQUIRE(document->Source() == "ranges.json");
    REQUIRE(document->Text() == text);
}

TEST_CASE("all JSON parser limits fail independently") {
    auto policy = ParsePolicy::Strict();
    policy.limits.bytes = 1;
    REQUIRE(HasCode(Document::Parse("null", {}, policy).error(), "CFG1001"));

    policy = ParsePolicy::Strict();
    policy.limits.depth = 1;
    REQUIRE(HasCode(Document::Parse("[[0]]", {}, policy).error(), "CFG1006"));

    policy = ParsePolicy::Strict();
    policy.limits.nodes = 2;
    REQUIRE(HasCode(Document::Parse("[0,1]", {}, policy).error(), "CFG1007"));

    policy = ParsePolicy::Strict();
    policy.limits.members = 1;
    REQUIRE(HasCode(Document::Parse(R"({"a":0,"b":1})", {}, policy).error(), "CFG1007"));

    policy = ParsePolicy::Strict();
    policy.limits.string_bytes = 2;
    REQUIRE(HasCode(Document::Parse(R"("\u0061\u0062\u0063")", {}, policy).error(), "CFG1013"));
}

TEST_CASE("typed views do not perform narrowing or type coercion") {
    const auto document = Document::Parse(R"({"negative":-1,"positive":2,"huge":18446744073709551615,"real":1.5,"flag":true,"text":"x","array":[1],"object":{}})");
    REQUIRE(document);
    const ObjectView root(document->Root());
    REQUIRE(root.Integer("negative") == -1);
    REQUIRE(root.Integer("positive") == 2);
    REQUIRE_FALSE(root.Integer("huge"));
    REQUIRE_FALSE(root.Unsigned("negative"));
    REQUIRE(root.Unsigned("huge") == std::numeric_limits<u64>::max());
    REQUIRE(root.Real("positive") == 2.0);
    REQUIRE(root.Real("real") == 1.5);
    REQUIRE(root.Boolean("flag") == true);
    REQUIRE(root.String("text") == "x");
    REQUIRE(root.Array("array")->Size() == 1);
    REQUIRE(root.Object("object")->Valid());
    REQUIRE_FALSE(root.String("flag"));
    REQUIRE(ArrayView(document->Root()).At(0) == nullptr);
}
