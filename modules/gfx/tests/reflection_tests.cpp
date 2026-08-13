#include <catch2/catch_test_macros.hpp>

#include <woki/gfx/advanced.hpp>

TEST_CASE("Tint reflection normalizes a WGSL interface snapshot") {
    if (!woki::gfx::IsTintShaderCompilerAvailable()) {
        SUCCEED("Tint is an optional host-tool dependency in this build");
        return;
    }
    woki::gfx::ShaderDescriptor descriptor;
    descriptor.name = "snapshot";
    descriptor.entry_points = {{woki::gfx::ShaderStage::Vertex, "vs"}, {woki::gfx::ShaderStage::Fragment, "fs"}};
    woki::gfx::ComposedSource source;
    source.code = R"(
        struct Frame { value: vec4f }
        @group(0) @binding(0) var<uniform> frame: Frame;
        @group(2) @binding(1) var color_texture: texture_2d<f32>;
        @group(2) @binding(2) var color_sampler: sampler;
        override exposure: f32 = 1.0;
        struct Out { @builtin(position) position: vec4f, @location(0) uv: vec2f }
        @vertex fn vs(@location(0) position: vec3f) -> Out { var out: Out; out.position = vec4f(position, 1); out.uv = frame.value.xy; return out; }
        @fragment fn fs(input: Out) -> @location(0) vec4f { return textureSample(color_texture, color_sampler, input.uv) * exposure; }
    )";
    auto output = woki::gfx::CreateTintShaderCompiler()->Compile({descriptor, source});
    REQUIRE(output.validated_with_tint);
    CHECK(output.interface.entry_points.size() == 2);
    CHECK(output.interface.bindings.size() == 3);
    CHECK(output.interface.overrides.size() == 1);
    CHECK(output.interface.hash != woki::ContentHash{});
}

TEST_CASE("Tint reflection honors selected entries and rejects unsupported layouts and fake permutations") {
    if (!woki::gfx::IsTintShaderCompilerAvailable())
        return;
    woki::gfx::ShaderDescriptor descriptor;
    descriptor.name = "selection";
    descriptor.entry_points = {{woki::gfx::ShaderStage::Vertex, "selected"}};
    descriptor.permutations = {{"mode", {woki::i64{0}, woki::i64{1}}}};
    woki::gfx::ComposedSource source;
    source.code = R"(
        override mode: i32 = 0;
        @vertex fn selected() -> @builtin(position) vec4f { return vec4f(f32(mode)); }
        @fragment fn unselected() -> @location(0) vec4f { return vec4f(); }
    )";
    auto output = woki::gfx::CreateTintShaderCompiler()->Compile({descriptor, source});
    REQUIRE(output.validated_with_tint);
    REQUIRE(output.interface.entry_points.size() == 1);
    CHECK(output.interface.entry_points.front().name == "selected");
    CHECK(output.interface.bindings.empty());

    descriptor.permutations = {{"source_macro", {true, false}}};
    output = woki::gfx::CreateTintShaderCompiler()->Compile({descriptor, source});
    CHECK(std::ranges::any_of(output.diagnostics, [](const auto& diagnostic) { return diagnostic.code == "SHD3008"; }));

    descriptor.permutations.clear();
    descriptor.entry_points = {{woki::gfx::ShaderStage::Fragment, "external_entry"}};
    source.code = R"(
        @group(0) @binding(0) var external_texture_value: texture_external;
        @group(0) @binding(1) var external_sampler: sampler;
        @fragment fn external_entry() -> @location(0) vec4f { return textureSampleBaseClampToEdge(external_texture_value, external_sampler, vec2f()); }
    )";
    output = woki::gfx::CreateTintShaderCompiler()->Compile({descriptor, source});
    const std::string external_diagnostic = output.diagnostics.empty() ? "no diagnostic" : output.diagnostics.front().message;
    INFO(external_diagnostic);
    CHECK(std::ranges::any_of(output.diagnostics, [](const auto& diagnostic) { return diagnostic.code == "SHD3007"; }));
}

TEST_CASE("Tint diagnostics map generated ranges back to source modules") {
    if (!woki::gfx::IsTintShaderCompilerAvailable())
        return;
    woki::gfx::ShaderDescriptor descriptor;
    descriptor.name = "diagnostic";
    descriptor.entry_points = {{woki::gfx::ShaderStage::Vertex, "main"}};
    woki::gfx::ComposedSource source;
    source.code = "@vertex fn main( {\n";
    const auto path = *woki::asset::AssetPath::Parse("shaders/broken.wgsl");
    source.source_map.push_back({0, source.code.size(), path, 20, 20 + source.code.size()});
    const auto output = woki::gfx::CreateTintShaderCompiler()->Compile({descriptor, source});
    REQUIRE_FALSE(output.diagnostics.empty());
    REQUIRE(output.diagnostics.front().range.path);
    CHECK(*output.diagnostics.front().range.path == path);
    CHECK(output.diagnostics.front().range.byte_offset >= 20);
}
