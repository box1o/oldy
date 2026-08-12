#include <catch2/catch_test_macros.hpp>

#include <woki/gfx.hpp>

TEST_CASE("Layout keys normalize binding order and stage visibility") {
    woki::gfx::ShaderInterface first;
    first.bindings = {{.group = 2, .binding = 3, .stages = 1, .kind = woki::gfx::ResourceKind::UniformBuffer}, {.group = 0, .binding = 1, .stages = 2, .kind = woki::gfx::ResourceKind::Sampler}};
    auto second = first;
    std::ranges::reverse(second.bindings);
    const auto first_key = woki::gfx::MakePipelineLayoutKey(first);
    const auto second_key = woki::gfx::MakePipelineLayoutKey(second);
    REQUIRE(first_key);
    REQUIRE(second_key);
    CHECK(first_key->hash == second_key->hash);
    CHECK(first_key->groups.size() == 3);
}

TEST_CASE("Layout policy rejects groups beyond object scope") {
    woki::gfx::ShaderInterface interface;
    interface.bindings.push_back({.group = 4});
    CHECK_FALSE(woki::gfx::MakePipelineLayoutKey(interface));
}
