#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include <woki/config.hpp>

using namespace woki;
using namespace woki::config;

TEST_CASE("schema validation covers types bounds uniqueness required and unknown keys") {
    Schema root;
    root.type = SchemaType::Object;
    root.required = {"name", "values"};
    root.additional_properties = false;
    root.properties["name"] = std::make_shared<Schema>(Schema{.type = SchemaType::String, .min_length = 2, .max_length = 4});
    auto item = std::make_shared<Schema>();
    item->type = SchemaType::Integer;
    item->minimum = 1;
    item->maximum = 3;
    auto values = std::make_shared<Schema>();
    values->type = SchemaType::Array;
    values->items = item;
    values->min_items = 2;
    values->max_items = 3;
    values->unique_items = true;
    root.properties["values"] = values;

    const auto good = Document::Parse(R"({"name":"ok","values":[1,2]})");
    REQUIRE(good);
    REQUIRE(Validate(*good, root).empty());

    const auto bad = Document::Parse(R"({"name":"x","values":[0,0,4,5],"extra":true})", "schema.json");
    REQUIRE(bad);
    const auto diagnostics = Validate(*bad, root);
    REQUIRE(diagnostics.size() >= 5);
    REQUIRE(std::ranges::all_of(diagnostics, [](const Diagnostic& diagnostic) { return diagnostic.source == "schema.json"; }));
}

TEST_CASE("every builtin schema is registered serializable and validates a minimal document") {
    constexpr std::array families{
        std::pair{"studio.settings", 1U},
        std::pair{"extension.manifest", 1U},
        std::pair{"gfx.shader", 1U},
        std::pair{"gfx.shader", 2U},
        std::pair{"gpu-struct", 1U},
        std::pair{"shader-pack", 1U},
        std::pair{"pipeline", 1U},
        std::pair{"feature", 1U},
        std::pair{"quality", 1U},
        std::pair{"material-type", 1U},
        std::pair{"material", 1U},
        std::pair{"texture", 1U},
        std::pair{"mesh-import", 1U},
        std::pair{"cook-manifest", 1U},
        std::pair{"ui.theme", 1U},
        std::pair{"ui.dock", 1U},
        std::pair{"ui.debug-tree", 1U},
    };
    Registry registry;
    RegisterBuiltinSchemas(registry);
    const auto minimal = Document::Parse(R"({"$schema":"https://schemas.woki.dev/test"})");
    REQUIRE(minimal);
    for (const auto& [family, version] : families) {
        CAPTURE(family, version);
        const auto* schema = registry.Find(family, version);
        REQUIRE(schema);
        REQUIRE(Validate(*minimal, *schema).empty());
        const auto json_schema = registry.JsonSchema(family, version);
        REQUIRE_FALSE(json_schema.empty());
        REQUIRE(Document::Parse(json_schema));
    }
    REQUIRE_FALSE(registry.Catalog().empty());
    REQUIRE(Document::Parse(registry.Catalog()));
    REQUIRE(registry.Current("gfx.shader") == 2);
    REQUIRE(registry.Maintained("gfx.shader", 1));
    REQUIRE_FALSE(registry.Maintained("gfx.shader", 99));
}

TEST_CASE("migration follows registered paths and canonical hashing ignores presentation") {
    Registry registry;
    RegisterBuiltinSchemas(registry);
    const auto old = Document::Parse(R"({"$schema":"old","schema":1,"name":"main","sources":["a"]})", "shader.json");
    REQUIRE(old);
    const auto migrated = registry.Migrate(*old, "gfx.shader", 1, 2);
    REQUIRE(migrated);
    const ObjectView root(migrated->Root());
    REQUIRE(root.Unsigned("schema") == 2);
    REQUIRE(root.String("$schema") == "https://schemas.woki.dev/gfx.shader/v2.schema.json");
    REQUIRE(Validate(*migrated, *registry.Find("gfx.shader", 2)).empty());
    REQUIRE_FALSE(registry.Migrate(*old, "gfx.shader", 2, 99));

    const auto first = Document::Parse("{\"b\":2,\"a\":1.0}");
    const auto second = Document::Parse("{ \"a\" : 1, \"b\" : 2 }");
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(CanonicalJson(first->Root()) == R"({"a":1,"b":2})");
    REQUIRE(CanonicalHash(*first, "family", 1) == CanonicalHash(*second, "family", 1));
    REQUIRE(first->ExactSourceHash() != second->ExactSourceHash());
    REQUIRE(CanonicalHash(*first, "family", 1) != CanonicalHash(*first, "family", 2));
}

TEST_CASE("all authored repository configs parse and validate against registered schemas") {
    const auto source = std::filesystem::path(WOKI_SOURCE_DIR);
    const std::array roots{source / "assets", source / "config", source / "extensions"};
    auto& registry = Registry::Global();
    std::size_t validated = 0;
    constexpr std::array authored_extensions{
        std::string_view{".json"},
        std::string_view{".jsonc"},
        std::string_view{".yaml"},
        std::string_view{".yml"},
        std::string_view{".woki-shader"},
        std::string_view{".woki-gpu-struct"},
        std::string_view{".woki-shader-pack"},
        std::string_view{".woki-pipeline"},
        std::string_view{".woki-feature"},
        std::string_view{".woki-quality"},
        std::string_view{".woki-material-type"},
        std::string_view{".woki-material"},
        std::string_view{".woki-texture"},
        std::string_view{".woki-mesh-import"},
        std::string_view{".woki-theme"},
    };
    for (const auto& root : roots) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file())
                continue;
            const auto extension = entry.path().extension().string();
            if (std::ranges::find(authored_extensions, extension) == authored_extensions.end())
                continue;
            std::ifstream input(entry.path(), std::ios::binary);
            const std::string text((std::istreambuf_iterator<char>(input)), {});
            constexpr std::string_view prefix = "https://schemas.woki.dev/";
            const auto schema = text.find(prefix);
            if (schema == std::string::npos)
                continue;
            const auto family_begin = schema + prefix.size();
            const auto version_marker = text.find("/v", family_begin);
            const auto version_end = text.find(".schema.json", version_marker);
            REQUIRE(version_marker != std::string::npos);
            REQUIRE(version_end != std::string::npos);
            const std::string family = text.substr(family_begin, version_marker - family_begin);
            const auto version_text = std::string_view(text).substr(version_marker + 2, version_end - version_marker - 2);
            u32 version{};
            const auto converted = std::from_chars(version_text.data(), version_text.data() + version_text.size(), version);
            REQUIRE(converted.ec == std::errc{});
            CAPTURE(entry.path(), family, version);
            const bool yaml = entry.path().extension() == ".yaml" || entry.path().extension() == ".yml";
            auto document = yaml ? Document::ParseYaml(text, entry.path().string()) : Document::Parse(text, entry.path().string(), ParsePolicy::Authored());
            REQUIRE(document);
            const auto* registered = registry.Find(family, version);
            REQUIRE(registered);
            REQUIRE(Validate(*document, *registered).empty());
            ++validated;
        }
    }
    REQUIRE(validated >= 30);
}
