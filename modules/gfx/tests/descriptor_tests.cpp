#include <catch2/catch_test_macros.hpp>

#include <woki/gfx.hpp>

TEST_CASE("JSONC shader descriptors parse all declared domains") {
    const auto path = *woki::asset::AssetPath::Parse("shaders/example.woki-shader");
    const auto result = woki::gfx::ParseShaderDescriptor(path, R"({
        // Descriptor comments are permitted.
        "schema": 1,
        "name": "example",
        "language": "wgsl",
        "sources": ["shaders/example.wgsl"],
        "entry_points": [{"stage": "vertex", "name": "vs"}, {"stage": "fragment", "name": "fs"}],
        "permutations": [{"name": "quality", "values": [0, 1]}],
        "capabilities": ["shader-f16"],
        "compile_options": {"warnings_as_errors": true, "emit_source_map": false}
    })");
    CHECK(result.diagnostics.empty());
    CHECK(result.descriptor.sources.size() == 1);
    CHECK(result.descriptor.entry_points.size() == 2);
    CHECK(result.descriptor.permutations.size() == 1);
    CHECK(result.descriptor.compile_options.warnings_as_errors);
}

TEST_CASE("Shader descriptors reject traversal and duplicate entries") {
    const auto path = *woki::asset::AssetPath::Parse("example.woki-shader");
    const auto result = woki::gfx::ParseShaderDescriptor(path,
        R"({"schema":1,"name":"bad","language":"wgsl","sources":["../bad.wgsl"],"entry_points":[{"stage":"vertex","name":"main"},{"stage":"vertex","name":"main"}]})");
    REQUIRE(result.diagnostics.size() == 2);
    CHECK(result.diagnostics[0].code == "SHD1008");
    CHECK(result.diagnostics[1].code == "SHD1012");
}

TEST_CASE("Shader descriptors reject unsupported keys and duplicate permutation values") {
    const auto path = *woki::asset::AssetPath::Parse("example.woki-shader");
    const auto result = woki::gfx::ParseShaderDescriptor(path,
        R"({"schema":1,"name":"bad","language":"wgsl","sources":["bad.wgsl"],"entry_points":[{"stage":"vertex","name":"main"}],"permutations":[{"name":"mode","values":[1,1]}],"unknown":true})");
    REQUIRE(result.diagnostics.size() == 2);
    CHECK(result.diagnostics[0].code == "SHD1020");
    CHECK(result.diagnostics[1].code == "SHD1025");
}
