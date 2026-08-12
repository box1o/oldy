#include <catch2/catch_test_macros.hpp>

#include <woki/gfx.hpp>

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
