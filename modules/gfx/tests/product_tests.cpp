#include <catch2/catch_test_macros.hpp>

#include <woki/gfx/advanced.hpp>

TEST_CASE("Shader payloads are deterministic and bounds checked") {
    woki::gfx::ShaderPayload payload;
    payload.code = "@vertex fn main() -> @builtin(position) vec4f { return vec4f(); }";
    payload.interface.entry_points.push_back({.name = "main", .stage = woki::gfx::ShaderStage::Vertex, .inputs = {}, .outputs = {}, .workgroup_size = {1, 1, 1}});
    woki::gfx::NormalizeInterface(payload.interface);
    payload.module_hash = woki::Sha256(payload.code);
    payload.interface_hash = payload.interface.hash;
    payload.variant_hash = woki::Sha256("default");
    payload.dependencies.push_back(*woki::asset::AssetPath::Parse("main.wgsl"));
    payload.source_map.push_back({0, payload.code.size(), payload.dependencies[0], 0, payload.code.size()});
    const auto first = woki::gfx::SerializeShaderPayload(payload);
    const auto second = woki::gfx::SerializeShaderPayload(payload);
    REQUIRE(first);
    CHECK(*first == *second);
    const auto parsed = woki::gfx::ParseShaderPayload(*first);
    REQUIRE(parsed);
    CHECK(parsed->interface.hash == payload.interface.hash);
    auto truncated = *first;
    truncated.pop_back();
    CHECK_FALSE(woki::gfx::ParseShaderPayload(truncated));

    payload.interface.bindings.push_back({.kind = woki::gfx::ResourceKind::ExternalTexture});
    woki::gfx::NormalizeInterface(payload.interface);
    payload.interface_hash = payload.interface.hash;
    CHECK_FALSE(woki::gfx::SerializeShaderPayload(payload));
}

TEST_CASE("Texture descriptors and mip products use strict bounded formats") {
    constexpr std::string_view source = R"({
        // Authored texture metadata is JSONC.
        "schema": 1,
        "asset_id": "22d236a4-8408-56be-8e66-8a52dfe79fd7",
        "source": "project://textures/checker.png",
        "semantic": "color",
        "color_space": "srgb",
        "mips": "generate"
    })";
    const auto descriptor = woki::gfx::ParseTextureSource(source);
    REQUIRE(descriptor);
    CHECK(descriptor->semantic == woki::gfx::TextureSemantic::Color);
    CHECK_FALSE(woki::gfx::ParseTextureSource(R"({"schema":1,"asset_id":"22d236a4-8408-56be-8e66-8a52dfe79fd7","source":"project://a.png","extra":true})"));

    woki::gfx::TextureMetadata metadata{.format_class = woki::gfx::TextureFormatClass::Uncompressed,
        .format = woki::gfx::PortableTextureFormat::Rgba8Srgb,
        .semantic = woki::gfx::TextureSemantic::Color,
        .color_space = woki::gfx::TextureColorSpace::Srgb,
        .dimension = woki::gfx::TextureShape::e2D,
        .streaming = woki::gfx::TextureStreamingPolicy::StreamMips,
        .width = 2,
        .height = 2,
        .layers = 1,
        .faces = 1,
        .mip_count = 2,
        .sampler = {}};
    const std::vector<std::vector<std::byte>> mips{{16, std::byte{0x7f}}, {4, std::byte{0x3f}}};
    const auto bytes = woki::gfx::SerializeTextureProduct(metadata, mips, {});
    REQUIRE(bytes);
    const auto parsed = woki::gfx::ParseTextureProduct(*bytes);
    REQUIRE(parsed);
    CHECK(std::ranges::equal(parsed->Mip(1), mips[1]));
    const auto header = woki::gfx::ParseTextureProductHeader(std::span<const std::byte>(*bytes).first(parsed->mips.front().offset));
    REQUIRE(header);
    CHECK(header->mips == parsed->mips);
    CHECK(header->Mip(0).empty());
    auto corrupt = *bytes;
    corrupt.back() ^= std::byte{1};
    CHECK_FALSE(woki::gfx::ParseTextureProduct(corrupt));
}

TEST_CASE("Material products retain pass programs and reject malformed tails") {
    using namespace woki;
    constexpr std::string_view source = R"({
      "schema": 1,
      "id": "398db147-4060-5923-bf8c-891905d4e26d",
      "name": "Smoke",
      "shader": "18388743-afdf-5e65-aa2b-dd2ea73eabe7",
      "product_family": "smoke",
      "passes": ["forward"],
      "entry_points": { "forward": { "vertex": "vs", "fragment": "fs" } },
      "properties": [{ "name": "color", "type": "vec4", "default": [1, 1, 1, 1] }],
      "textures": [{ "name": "albedo", "type": "texture2d", "binding": 1, "semantic": "color", "default": "38dd26a9-cbc0-5891-9b82-9acf223ce533" }],
      "samplers": [{ "name": "linear_sampler", "binding": 2 }],
      "overrides": []
    })";
    auto authored = gfx::ParseMaterialType(source);
    REQUIRE(authored);
    gfx::ShaderPayload shader;
    gfx::EntryPointInfo vertex;
    vertex.name = "vs";
    vertex.stage = gfx::ShaderStage::Vertex;
    gfx::EntryPointInfo fragment;
    fragment.name = "fs";
    fragment.stage = gfx::ShaderStage::Fragment;
    shader.interface.entry_points = {vertex, fragment};
    gfx::BindingInfo parameters;
    parameters.group = gfx::kMaterialGroup;
    parameters.binding = 0;
    parameters.stages = 3;
    parameters.min_binding_size = 16;
    gfx::BindingInfo texture;
    texture.group = gfx::kMaterialGroup;
    texture.binding = 1;
    texture.stages = 2;
    texture.kind = gfx::ResourceKind::SampledTexture;
    texture.dimension = gfx::TextureDimension::D2;
    texture.sample_type = gfx::SampleType::Float;
    gfx::BindingInfo sampler;
    sampler.group = gfx::kMaterialGroup;
    sampler.binding = 2;
    sampler.stages = 2;
    sampler.kind = gfx::ResourceKind::Sampler;
    shader.interface.bindings = {parameters, texture, sampler};
    gfx::NormalizeInterface(shader.interface);
    shader.interface_hash = shader.interface.hash;
    shader.variant_hash = Sha256("default-variant");
    const auto compiled = gfx::CompileMaterialType(*authored, shader, Sha256("shader-product"));
    const std::string diagnostic = compiled.diagnostics.empty() ? "" : compiled.diagnostics.front().message;
    INFO(diagnostic);
    REQUIRE(compiled.definition);
    REQUIRE(compiled.definition->programs.size() == 1);
    CHECK(compiled.definition->programs[0].vertex_entry == "vs");
    auto bytes = gfx::SerializeMaterialDefinition(*compiled.definition);
    REQUIRE(bytes);
    auto restored = gfx::ParseMaterialDefinition(*bytes);
    REQUIRE(restored);
    CHECK(restored->programs == compiled.definition->programs);
    bytes->push_back(std::byte{});
    CHECK_FALSE(gfx::ParseMaterialDefinition(*bytes));
}
